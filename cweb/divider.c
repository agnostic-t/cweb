#include "divider.h"
#include "cweb/box.h"

// cweb_widget divider;
// cweb_box_create(&divider);
// cweb_box_set_size(&divider, 1.0f, 0);          /* 0 height = 1px line */
// cweb_box_set_border(&divider, CWEB_BORDER_SOLID);
// cweb_box_set_border_color(&divider, 0, 0, 0);

int cweb_divider_create(cweb_widget *d){
    if (!d) return -1;

    cweb_box_create(d);
    cweb_box_set_size(d, 1.0f, 0);
    cweb_box_set_border(d, CWEB_BORDER_SOLID);
    cweb_box_set_border_color(d, (cweb_rgb){0, 0, 0});

    return 0;
}

void cweb_divider_set_size(cweb_widget *d, float w) {
    if (!d) return;

    d->w = w;
}

void cweb_divider_set_color(cweb_widget *d, cweb_rgb rgb){
    if (!d) return;

    d->border_r = rgb.r;
    d->border_g = rgb.g;
    d->border_b = rgb.b;
}
