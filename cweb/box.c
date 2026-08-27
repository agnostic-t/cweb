#include "cweb/box.h"
#include "cweb/app.h"

#include <stdlib.h>
#include <string.h>

int cweb_box_create(cweb_widget *b) {
    if (!b) return -1;
    cweb_widget_init(b, CWEB_W_BOX);
    return 0;
}

void cweb_box_set_size(cweb_widget *b, float w, float h) {
    if (!b) return;
    b->w = w; b->h = h;
}

void cweb_box_set_color(cweb_widget *b, int r, int g, int bl) {
    if (!b) return;
    b->r = r; b->g = g; b->b = bl;
}

void cweb_box_set_bg(cweb_widget *b, int r, int g, int bl) {
    if (!b) return;
    b->bg_r = r; b->bg_g = g; b->bg_b = bl;
}

void cweb_box_set_border(cweb_widget *b, cweb_border_style style) {
    if (!b) return;
    b->border = style;
}

void cweb_box_set_border_color(cweb_widget *b, int r, int g, int bl) {
    if (!b) return;
    b->border_r = r;
    b->border_g = g;
    b->border_b = bl;
}

void cweb_box_set_border_radius(cweb_widget *b, int px) {
    if (!b) return;
    b->border_radius = px;  /* 0 = sharp; -1 = inherit style default */
}

void cweb_box_set_padding(cweb_widget *b, int px) {
    if (!b) return;
    b->padding = px;
}

void cweb_box_set_margin(cweb_widget *b, int px) {
    if (!b) return;
    b->margin = px;
}

void cweb_box_set_placement(cweb_widget *b, cweb_placement p) {
    if (!b) return;
    b->placement = p;
}

void cweb_box_set_font_family(cweb_widget *b, const char *family) {
    if (!b) return;
    free(b->box_font_family);
    b->box_font_family = family ? strdup(family) : NULL;
}

void cweb_box_set_font_size(cweb_widget *b, int px) {
    if (!b) return;
    b->font_size = px;
}

int cweb_box_add_text(cweb_widget *b, cweb_widget *t) {
    if (!b || !t) return -1;
    return cweb_widget_add_child(b, t);
}

int cweb_box_take_text(cweb_widget *b, cweb_widget *t) {
    if (!b || !t) return -1;
    return cweb_widget_take_child(b, t);
}

void cweb_box_set_on_click(cweb_widget *b, cweb_callback cb) {
    if (!b) return;
    b->on_click = cb;
}

void cweb_box_set_input(cweb_widget *b, cweb_callback cb) {
    if (!b) return;
    b->on_input = cb;
    b->inputable = 1;  /* even with NULL callback, render <input> */
    if (!b->input_buffer) {
        b->input_buf_cap = 256;
        b->input_buffer = malloc(b->input_buf_cap);
        if (b->input_buffer) {
            b->input_buffer[0] = '\0';
            b->input_buf_len = 0;
        }
    }
}

void cweb_box_set_multiline(cweb_widget *b, int enable) {
    if (!b) return;
    b->multiline = enable ? 1 : 0;
}

void cweb_box_set_placeholder(cweb_widget *b, const char *text) {
    if (!b) return;
    free(b->placeholder);
    b->placeholder = text ? strdup(text) : NULL;
}

const char *cweb_input_get_value(cweb_widget *b) {
    if (!b || !b->input_buffer) return "";
    return b->input_buffer;
}

void cweb_input_set_value(cweb_widget *b, const char *value) {
    if (!b || !value) return;
    if (!b->input_buffer) {
        b->input_buf_cap = 256;
        b->input_buffer = malloc(b->input_buf_cap);
        if (!b->input_buffer) return;
    }
    size_t len = strlen(value);
    if (len + 1 > b->input_buf_cap) {
        size_t nc = b->input_buf_cap;
        while (nc < len + 1) nc *= 2;
        char *nb = realloc(b->input_buffer, nc);
        if (!nb) return;
        b->input_buffer = nb;
        b->input_buf_cap = nc;
    }
    memcpy(b->input_buffer, value, len + 1);
    b->input_buf_len = len;
}
