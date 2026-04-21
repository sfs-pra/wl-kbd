#pragma once

#include <glib.h>

typedef enum {
    SHORTCUT_HINT_WM_UNKNOWN = 0,
    SHORTCUT_HINT_WM_LABWC,
    SHORTCUT_HINT_WM_SWAY,
    SHORTCUT_HINT_WM_WAYFIRE,
    SHORTCUT_HINT_WM_RIVER,
    SHORTCUT_HINT_WM_HYPRLAND,
} ShortcutHintWM;

ShortcutHintWM shortcut_hint_detect_wm(void);
char *shortcut_hint_normalize(const char *raw);
char *shortcut_hint_parse_for_wm(ShortcutHintWM wm, const char *config_text);
char *shortcut_hint_detect(void);
