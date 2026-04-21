#include "config.h"
#include <glib.h>
#include <string.h>

#define CFG_GROUP "Indicator"
#define CFG_DIR   "wl-kbd"
#define CFG_FILE  "indicator.ini"

static int clamp_icon_size(int v)
{
    if (v < 16) return 16;
    if (v > 64) return 64;
    return v;
}

static char *config_path(void)
{
    return g_build_filename(g_get_user_config_dir(), CFG_DIR, CFG_FILE, NULL);
}

Config config_load(void)
{
    Config cfg = {
        .tray_style   = TRAY_STYLE_FLAG,
        .letter_theme = ICON_THEME_LIGHT,
        .letter_icon_size = 24,
    };

    char *path = config_path();
    GKeyFile *kf = g_key_file_new();

    if (!g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL))
        goto done;

    char *s = g_key_file_get_string(kf, CFG_GROUP, "tray_style", NULL);
    if (s) {
        if (strcmp(s, "LETTERS") == 0) cfg.tray_style = TRAY_STYLE_LETTERS;
        g_free(s);
    }

    s = g_key_file_get_string(kf, CFG_GROUP, "letter_theme", NULL);
    if (s) {
        if (strcmp(s, "dark") == 0) cfg.letter_theme = ICON_THEME_DARK;
        g_free(s);
    }

    if (g_key_file_has_key(kf, CFG_GROUP, "letter_icon_size", NULL)) {
        gint sz = g_key_file_get_integer(kf, CFG_GROUP, "letter_icon_size", NULL);
        cfg.letter_icon_size = clamp_icon_size((int)sz);
    }

done:
    g_key_file_free(kf);
    g_free(path);
    return cfg;
}

void config_save(const Config *cfg)
{
    char *path = config_path();
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    GKeyFile *kf = g_key_file_new();
    g_key_file_set_string(kf, CFG_GROUP, "tray_style",
                          cfg->tray_style == TRAY_STYLE_LETTERS ? "LETTERS" : "FLAG");
    g_key_file_set_string(kf, CFG_GROUP, "letter_theme",
                          cfg->letter_theme == ICON_THEME_DARK ? "dark" : "light");
    g_key_file_set_integer(kf, CFG_GROUP, "letter_icon_size",
                           clamp_icon_size(cfg->letter_icon_size));

    GError *err = NULL;
    if (!g_key_file_save_to_file(kf, path, &err)) {
        g_warning("wl-kbd: failed to save config: %s", err->message);
        g_error_free(err);
    }

    g_key_file_free(kf);
    g_free(path);
}
