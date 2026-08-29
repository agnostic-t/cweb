#include "cweb/container.h"
#include "cweb/colors.h"
#include "cweb/padnmarg.h"
#include "cweb/widget.h"

#include <stdlib.h>
#include <string.h>

int cweb_container_create(cweb_widget *c, cweb_direction dir) {
    if (!c) return -1;
    cweb_widget_init(c, CWEB_W_CONTAINER);
    c->direction = dir;
    c->placement = CWEB_PLACE_NOT_SET;
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

void cweb_container_set_padding(cweb_widget *c, cweb_padding pad) {
    if (!c) return;
    c->pad = pad;
}

void cweb_container_set_margin(cweb_widget *c, cweb_margin marg){
    if (!c) return;
    c->marg = marg;
}

void cweb_container_set_scrollable(cweb_widget *c, int enable) {
    if (!c) return;
    c->scrollable = enable ? 1 : 0;
}

void cweb_container_set_size(cweb_widget *c, float w, float h) {
    if (!c) return;
    c->w = w; c->h = h;
}

void cweb_container_set_placement(cweb_widget *c, cweb_placement pl) {
    if (!c) return;

    c->placement = pl;
}

void cweb_container_set_size_vh(cweb_widget *c, float wvh, float hvh) {
    if (!c) return;
    c->wvh = wvh; c->hvh = hvh;
}

void cweb_container_set_bg(cweb_widget *c, cweb_rgb rgb) {
    if (!c) return;
    c->bg_r = rgb.r; c->bg_g = rgb.g; c->bg_b = rgb.b;
}

void cweb_container_set_border_color(cweb_widget *c, cweb_rgb rgb) {
    if (!c) return;
    c->border_r = rgb.r; c->border_g = rgb.g; c->border_b = rgb.b;
}

void cweb_container_set_border_radius(cweb_widget *c, int radius) {
    if (!c) return;
    c->border_radius = radius;
}

void cweb_container_set_border(cweb_widget *c, cweb_border_style style) {
    if (!c) return;
    c->border = style;
}

int cweb_container_add(cweb_widget *c, cweb_widget *child) {
    if (!c || !child) return -1;
    return cweb_widget_add_child(c, child);
}

int cweb_container_take(cweb_widget *c, cweb_widget *child) {
    if (!c || !child) return -1;
    return cweb_widget_take_child(c, child);
}
