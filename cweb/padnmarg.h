#ifndef CWEB_PADNMARG_H
#define CWEB_PADNMARG_H

#include <stdbool.h>
typedef struct {
  float top, left, right, bottom;
} cweb_padding;

typedef struct {
  float top, left, right, bottom;
} cweb_margin;

cweb_padding cweb_pad(float val);
cweb_margin cweb_marg(float val);

cweb_padding cweb_pad_x(float val);
cweb_padding cweb_pad_y(float val);

cweb_margin cweb_marg_x(float val);
cweb_margin cweb_marg_y(float val);

bool cweb_pad_nz(cweb_padding pad);
bool cweb_marg_nz(cweb_margin marg);

#endif
