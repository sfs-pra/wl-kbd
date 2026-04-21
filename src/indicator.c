#include "indicator.h"

#include "config.h"
#include "icon.h"
#include "layout_code.h"
#include "shortcut_hint.h"
#include "wl_keyboard.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <sni-exporter.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct _Indicator {
    Config       cfg;
    GMainLoop   *loop;           /* borrowed */
    WlKeyboard  *kbd;
    SniExporter *tray;
    char        *argv0;
    char         layout_code[16];  /* current active layout code, e.g. "us" */
    guint        pending_update;   /* g_idle source id; 0 if none */
};

static void indicator_update_icon(Indicator *ind);
static void indicator_rebuild_menu(Indicator *ind);
static void on_action(const char *action_id, void *user_data);

static void
restart_self(Indicator *ind, TrayStyle style)
{
    const char *flag = style == TRAY_STYLE_LETTERS ? "-l" : "-f";

    if (!ind->argv0 || !ind->argv0[0]) {
        g_warning("wl-kbd: cannot restart without argv[0]");
        return;
    }

    execlp(ind->argv0, ind->argv0, flag, NULL);
    g_warning("wl-kbd: exec restart failed for %s", ind->argv0);
}

/* Deferred update: runs on the next GLib main-loop iteration so the D-Bus
 * Event reply reaches sfwbar before we send LayoutUpdated / NewMenu. */
static gboolean
on_idle_update(gpointer user_data)
{
    Indicator *ind = user_data;
    ind->pending_update = 0;

    indicator_update_icon(ind);
    indicator_rebuild_menu(ind);
    return G_SOURCE_REMOVE;
}

static void
schedule_update(Indicator *ind)
{
    if (ind->pending_update != 0)
        g_source_remove(ind->pending_update);
    ind->pending_update = g_idle_add(on_idle_update, ind);
}

static void
on_layout_changed(const char *layout_code, guint group, gpointer user_data)
{
    Indicator *ind = (Indicator *)user_data;
    (void)group;

    if (!ind) {
        return;
    }

    if (!layout_code || !layout_code[0]) {
        layout_code = "us";
    }

    strncpy(ind->layout_code, layout_code, sizeof(ind->layout_code) - 1);
    ind->layout_code[sizeof(ind->layout_code) - 1] = '\0';

    /* Layout changes don't come from a D-Bus handler, so update directly. */
    indicator_update_icon(ind);
}

static void
on_action(const char *action_id, void *user_data)
{
    Indicator *ind = (Indicator *)user_data;

    if (!ind || !action_id) {
        return;
    }

    if (g_strcmp0(action_id, "toggle_style") == 0) {
        ind->cfg.tray_style =
            (ind->cfg.tray_style == TRAY_STYLE_FLAG) ? TRAY_STYLE_LETTERS : TRAY_STYLE_FLAG;
        config_save(&ind->cfg);
        restart_self(ind, ind->cfg.tray_style);
        return;
    }

    if (g_strcmp0(action_id, "toggle_theme") == 0) {
        ind->cfg.letter_theme =
            (ind->cfg.letter_theme == ICON_THEME_LIGHT) ? ICON_THEME_DARK : ICON_THEME_LIGHT;
        config_save(&ind->cfg);
        schedule_update(ind);
        return;
    }

    if (g_strcmp0(action_id, "kbd_settings") == 0) {
        GError *err = NULL;
        if (!g_spawn_command_line_async("wl-kbd-config", &err)) {
            g_warning("wl-kbd: cannot launch wl-kbd-config: %s", err->message);
            g_error_free(err);
        }
        return;
    }

    if (g_strcmp0(action_id, "quit") == 0) {
        g_main_loop_quit(ind->loop);
    }
}

static void
indicator_update_icon(Indicator *ind)
{
    char text[16];
    const char *layout_code;
    char *layouts;
    char *shortcut;
    char *tooltip_body;

    if (!ind || !ind->tray) {
        return;
    }

    layout_code = ind->layout_code[0] ? ind->layout_code : "us";

    if (ind->cfg.tray_style == TRAY_STYLE_FLAG) {
        char icon_name[64];
        sni_exporter_set_icon_theme_path(ind->tray, "/usr/share/icons");
        sni_exporter_set_icon_argb(ind->tray, 0, 0, NULL, 0);
        g_snprintf(icon_name, sizeof(icon_name), "wl-kbd-layout-%s", layout_to_icon_code(layout_code));
        sni_exporter_set_icon_name(ind->tray, icon_name);
    } else {
        guint8 *argb;
        int icon_size = ind->cfg.letter_icon_size;
        if (icon_size < 16) icon_size = 16;
        if (icon_size > 64) icon_size = 64;

        layout_to_display_text(layout_code, text, sizeof(text));
        sni_exporter_set_icon_name(ind->tray, "");
        argb = icon_render_letter(text, ind->cfg.letter_theme, icon_size);
        sni_exporter_set_icon_argb(ind->tray,
                                   icon_size,
                                   icon_size,
                                   argb,
                                   (gsize)icon_size * (gsize)icon_size * 4);
        g_free(argb);
    }

    layouts = wl_keyboard_format_layouts(ind->kbd);
    shortcut = shortcut_hint_detect();
    if (shortcut == NULL || shortcut[0] == '\0') {
        g_free(shortcut);
        shortcut = g_strdup(_("unknown"));
    }

    tooltip_body = g_strdup_printf(_("Layouts: %s\nSwitch shortcut: %s"), layouts, shortcut);
    sni_exporter_set_tooltip(ind->tray, "", tooltip_body);

    g_free(tooltip_body);
    g_free(shortcut);
    g_free(layouts);
}

static void
indicator_rebuild_menu(Indicator *ind)
{
    if (!ind || !ind->tray) {
        return;
    }

    sni_exporter_menu_begin(ind->tray);

    if (ind->cfg.tray_style == TRAY_STYLE_FLAG) {
        sni_exporter_menu_add_action_item(ind->tray, "toggle_style",
                                          _("Letter icons"),
                                          "format-text-bold");
    } else {
        sni_exporter_menu_add_action_item(ind->tray, "toggle_style",
                                          _("Flag icons"),
                                          "preferences-desktop-locale");
    }

    if (ind->cfg.tray_style == TRAY_STYLE_LETTERS) {
        if (ind->cfg.letter_theme == ICON_THEME_LIGHT) {
            sni_exporter_menu_add_action_item(ind->tray, "toggle_theme",
                                              _("Light icon"),
                                              "weather-clear");
        } else {
            sni_exporter_menu_add_action_item(ind->tray, "toggle_theme",
                                              _("Dark icon"),
                                              "weather-clear-night");
        }
    }

    sni_exporter_menu_add_separator(ind->tray);

    sni_exporter_menu_add_action_item(ind->tray, "kbd_settings", _("Keyboard Settings"), "input-keyboard");
    sni_exporter_menu_add_action_item(ind->tray, "quit", _("Quit"), "application-exit");

    sni_exporter_menu_end(ind->tray);
}

Indicator *
indicator_new(Config cfg, GMainLoop *loop, const char *argv0)
{
    Indicator *ind;
    GError *err = NULL;

    ind = g_new0(Indicator, 1);
    ind->cfg = cfg;
    ind->loop = loop;
    ind->argv0 = g_strdup(argv0 != NULL ? argv0 : "wl-kbd");
    strncpy(ind->layout_code, "us", sizeof(ind->layout_code) - 1);
    ind->layout_code[sizeof(ind->layout_code) - 1] = '\0';

    ind->tray = sni_exporter_new("wl-kbd");
    if (!ind->tray) {
        g_free(ind);
        return NULL;
    }

    sni_exporter_set_action_callback(ind->tray, on_action, ind, NULL);
    sni_exporter_set_title(ind->tray, "wl-kbd");

    ind->kbd = wl_keyboard_new(on_layout_changed, ind, NULL);
    if (!ind->kbd) {
        g_warning("wl-kbd: could not initialize Wayland keyboard, using fallback layout icon");
    } else {
        const char *current = wl_keyboard_get_layout_code(ind->kbd);
        if (current && current[0]) {
            strncpy(ind->layout_code, current, sizeof(ind->layout_code) - 1);
            ind->layout_code[sizeof(ind->layout_code) - 1] = '\0';
        }
    }

    indicator_update_icon(ind);
    indicator_rebuild_menu(ind);

    if (!sni_exporter_start(ind->tray, &err)) {
        g_warning("wl-kbd: failed to start tray indicator: %s", err ? err->message : "unknown error");
        if (err) {
            g_error_free(err);
        }
        indicator_free(ind);
        return NULL;
    }

    return ind;
}

void
indicator_free(Indicator *ind)
{
    if (!ind) {
        return;
    }

    if (ind->pending_update != 0) {
        g_source_remove(ind->pending_update);
        ind->pending_update = 0;
    }
    if (ind->kbd) {
        wl_keyboard_free(ind->kbd);
    }
    if (ind->tray) {
        sni_exporter_free(ind->tray);
    }
    g_free(ind->argv0);
    g_free(ind);
}
