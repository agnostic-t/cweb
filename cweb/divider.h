#ifndef CWEB_DIVIDER_H
#define CWEB_DIVIDER_H

#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

int cweb_divider_create(cweb_widget *d);

void cweb_divider_set_size(cweb_widget *d, float w);
void cweb_divider_set_color(cweb_widget *d, int r, int g, int b);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_DIVIDER_H */
