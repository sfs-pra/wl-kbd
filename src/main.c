#include <locale.h>
#include <libintl.h>

#include <glib.h>

#include "config.h"
#include "indicator.h"

int
main(int argc, char **argv)
{
    setlocale(LC_ALL, "");
    bindtextdomain("wl-kbd", "/usr/share/locale");
    bind_textdomain_codeset("wl-kbd", "UTF-8");
    textdomain("wl-kbd");

    Config cfg = config_load();

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "-f") == 0) {
            cfg.tray_style = TRAY_STYLE_FLAG;
        } else if (g_strcmp0(argv[i], "-l") == 0) {
            cfg.tray_style = TRAY_STYLE_LETTERS;
        }
    }

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    Indicator *ind = indicator_new(cfg, loop, argv[0]);
    if (!ind) {
        g_printerr("wl-kbd: failed to initialize\n");
        g_main_loop_unref(loop);
        return 1;
    }

    g_main_loop_run(loop);

    indicator_free(ind);
    g_main_loop_unref(loop);
    return 0;
}
