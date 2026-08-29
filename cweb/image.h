#ifndef CWEB_IMAGE_H
#define CWEB_IMAGE_H

#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

/* An image widget — renders as <img src="..." alt="..."> with optional
   width/height and object-fit. The src can be:
     - An absolute URL: "https://example.com/foo.png"
     - A server-relative URL: "/static/foo.png" (served by the static
       file handler in http.c)
     - A data: URI: "data:image/png;base64,...."                              */

typedef enum {
  CWEB_IMG_FILL = 0,    /* object-fit: fill (stretch)   */
  CWEB_IMG_CONTAIN = 1, /* object-fit: contain          */
  CWEB_IMG_COVER = 2,   /* object-fit: cover            */
} cweb_image_fit;

int cweb_image_create(cweb_widget *img, const char *src);
void cweb_image_set_src(cweb_widget *img, const char *src);
void cweb_image_set_alt(cweb_widget *img, const char *alt);
void cweb_image_set_size(cweb_widget *img, int w_px, int h_px);
void cweb_image_set_fit(cweb_widget *img, cweb_image_fit fit);
/* Round the image's own corners (emits border-radius on the <img>).
   To instead cut the image off along the PARENT box's rounded corners
   when it overflows, set the parent's clip: cweb_widget_set_clip(p, 1). */
void cweb_image_set_radius(cweb_widget *img, int radius_px);

void cweb_image_set_pad(cweb_widget *img, cweb_padding pad);
void cweb_image_set_marg(cweb_widget *img, cweb_margin marg);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_IMAGE_H */
