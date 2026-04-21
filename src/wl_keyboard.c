#include "wl_keyboard.h"
#include "layout_code.h"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include <glib.h>
#include <glib-unix.h>

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

struct _WlKeyboard {
    struct wl_display  *display;
    struct wl_registry *registry;
    struct wl_seat     *seat;
    struct wl_keyboard *keyboard;

    struct xkb_context *xkb_ctx;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;

    /* layout_codes: array of lowercase codes from XKB_DEFAULT_LAYOUT (e.g. ["us","ru"]) */
    char             **layout_codes;  /* g_strdup'd strings, NULL-terminated */
    int                num_layouts;
    int                current_group;
    char              *current_layout;

    LayoutChangedCb  cb;
    gpointer         user_data;
    GDestroyNotify   notify;

    guint            source_id;   /* GLib fd source for Wayland events */
};

static gboolean on_wl_io(gint fd, GIOCondition cond, gpointer data);
static void wl_keyboard_notify_layout(WlKeyboard *self);

static void keyboard_keymap(void *data,
                            struct wl_keyboard *keyboard,
                            guint32 format,
                            int fd,
                            guint32 size);
static void keyboard_enter(void *data,
                           struct wl_keyboard *keyboard,
                           guint32 serial,
                           struct wl_surface *surface,
                           struct wl_array *keys);
static void keyboard_leave(void *data,
                           struct wl_keyboard *keyboard,
                           guint32 serial,
                           struct wl_surface *surface);
static void keyboard_key(void *data,
                         struct wl_keyboard *keyboard,
                         guint32 serial,
                         guint32 time,
                         guint32 key,
                         guint32 state);
static void keyboard_modifiers(void *data,
                               struct wl_keyboard *keyboard,
                               guint32 serial,
                               guint32 mods_depressed,
                               guint32 mods_latched,
                               guint32 mods_locked,
                               guint32 group);
static void keyboard_repeat_info(void *data,
                                 struct wl_keyboard *keyboard,
                                 gint32 rate,
                                 gint32 delay);

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void seat_capabilities(void *data, struct wl_seat *seat, guint32 capabilities);
static void seat_name(void *data, struct wl_seat *seat, const char *name);

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void registry_global(void *data,
                            struct wl_registry *registry,
                            guint32 name,
                            const char *interface,
                            guint32 version);
static void registry_global_remove(void *data, struct wl_registry *registry, guint32 name);

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static char *normalize_layout_token(const char *token)
{
    char *tmp;
    char *paren;
    char *lower;

    if (token == NULL) {
        return NULL;
    }

    tmp = g_strdup(token);
    g_strstrip(tmp);
    if (*tmp == '\0') {
        g_free(tmp);
        return NULL;
    }

    paren = strchr(tmp, '(');
    if (paren != NULL) {
        *paren = '\0';
        g_strstrip(tmp);
    }

    if (*tmp == '\0') {
        g_free(tmp);
        return NULL;
    }

    lower = g_ascii_strdown(tmp, -1);
    g_free(tmp);
    return lower;
}

static void parse_layout_codes(WlKeyboard *self)
{
    const char *env_layouts;
    gchar **parts;
    GPtrArray *arr;
    int i;

    env_layouts = g_getenv("XKB_DEFAULT_LAYOUT");
    parts = g_strsplit(env_layouts != NULL ? env_layouts : "", ",", -1);
    arr = g_ptr_array_new();

    for (i = 0; parts[i] != NULL; i++) {
        char *normalized = normalize_layout_token(parts[i]);
        if (normalized != NULL) {
            g_ptr_array_add(arr, normalized);
        }
    }

    if (arr->len == 0) {
        g_ptr_array_add(arr, g_strdup("us"));
    }

    g_ptr_array_add(arr, NULL);
    self->layout_codes = (char **) g_ptr_array_free(arr, FALSE);
    self->num_layouts = g_strv_length(self->layout_codes);
    g_strfreev(parts);
}

WlKeyboard *wl_keyboard_new(LayoutChangedCb cb, gpointer user_data, GDestroyNotify notify)
{
    WlKeyboard *self;

    self = g_new0(WlKeyboard, 1);
    self->cb = cb;
    self->user_data = user_data;
    self->notify = notify;

    self->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    parse_layout_codes(self);
    self->current_group = 0;

    self->display = wl_display_connect(NULL);
    if (self->display == NULL) {
        g_warning("Failed to connect to Wayland display");
        wl_keyboard_free(self);
        return NULL;
    }

    self->registry = wl_display_get_registry(self->display);
    if (self->registry == NULL) {
        g_warning("Failed to get Wayland registry");
        wl_keyboard_free(self);
        return NULL;
    }

    wl_registry_add_listener(self->registry, &registry_listener, self);
    wl_display_roundtrip(self->display);

    if (self->seat == NULL) {
        g_warning("No Wayland seat available");
        wl_keyboard_free(self);
        return NULL;
    }

    wl_seat_add_listener(self->seat, &seat_listener, self);
    wl_display_roundtrip(self->display);

    if (self->keyboard == NULL) {
        g_warning("No Wayland keyboard available on seat");
        wl_keyboard_free(self);
        return NULL;
    }

    self->source_id = g_unix_fd_add(wl_display_get_fd(self->display),
                                    G_IO_IN | G_IO_HUP | G_IO_ERR,
                                    on_wl_io,
                                    self);

    wl_display_flush(self->display);

    if (self->cb != NULL) {
        self->cb(self->layout_codes[0], 0, self->user_data);
    }

    return self;
}

void wl_keyboard_free(WlKeyboard *self)
{
    if (self == NULL) {
        return;
    }

    if (self->source_id != 0) {
        g_source_remove(self->source_id);
    }

    if (self->xkb_state != NULL) {
        xkb_state_unref(self->xkb_state);
    }
    if (self->xkb_keymap != NULL) {
        xkb_keymap_unref(self->xkb_keymap);
    }
    if (self->xkb_ctx != NULL) {
        xkb_context_unref(self->xkb_ctx);
    }

    if (self->keyboard != NULL) {
        wl_keyboard_destroy(self->keyboard);
    }
    if (self->seat != NULL) {
        wl_seat_destroy(self->seat);
    }
    if (self->registry != NULL) {
        wl_registry_destroy(self->registry);
    }
    if (self->display != NULL) {
        wl_display_disconnect(self->display);
    }

    if (self->notify != NULL) {
        self->notify(self->user_data);
    }

    g_strfreev(self->layout_codes);
    g_free(self->current_layout);
    g_free(self);
}

const char *wl_keyboard_get_layout_code(const WlKeyboard *self)
{
    return (self != NULL && self->layout_codes != NULL &&
            self->current_group >= 0 && self->current_group < self->num_layouts)
               ? self->layout_codes[self->current_group]
               : NULL;
}

int wl_keyboard_get_layout_count(const WlKeyboard *self)
{
    return self != NULL ? self->num_layouts : 0;
}

const char *wl_keyboard_get_layout_at(const WlKeyboard *self, int idx)
{
    return (self != NULL && self->layout_codes != NULL && idx >= 0 && idx < self->num_layouts)
               ? self->layout_codes[idx]
               : NULL;
}

char *wl_keyboard_format_layouts(const WlKeyboard *self)
{
    GString *joined;
    int i;

    if (self == NULL || self->layout_codes == NULL || self->num_layouts <= 0) {
        return g_strdup("us");
    }

    joined = g_string_new(NULL);
    for (i = 0; i < self->num_layouts; i++) {
        const char *layout = self->layout_codes[i];

        if (layout == NULL || layout[0] == '\0') {
            continue;
        }

        if (joined->len > 0) {
            g_string_append_c(joined, ',');
        }
        g_string_append(joined, layout);
    }

    if (joined->len == 0) {
        g_string_append(joined, "us");
    }

    return g_string_free(joined, FALSE);
}

static gboolean on_wl_io(gint fd, GIOCondition cond, gpointer data)
{
    WlKeyboard *self = data;
    int ret;
    (void) fd;

    if ((cond & (G_IO_HUP | G_IO_ERR)) != 0) {
        return G_SOURCE_REMOVE;
    }

    if ((cond & G_IO_IN) != 0) {
        ret = wl_display_dispatch(self->display);
        if (ret < 0) {
            return G_SOURCE_REMOVE;
        }
        wl_display_flush(self->display);
    }

    return G_SOURCE_CONTINUE;
}

static void registry_global(void *data,
                            struct wl_registry *registry,
                            guint32 name,
                            const char *interface,
                            guint32 version)
{
    WlKeyboard *self = data;
    guint32 bind_version;

    if (g_strcmp0(interface, wl_seat_interface.name) == 0) {
        if (self->seat == NULL) {
            bind_version = version > 5 ? 5 : version;
            self->seat = wl_registry_bind(registry, name, &wl_seat_interface, bind_version);
        }
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, guint32 name)
{
    (void) data;
    (void) registry;
    (void) name;
}

static void seat_capabilities(void *data, struct wl_seat *seat, guint32 capabilities)
{
    WlKeyboard *self = data;

    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0 && self->keyboard == NULL) {
        self->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(self->keyboard, &keyboard_listener, self);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
    (void) data;
    (void) seat;
    (void) name;
}

static void keyboard_keymap(void *data,
                            struct wl_keyboard *keyboard,
                            guint32 format,
                            int fd,
                            guint32 size)
{
    WlKeyboard *self = data;
    char *map;

    (void) keyboard;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return;
    }

    if (self->xkb_state != NULL) {
        xkb_state_unref(self->xkb_state);
        self->xkb_state = NULL;
    }
    if (self->xkb_keymap != NULL) {
        xkb_keymap_unref(self->xkb_keymap);
        self->xkb_keymap = NULL;
    }

    self->xkb_keymap = xkb_keymap_new_from_string(self->xkb_ctx,
                                                   map,
                                                   XKB_KEYMAP_FORMAT_TEXT_V1,
                                                   XKB_KEYMAP_COMPILE_NO_FLAGS);

    munmap(map, size);
    close(fd);

    if (self->xkb_keymap != NULL) {
        self->xkb_state = xkb_state_new(self->xkb_keymap);
        wl_keyboard_notify_layout(self);
    }
}

static void keyboard_enter(void *data,
                           struct wl_keyboard *keyboard,
                           guint32 serial,
                           struct wl_surface *surface,
                           struct wl_array *keys)
{
    (void) data;
    (void) keyboard;
    (void) serial;
    (void) surface;
    (void) keys;
}

static void keyboard_leave(void *data,
                           struct wl_keyboard *keyboard,
                           guint32 serial,
                           struct wl_surface *surface)
{
    (void) data;
    (void) keyboard;
    (void) serial;
    (void) surface;
}

static void keyboard_key(void *data,
                         struct wl_keyboard *keyboard,
                         guint32 serial,
                         guint32 time,
                         guint32 key,
                         guint32 state)
{
    (void) data;
    (void) keyboard;
    (void) serial;
    (void) time;
    (void) key;
    (void) state;
}

static void keyboard_modifiers(void *data,
                               struct wl_keyboard *keyboard,
                               guint32 serial,
                               guint32 mods_depressed,
                               guint32 mods_latched,
                               guint32 mods_locked,
                               guint32 group)
{
    WlKeyboard *self = data;

    (void) keyboard;
    (void) serial;

    if (self->xkb_state != NULL) {
        xkb_state_update_mask(self->xkb_state,
                              mods_depressed,
                              mods_latched,
                              mods_locked,
                              0,
                              0,
                              group);
    }

    (void) group;
    wl_keyboard_notify_layout(self);
}

static void keyboard_repeat_info(void *data,
                                 struct wl_keyboard *keyboard,
                                 gint32 rate,
                                 gint32 delay)
{
    (void) data;
    (void) keyboard;
    (void) rate;
    (void) delay;
}

static void wl_keyboard_notify_layout(WlKeyboard *self)
{
    xkb_layout_index_t idx;
    const char *layout_name;

    if (self == NULL || self->xkb_state == NULL || self->xkb_keymap == NULL)
        return;

    idx = xkb_state_serialize_layout(self->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
    if (idx >= xkb_keymap_num_layouts(self->xkb_keymap))
        return;

    layout_name = xkb_keymap_layout_get_name(self->xkb_keymap, idx);
    if (layout_name == NULL || layout_name[0] == '\0')
        return;

    if (g_strcmp0(self->current_layout, layout_name) == 0)
        return;

    g_free(self->current_layout);
    self->current_layout = g_strdup(layout_name);

    if ((int) idx >= 0 && (int) idx < self->num_layouts)
        self->current_group = (int) idx;
    else
        self->current_group = 0;

    if (self->cb != NULL)
        self->cb(layout_name, (guint) idx, self->user_data);
}
