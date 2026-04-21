#pragma once

/*
 * Map an XKB layout code (as in XKB_DEFAULT_LAYOUT, e.g. "us", "ru",
 * "english", "us(intl)") to a canonical 2-letter ISO country code
 * used for flag icons (e.g. "us", "ru").
 *
 * Returned string is a string literal or points into a static buffer —
 * do NOT free. Result is always lowercase, at most 4 chars.
 */
const char *layout_to_icon_code(const char *layout);

/*
 * Produce a short uppercase display label for a layout code,
 * e.g. "us" → "US", "ru" → "RU". Writes into buf (must be ≥ 4 bytes).
 */
void layout_to_display_text(const char *layout, char *buf, int bufsz);
