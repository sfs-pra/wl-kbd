#include "icon.h"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <glib.h>
#include <string.h>

#define CORNER_R  4.0
#define FONT_FACE "Liberation Sans Bold"

/* Draw a rounded rectangle path (no fill/stroke).
 * Uses GLib G_PI / G_PI_2 to avoid M_PI availability issues in strict C11. */
static void rounded_rect(cairo_t *cr,
                          double x, double y,
                          double w, double h,
                          double r)
{
    cairo_move_to  (cr, x + r, y);
    cairo_line_to  (cr, x + w - r, y);
    cairo_arc      (cr, x + w - r, y + r,     r, -G_PI_2, 0);
    cairo_line_to  (cr, x + w, y + h - r);
    cairo_arc      (cr, x + w - r, y + h - r, r, 0,        G_PI_2);
    cairo_line_to  (cr, x + r, y + h);
    cairo_arc      (cr, x + r,     y + h - r, r, G_PI_2,   G_PI);
    cairo_line_to  (cr, x, y + r);
    cairo_arc      (cr, x + r,     y + r,     r, G_PI,     3 * G_PI_2);
    cairo_close_path(cr);
}

guint8 *icon_render_letter(const char *text, IconTheme theme, int size)
{
    int sz = size;
    if (sz < 16) sz = 16;
    if (sz > 64) sz = 64;

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sz, sz);
    cairo_t         *cr   = cairo_create(surf);
    double bg_r = 0.03, bg_g = 0.03, bg_b = 0.03;
    double fg_r = 1.0, fg_g = 1.0, fg_b = 1.0;
    cairo_text_extents_t extents;
    double x, y;

    /* Clear to transparent */
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    /* Background (match wl-kbd-indicator look) */
    if (theme == ICON_THEME_DARK) {
        bg_r = 1.0; bg_g = 1.0; bg_b = 1.0;
        fg_r = 0.05; fg_g = 0.05; fg_b = 0.05;
    }

    rounded_rect(cr, 1.0, 1.0, sz - 2.0, sz - 2.0, CORNER_R);
    cairo_set_source_rgb(cr, bg_r, bg_g, bg_b);
    cairo_fill(cr);

    if (theme == ICON_THEME_DARK) {
        rounded_rect(cr, 1.5, 1.5, sz - 3.0, sz - 3.0, CORNER_R);
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.25);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    cairo_select_font_face(cr, FONT_FACE, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 14.5);

    cairo_text_extents(cr, text, &extents);
    x = (sz - extents.width) / 2.0 - extents.x_bearing;
    y = (sz - extents.height) / 2.0 - extents.y_bearing;
    x = (double)((int)(x + (x >= 0.0 ? 0.5 : -0.5)));
    y = (double)((int)(y + (y >= 0.0 ? 0.5 : -0.5)));

    cairo_set_source_rgb(cr, fg_r, fg_g, fg_b);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_surface_flush(surf);

    /* Convert Cairo BGRA (LE) → ARGB big-endian for SNI */
    const guint8 *src    = cairo_image_surface_get_data(surf);
    int           stride = cairo_image_surface_get_stride(surf);
    guint8       *argb   = g_new(guint8, sz * sz * 4);

    for (int y = 0; y < sz; y++) {
        const guint8 *row = src + y * stride;
        for (int x = 0; x < sz; x++) {
            const int si = x * 4;
            const int di = (y * sz + x) * 4;
            argb[di + 0] = row[si + 3]; /* A */
            argb[di + 1] = row[si + 2]; /* R */
            argb[di + 2] = row[si + 1]; /* G */
            argb[di + 3] = row[si + 0]; /* B */
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
    return argb;
}
