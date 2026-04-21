#pragma once
#include <glib.h>

typedef enum {
    TRAY_STYLE_FLAG    = 0,
    TRAY_STYLE_LETTERS = 1,
} TrayStyle;

typedef enum {
    ICON_THEME_LIGHT = 0,   /* white text on dark background (default) */
    ICON_THEME_DARK  = 1,   /* dark text on light background */
} IconTheme;

typedef struct {
    TrayStyle tray_style;
    IconTheme letter_theme;
    int       letter_icon_size;
} Config;

/* Load from ~/.config/wl-kbd/indicator.ini; returns defaults on missing file. */
Config config_load(void);

/* Save to ~/.config/wl-kbd/indicator.ini; creates directory if needed. */
void   config_save(const Config *cfg);
