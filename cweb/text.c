#include "cweb/text.h"

#include <stdlib.h>
#include <string.h>

int cweb_text_create(cweb_widget *t, const char *content) {
    if (!t) return -1;
    cweb_widget_init(t, CWEB_W_TEXT);
    if (content) {
        t->content = strdup(content);
    }
    return 0;
}

void cweb_text_set_content(cweb_widget *t, const char *s) {
    if (!t) return;
    free(t->content);
    t->content = s ? strdup(s) : NULL;
}

void cweb_text_set_style(cweb_widget *t, int style_bitmask) {
    if (!t) return;
    t->style = style_bitmask;
}

void cweb_text_set_color(cweb_widget *t, int r, int g, int b) {
    if (!t) return;
    t->r = r; t->g = g; t->b = b;
}

void cweb_text_set_placement(cweb_widget *t, cweb_placement p) {
    if (!t) return;
    t->placement = p;
}

void cweb_text_set_font_size(cweb_widget *t, int px) {
    if (!t) return;
    t->font_size = px;
}

void cweb_text_set_font_family(cweb_widget *t, const char *family) {
    if (!t) return;
    free(t->font_family);
    t->font_family = family ? strdup(family) : NULL;
}

void cweb_text_set_wrap(cweb_widget *t, int enable) {
    if (!t) return;
    t->wrap = enable ? 1 : 0;
}
