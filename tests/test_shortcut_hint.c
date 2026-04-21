#include <glib.h>

#include "shortcut_hint.h"

static void
test_normalize(void)
{
    g_autofree char *a = shortcut_hint_normalize("ALT+SHIFT");
    g_assert_cmpstr(a, ==, "Alt+Shift");

    g_autofree char *b = shortcut_hint_normalize("SUPER+SPACE");
    g_assert_cmpstr(b, ==, "Super+Space");

    g_autofree char *c = shortcut_hint_normalize("CAPSLOCK");
    g_assert_cmpstr(c, ==, "Caps Lock");
}

static void
test_normalize_unknown(void)
{
    g_autofree char *a = shortcut_hint_normalize(NULL);
    g_assert_cmpstr(a, ==, "unknown");

    g_autofree char *b = shortcut_hint_normalize("   ");
    g_assert_cmpstr(b, ==, "unknown");
}

static void
test_parse_sway(void)
{
    const char *cfg =
        "# sample sway config\n"
        "bindsym Alt+Shift exec swaymsg input type:keyboard xkb_switch_layout next\n";

    g_autofree char *hint = shortcut_hint_parse_for_wm(SHORTCUT_HINT_WM_SWAY, cfg);
    g_assert_cmpstr(hint, ==, "Alt+Shift");
}

static void
test_parse_hypr(void)
{
    const char *cfg =
        "bind = SUPER, SPACE, exec, hyprctl switchxkblayout all next\n";

    g_autofree char *hint = shortcut_hint_parse_for_wm(SHORTCUT_HINT_WM_HYPRLAND, cfg);
    g_assert_cmpstr(hint, ==, "Super+Space");
}

static void
test_parse_unknown_fallback(void)
{
    const char *cfg =
        "bindsym $mod+space nop\n"
        "# no layout switch command\n";

    g_autofree char *hint = shortcut_hint_parse_for_wm(SHORTCUT_HINT_WM_SWAY, cfg);
    g_assert_cmpstr(hint, ==, "unknown");
}

int
main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/shortcut_hint/normalize", test_normalize);
    g_test_add_func("/shortcut_hint/normalize_unknown", test_normalize_unknown);
    g_test_add_func("/shortcut_hint/parse_sway", test_parse_sway);
    g_test_add_func("/shortcut_hint/parse_hypr", test_parse_hypr);
    g_test_add_func("/shortcut_hint/parse_unknown", test_parse_unknown_fallback);

    return g_test_run();
}
