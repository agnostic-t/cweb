#include "colors.h"
#include <stdlib.h>
#include <string.h>

cweb_rgb cweb_clr_hex(const char *code) {
    if (!code) goto failure;
    if (strlen(code) != 7) goto failure;
    if (code[0] != '#') goto failure;

    char r_hex[3] = {code[1], code[2], 0};
    char g_hex[3] = {code[3], code[4], 0};
    char b_hex[3] = {code[5], code[6], 0};

    int r = (int)strtol(r_hex, NULL, 16);
    int g = (int)strtol(g_hex, NULL, 16);
    int b = (int)strtol(b_hex, NULL, 16);

    return (cweb_rgb){r, g, b};
failure:
    return (cweb_rgb){0, 0, 0};
}
