#pragma once
#include "config.h"
#include <glib.h>

#define ICON_SIZE 24

/*
 * Render a letter abbreviation (e.g. "US", "RU") as a 24×24 ARGB-BE pixmap
 * suitable for sni_exporter_set_icon_argb().
 *
 * Returns a g_new'd buffer of ICON_SIZE*ICON_SIZE*4 bytes; caller must g_free().
 * text: up to 3 characters, will be truncated to fit.
 */
guint8 *icon_render_letter(const char *text, IconTheme theme, int size);
