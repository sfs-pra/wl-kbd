#pragma once
#include <glib.h>

typedef struct _WlKeyboard WlKeyboard;

/*
 * Called whenever the active keyboard layout group changes.
 *
 * layout_code: canonical lowercase XKB layout code for the active group,
 *              e.g. "us", "ru".  Valid only for the duration of the call.
 * group:       active XKB group index (0-based).
 * user_data:   value passed to wl_keyboard_new().
 */
typedef void (*LayoutChangedCb)(const char *layout_code, guint group, gpointer user_data);

/*
 * Connect to the Wayland seat, bind wl_keyboard, parse the XKB keymap, and
 * integrate with the default GLib main context for asynchronous event dispatch.
 *
 * Layout codes come from $XKB_DEFAULT_LAYOUT (comma-separated, e.g. "us,ru").
 * The active group is tracked from wl_keyboard.modifiers events whenever the
 * Wayland display sends them (requires the caller's process to have a focused
 * surface; without one, only the initial group from the env var is reported).
 *
 * Returns NULL if WAYLAND_DISPLAY is not set or the seat has no keyboard.
 */
WlKeyboard *wl_keyboard_new(LayoutChangedCb cb, gpointer user_data, GDestroyNotify notify);

void        wl_keyboard_free(WlKeyboard *self);

/* Current active layout code (e.g. "us"). Valid as long as self is alive. */
const char *wl_keyboard_get_layout_code(const WlKeyboard *self);

/* Number of configured layouts (length of XKB_DEFAULT_LAYOUT list). */
int         wl_keyboard_get_layout_count(const WlKeyboard *self);

/* Layout code at index idx, e.g. "ru". Valid as long as self is alive. */
const char *wl_keyboard_get_layout_at(const WlKeyboard *self, int idx);

/* Comma-separated configured layouts (e.g. "us,ru"). Caller must free. */
char       *wl_keyboard_format_layouts(const WlKeyboard *self);
