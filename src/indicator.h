#pragma once
#include "config.h"
#include <glib.h>

typedef struct _Indicator Indicator;

/*
 * Create the keyboard layout indicator:
 *   - connects to Wayland keyboard
 *   - exports an SNI tray icon via libsni-exporter
 *   - builds the menu
 *   - sets the initial icon
 *
 * cfg is copied; caller owns the original.
 * loop is borrowed; must remain valid for the lifetime of the indicator.
 *
 * Returns NULL on fatal error (e.g. D-Bus unavailable).
 */
Indicator *indicator_new(Config cfg, GMainLoop *loop, const char *argv0);

void indicator_free(Indicator *ind);
