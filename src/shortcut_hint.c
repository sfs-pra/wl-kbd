#include "shortcut_hint.h"

#include <glib.h>
#include <string.h>

static char *normalize_xkb_options_value(const char *opts);

static char *
read_env_var_from_environ_blob(const char *blob, gsize len, const char *key)
{
    gsize key_len;
    gsize i;

    if (!blob || !key)
        return NULL;

    key_len = strlen(key);
    i = 0;
    while (i < len) {
        const char *entry = blob + i;
        gsize entry_len = strlen(entry);

        if (entry_len == 0) {
            i++;
            continue;
        }

        if (entry_len > key_len + 1 &&
            strncmp(entry, key, key_len) == 0 &&
            entry[key_len] == '=') {
            return g_strdup(entry + key_len + 1);
        }

        i += entry_len + 1;
    }

    return NULL;
}

static char *
xkb_options_from_process_name(const char *process_name)
{
    GDir *dir;
    const char *name;

    if (!process_name || !*process_name)
        return NULL;

    dir = g_dir_open("/proc", 0, NULL);
    if (!dir)
        return NULL;

    while ((name = g_dir_read_name(dir)) != NULL) {
        char *comm_path;
        char *comm = NULL;
        gsize comm_len = 0;
        char *environ_path;
        char *environ_blob = NULL;
        gsize environ_len = 0;
        char *opts;
        char *norm;

        if (!g_ascii_isdigit(name[0]))
            continue;

        comm_path = g_strdup_printf("/proc/%s/comm", name);
        if (!g_file_get_contents(comm_path, &comm, &comm_len, NULL)) {
            g_free(comm_path);
            continue;
        }
        g_free(comm_path);

        g_strchomp(comm);
        if (g_strcmp0(comm, process_name) != 0) {
            g_free(comm);
            continue;
        }
        g_free(comm);

        environ_path = g_strdup_printf("/proc/%s/environ", name);
        if (!g_file_get_contents(environ_path, &environ_blob, &environ_len, NULL)) {
            g_free(environ_path);
            continue;
        }
        g_free(environ_path);

        opts = read_env_var_from_environ_blob(environ_blob, environ_len, "XKB_DEFAULT_OPTIONS");
        g_free(environ_blob);
        if (!opts)
            continue;

        norm = normalize_xkb_options_value(opts);
        g_free(opts);
        if (norm) {
            g_dir_close(dir);
            return norm;
        }
    }

    g_dir_close(dir);
    return NULL;
}

static char *
xkb_options_from_labwc_environment_file(void)
{
    char *path;
    char *content = NULL;
    gsize len = 0;
    char *result = NULL;
    char **lines;

    path = g_build_filename(g_get_home_dir(), ".config", "labwc", "environment", NULL);
    if (!g_file_get_contents(path, &content, &len, NULL)) {
        g_free(path);
        return NULL;
    }
    g_free(path);

    lines = g_strsplit(content, "\n", -1);
    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strdup(lines[i]);
        char *p;

        g_strstrip(line);
        if (!line[0] || line[0] == '#') {
            g_free(line);
            continue;
        }

        if (g_str_has_prefix(line, "export ")) {
            p = line + 7;
        } else {
            p = line;
        }

        if (g_str_has_prefix(p, "XKB_DEFAULT_OPTIONS=")) {
            char *v = g_strdup(p + strlen("XKB_DEFAULT_OPTIONS="));
            g_strstrip(v);

            if (v[0] == '\'' || v[0] == '"') {
                char quote = v[0];
                gsize l = strlen(v);
                if (l >= 2 && v[l - 1] == quote) {
                    memmove(v, v + 1, l - 2);
                    v[l - 2] = '\0';
                }
            }

            result = normalize_xkb_options_value(v);
            g_free(v);
            g_free(line);
            break;
        }

        g_free(line);
    }

    g_strfreev(lines);
    g_free(content);
    return result;
}

static const char *
wm_process_name(ShortcutHintWM wm)
{
    switch (wm) {
    case SHORTCUT_HINT_WM_LABWC:
        return "labwc";
    case SHORTCUT_HINT_WM_SWAY:
        return "sway";
    case SHORTCUT_HINT_WM_WAYFIRE:
        return "wayfire";
    case SHORTCUT_HINT_WM_RIVER:
        return "river";
    case SHORTCUT_HINT_WM_HYPRLAND:
        return "Hyprland";
    default:
        return NULL;
    }
}

static char *
normalize_xkb_options_value(const char *opts)
{
    if (!opts || !*opts)
        return NULL;

    /* XKB_DEFAULT_OPTIONS may contain comma-separated entries, e.g.
     * "grp:alt_shift_toggle,compose:ralt". */
    if (strstr(opts, "grp:alt_shift_toggle"))
        return g_strdup("Alt+Shift");
    if (strstr(opts, "grp:shift_alt_toggle"))
        return g_strdup("Alt+Shift");
    if (strstr(opts, "grp:ctrl_shift_toggle"))
        return g_strdup("Ctrl+Shift");
    if (strstr(opts, "grp:shift_ctrl_toggle"))
        return g_strdup("Ctrl+Shift");
    if (strstr(opts, "grp:win_space_toggle"))
        return g_strdup("Super+Space");
    if (strstr(opts, "grp:lwin_space_toggle"))
        return g_strdup("Super+Space");
    if (strstr(opts, "grp:rwin_space_toggle"))
        return g_strdup("Super+Space");
    if (strstr(opts, "grp:alt_space_toggle"))
        return g_strdup("Alt+Space");
    if (strstr(opts, "grp:caps_toggle"))
        return g_strdup("Caps Lock");
    if (strstr(opts, "grp:menu_toggle"))
        return g_strdup("Menu");

    return NULL;
}

static char *
shortcut_from_xkb_options(ShortcutHintWM wm)
{
    const char *proc_name;
    char *from_labwc_env;
    char *from_proc;
    const char *self_opts;

    if (wm == SHORTCUT_HINT_WM_LABWC) {
        from_labwc_env = xkb_options_from_labwc_environment_file();
        if (from_labwc_env)
            return from_labwc_env;
    }

    proc_name = wm_process_name(wm);
    from_proc = xkb_options_from_process_name(proc_name);
    if (from_proc)
        return from_proc;

    self_opts = g_getenv("XKB_DEFAULT_OPTIONS");
    return normalize_xkb_options_value(self_opts);
}

static gboolean
env_contains_ci(const char *name, const char *needle)
{
    const char *v = g_getenv(name);
    if (!v || !*v)
        return FALSE;

    char *low_v = g_ascii_strdown(v, -1);
    char *low_n = g_ascii_strdown(needle, -1);
    gboolean ok = strstr(low_v, low_n) != NULL;
    g_free(low_v);
    g_free(low_n);
    return ok;
}

static char *
wm_config_path(ShortcutHintWM wm)
{
    switch (wm) {
    case SHORTCUT_HINT_WM_LABWC:
        return g_build_filename(g_get_home_dir(), ".config", "labwc", "rc.xml", NULL);
    case SHORTCUT_HINT_WM_SWAY:
        return g_build_filename(g_get_home_dir(), ".config", "sway", "config", NULL);
    case SHORTCUT_HINT_WM_WAYFIRE:
        return g_build_filename(g_get_home_dir(), ".config", "wayfire.ini", NULL);
    case SHORTCUT_HINT_WM_RIVER:
        return g_build_filename(g_get_home_dir(), ".config", "river", "init", NULL);
    case SHORTCUT_HINT_WM_HYPRLAND:
        return g_build_filename(g_get_home_dir(), ".config", "hypr", "hyprland.conf", NULL);
    case SHORTCUT_HINT_WM_UNKNOWN:
    default:
        return NULL;
    }
}

static gboolean
wm_has_config(ShortcutHintWM wm)
{
    char *path = wm_config_path(wm);
    if (!path)
        return FALSE;
    gboolean ok = g_file_test(path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR);
    g_free(path);
    return ok;
}

ShortcutHintWM
shortcut_hint_detect_wm(void)
{
    if (g_getenv("HYPRLAND_INSTANCE_SIGNATURE"))
        return SHORTCUT_HINT_WM_HYPRLAND;
    if (g_getenv("SWAYSOCK"))
        return SHORTCUT_HINT_WM_SWAY;
    if (g_getenv("WAYFIRE_SOCKET"))
        return SHORTCUT_HINT_WM_WAYFIRE;
    if (g_getenv("RIVER_SOCKET"))
        return SHORTCUT_HINT_WM_RIVER;

    if (env_contains_ci("XDG_CURRENT_DESKTOP", "hyprland") ||
        env_contains_ci("DESKTOP_SESSION", "hyprland"))
        return SHORTCUT_HINT_WM_HYPRLAND;
    if (env_contains_ci("XDG_CURRENT_DESKTOP", "sway") ||
        env_contains_ci("DESKTOP_SESSION", "sway"))
        return SHORTCUT_HINT_WM_SWAY;
    if (env_contains_ci("XDG_CURRENT_DESKTOP", "wayfire") ||
        env_contains_ci("DESKTOP_SESSION", "wayfire"))
        return SHORTCUT_HINT_WM_WAYFIRE;
    if (env_contains_ci("XDG_CURRENT_DESKTOP", "river") ||
        env_contains_ci("DESKTOP_SESSION", "river"))
        return SHORTCUT_HINT_WM_RIVER;
    if (env_contains_ci("XDG_CURRENT_DESKTOP", "labwc") ||
        env_contains_ci("DESKTOP_SESSION", "labwc"))
        return SHORTCUT_HINT_WM_LABWC;

    if (wm_has_config(SHORTCUT_HINT_WM_HYPRLAND))
        return SHORTCUT_HINT_WM_HYPRLAND;
    if (wm_has_config(SHORTCUT_HINT_WM_SWAY))
        return SHORTCUT_HINT_WM_SWAY;
    if (wm_has_config(SHORTCUT_HINT_WM_WAYFIRE))
        return SHORTCUT_HINT_WM_WAYFIRE;
    if (wm_has_config(SHORTCUT_HINT_WM_RIVER))
        return SHORTCUT_HINT_WM_RIVER;
    if (wm_has_config(SHORTCUT_HINT_WM_LABWC))
        return SHORTCUT_HINT_WM_LABWC;

    return SHORTCUT_HINT_WM_UNKNOWN;
}

static const char *
normalize_token(const char *t)
{
    if (g_ascii_strcasecmp(t, "alt") == 0)
        return "Alt";
    if (g_ascii_strcasecmp(t, "shift") == 0)
        return "Shift";
    if (g_ascii_strcasecmp(t, "super") == 0 || g_ascii_strcasecmp(t, "win") == 0)
        return "Super";
    if (g_ascii_strcasecmp(t, "ctrl") == 0 || g_ascii_strcasecmp(t, "control") == 0)
        return "Ctrl";
    if (g_ascii_strcasecmp(t, "space") == 0)
        return "Space";
    if (g_ascii_strcasecmp(t, "capslock") == 0)
        return "Caps Lock";
    if (g_ascii_strcasecmp(t, "return") == 0)
        return "Enter";
    if (g_ascii_strcasecmp(t, "enter") == 0)
        return "Enter";
    if (g_ascii_strcasecmp(t, "esc") == 0 || g_ascii_strcasecmp(t, "escape") == 0)
        return "Esc";
    if (g_ascii_strcasecmp(t, "tab") == 0)
        return "Tab";
    return NULL;
}

char *
shortcut_hint_normalize(const char *raw)
{
    if (!raw)
        return g_strdup("unknown");

    char *trim = g_strdup(raw);
    g_strstrip(trim);
    if (!*trim) {
        g_free(trim);
        return g_strdup("unknown");
    }

    char *caps_probe = g_ascii_strdown(trim, -1);
    GString *caps_compact = g_string_new(NULL);
    for (char *p = caps_probe; *p; p++) {
        if (*p == ' ' || *p == '-' || *p == '_')
            continue;
        g_string_append_c(caps_compact, *p);
    }
    if (g_strcmp0(caps_compact->str, "capslock") == 0) {
        g_string_free(caps_compact, TRUE);
        g_free(caps_probe);
        g_free(trim);
        return g_strdup("Caps Lock");
    }
    g_string_free(caps_compact, TRUE);
    g_free(caps_probe);

    char **parts = g_strsplit_set(trim, " +,;:-_/\\\t\r\n", -1);
    GString *out = g_string_new(NULL);

    for (int i = 0; parts[i] != NULL; i++) {
        if (!parts[i][0])
            continue;

        const char *mapped = normalize_token(parts[i]);
        char *piece = NULL;

        if (mapped) {
            piece = g_strdup(mapped);
        } else {
            piece = g_ascii_strdown(parts[i], -1);
            if (piece[0])
                piece[0] = g_ascii_toupper(piece[0]);
        }

        if (out->len > 0)
            g_string_append_c(out, '+');
        g_string_append(out, piece);
        g_free(piece);
    }

    g_strfreev(parts);
    g_free(trim);

    if (out->len == 0) {
        g_string_free(out, TRUE);
        return g_strdup("unknown");
    }

    return g_string_free(out, FALSE);
}

static char *
parse_sway_like(const char *config_text)
{
    char **lines = g_strsplit(config_text, "\n", -1);
    char *result = NULL;

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strdup(lines[i]);
        g_strstrip(line);

        if (!*line || line[0] == '#') {
            g_free(line);
            continue;
        }

        if (strstr(line, "bindsym") == NULL || strstr(line, "xkb_switch_layout") == NULL) {
            g_free(line);
            continue;
        }

        char *p = strstr(line, "bindsym");
        p += strlen("bindsym");
        while (*p == ' ' || *p == '\t')
            p++;

        char **toks = g_strsplit_set(p, " \t", -1);
        const char *combo = NULL;
        for (int j = 0; toks[j] != NULL; j++) {
            if (!toks[j][0])
                continue;
            if (g_str_has_prefix(toks[j], "--"))
                continue;
            combo = toks[j];
            break;
        }

        if (combo)
            result = shortcut_hint_normalize(combo);

        g_strfreev(toks);
        g_free(line);

        if (result)
            break;
    }

    g_strfreev(lines);
    if (!result)
        return g_strdup("unknown");
    return result;
}

static char *
parse_hyprland(const char *config_text)
{
    char **lines = g_strsplit(config_text, "\n", -1);
    char *result = NULL;

    for (int i = 0; lines[i] != NULL; i++) {
        char *line = g_strdup(lines[i]);
        g_strstrip(line);

        if (!*line || line[0] == '#') {
            g_free(line);
            continue;
        }

        if (strstr(line, "switchxkblayout") == NULL || strstr(line, "bind") == NULL) {
            g_free(line);
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) {
            g_free(line);
            continue;
        }

        eq++;
        while (*eq == ' ' || *eq == '\t')
            eq++;

        char **parts = g_strsplit(eq, ",", -1);
        if (parts[0] && parts[1]) {
            char *mods = g_strdup(parts[0]);
            char *key = g_strdup(parts[1]);
            g_strstrip(mods);
            g_strstrip(key);

            if (mods[0]) {
                char *raw = g_strconcat(mods, "+", key, NULL);
                result = shortcut_hint_normalize(raw);
                g_free(raw);
            } else {
                result = shortcut_hint_normalize(key);
            }

            g_free(mods);
            g_free(key);
        }

        g_strfreev(parts);
        g_free(line);

        if (result)
            break;
    }

    g_strfreev(lines);
    if (!result)
        return g_strdup("unknown");
    return result;
}

char *
shortcut_hint_parse_for_wm(ShortcutHintWM wm, const char *config_text)
{
    if (!config_text || !*config_text)
        return g_strdup("unknown");

    switch (wm) {
    case SHORTCUT_HINT_WM_SWAY:
    case SHORTCUT_HINT_WM_LABWC:
    case SHORTCUT_HINT_WM_WAYFIRE:
    case SHORTCUT_HINT_WM_RIVER:
        return parse_sway_like(config_text);
    case SHORTCUT_HINT_WM_HYPRLAND:
        return parse_hyprland(config_text);
    case SHORTCUT_HINT_WM_UNKNOWN:
    default: {
        char *sway_try = parse_sway_like(config_text);
        if (g_strcmp0(sway_try, "unknown") != 0)
            return sway_try;
        g_free(sway_try);
        return parse_hyprland(config_text);
    }
    }
}

char *
shortcut_hint_detect(void)
{
    ShortcutHintWM wm = shortcut_hint_detect_wm();
    char *from_opts = shortcut_from_xkb_options(wm);
    if (from_opts)
        return from_opts;

    if (wm == SHORTCUT_HINT_WM_UNKNOWN)
        return g_strdup("unknown");

    char *path = wm_config_path(wm);
    if (!path)
        return g_strdup("unknown");

    char *content = NULL;
    gsize len = 0;
    gboolean ok = g_file_get_contents(path, &content, &len, NULL);
    g_free(path);

    if (!ok || !content)
        return g_strdup("unknown");

    char *parsed = shortcut_hint_parse_for_wm(wm, content);
    g_free(content);
    return parsed;
}
