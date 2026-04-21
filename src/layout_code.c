/*
 * layout_code.c — Map XKB layout codes / descriptions to canonical icon codes.
 *
 * Ported from flag_utils.vala (labwc-kbd) with identical mapping table.
 */
#include "layout_code.h"
#include <string.h>
#include <ctype.h>
#include <glib.h>

/* Strip variant suffix: "us(intl)" → "us", "ru" → "ru" */
static void strip_variant(const char *in, char *out, size_t outsz)
{
    size_t n = 0;
    for (const char *p = in; *p && n < outsz - 1; p++) {
        if (*p == '(' || *p == '/')
            break;
        out[n++] = *p;
    }
    out[n] = '\0';
    /* trim trailing whitespace */
    while (n > 0 && out[n - 1] == ' ')
        out[--n] = '\0';
}

/* Sanitize to lowercase alphanumeric, collapsing non-alnum to '-' */
static void sanitize(const char *in, char *out, size_t outsz)
{
    size_t n = 0;
    int last_dash = 1;   /* start true to suppress leading dash */
    for (const char *p = in; *p && n < outsz - 1; p++) {
        char c = (char)tolower((unsigned char)*p);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[n++] = c;
            last_dash = 0;
        } else if (!last_dash && n > 0) {
            out[n++] = '-';
            last_dash = 1;
        }
    }
    /* strip trailing dash */
    if (n > 0 && out[n - 1] == '-') n--;
    out[n] = '\0';
}

const char *layout_to_icon_code(const char *layout)
{
    if (!layout || !*layout) return "us";

    /* Strip variant and lowercase */
    char base[64];
    strip_variant(layout, base, sizeof(base));

    char lower[64];
    size_t i = 0;
    for (; base[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)tolower((unsigned char)base[i]);
    lower[i] = '\0';

    /* Known long-name → code mappings (mirrors flag_utils.vala) */
    struct { const char *name; const char *code; } map[] = {
        { "en",           "us" },
        { "english",      "us" },
        { "american",     "us" },
        { "russian",      "ru" },
        { "ukrainian",    "ua" },
        { "belarusian",   "by" },
        { "ara",          "ar" },
        { "arabic",       "ar" },
        { "armenian",     "am" },
        { "bengali",      "bd" },
        { "bangla",       "bd" },
        { "brazilian",    "br" },
        { "brazil",       "br" },
        { "bulgarian",    "bg" },
        { "canadian",     "ca" },
        { "croatian",     "hr" },
        { "czech",        "cz" },
        { "danish",       "dk" },
        { "estonian",     "et" },
        { "persian",      "ir" },
        { "greek",        "gr" },
        { "hebrew",       "il" },
        { "hindi",        "in" },
        { "japanese",     "jp" },
        { "korean",       "kr" },
        { "latam",        "es" },
        { "latinamerican","es" },
        { "polish",       "pl" },
        { "romanian",     "ro" },
        { "serbian",      "rs" },
        { "slovak",       "sk" },
        { "slovenian",    "si" },
        { "swedish",      "se" },
        { "thai",         "th" },
        { "turkish",      "tr" },
        /* quirky XKB names */
        { "ee",           "et" },  /* Estonia in XKB is "ee" but asset is et.svg */
        { "jp",           "jp" },
        { "nec-vndr",     "jp" },
        { "nec_vndr",     "jp" },
        { NULL, NULL }
    };

    for (int j = 0; map[j].name; j++) {
        if (strcmp(lower, map[j].name) == 0)
            return map[j].code;
    }

    /* Fallback: sanitize and return (static buffer — single-use per call) */
    static char fallback[32];
    sanitize(lower, fallback, sizeof(fallback));
    return fallback[0] ? fallback : "us";
}

void layout_to_display_text(const char *layout, char *buf, int bufsz)
{
    const char *code = layout_to_icon_code(layout);
    int n = 0;
    for (const char *p = code; *p && n < bufsz - 1 && n < 3; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z'))
            buf[n++] = (char)toupper((unsigned char)*p);
    }
    if (n == 0) { buf[0] = '?'; n = 1; }
    buf[n] = '\0';
}
