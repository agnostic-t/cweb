#include "cweb/image.h"

#include <stdlib.h>
#include <string.h>

int cweb_image_create(cweb_widget *img, const char *src) {
    if (!img) return -1;
    cweb_widget_init(img, CWEB_W_IMAGE);
    if (src) {
        img->img_src = strdup(src);
    }
    return 0;
}

void cweb_image_set_src(cweb_widget *img, const char *src) {
    if (!img) return;
    free(img->img_src);
    img->img_src = src ? strdup(src) : NULL;
}

void cweb_image_set_alt(cweb_widget *img, const char *alt) {
    if (!img) return;
    free(img->img_alt);
    img->img_alt = alt ? strdup(alt) : NULL;
}

void cweb_image_set_size(cweb_widget *img, int w_px, int h_px) {
    if (!img) return;
    img->img_width = w_px;
    img->img_height = h_px;
}

void cweb_image_set_fit(cweb_widget *img, cweb_image_fit fit) {
    if (!img) return;
    img->img_fit = (int)fit;
}

void cweb_image_set_radius(cweb_widget *img, int radius_px) {
    if (!img) return;
    img->border_radius = radius_px > 0 ? radius_px : 0;
}

void cweb_image_set_pad(cweb_widget *img, cweb_padding pad) {
    if (!img) return;
    img->pad = pad;
}

void cweb_image_set_marg(cweb_widget *img, cweb_margin marg) {
    if (!img) return;
    img->marg = marg;
}
