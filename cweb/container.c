#include "cweb/container.h"

#include <stdlib.h>
#include <string.h>

int cweb_container_create(cweb_widget *c, cweb_direction dir) {
    if (!c) return -1;
    cweb_widget_init(c, CWEB_W_CONTAINER);
    c->direction = dir;
    return 0;
}

void cweb_container_set_direction(cweb_widget *c, cweb_direction dir) {
    if (!c) return;
    c->direction = dir;
}

void cweb_container_set_gap(cweb_widget *c, int px) {
    if (!c) return;
    c->gap = px;
}

void cweb_container_set_padding(cweb_widget *c, int px) {
    if (!c) return;
    c->padding = px;
}

void cweb_container_set_scrollable(cweb_widget *c, int enable) {
    if (!c) return;
    c->scrollable = enable ? 1 : 0;
}

void cweb_container_set_size(cweb_widget *c, float w, float h) {
    if (!c) return;
    c->w = w; c->h = h;
}

void cweb_container_set_bg(cweb_widget *c, int r, int g, int b) {
    if (!c) return;
    c->bg_r = r; c->bg_g = g; c->bg_b = b;
}

int cweb_container_add(cweb_widget *c, cweb_widget *child) {
    if (!c || !child) return -1;
    return cweb_widget_add_child(c, child);
}

int cweb_container_take(cweb_widget *c, cweb_widget *child) {
    if (!c || !child) return -1;
    return cweb_widget_take_child(c, child);
}
