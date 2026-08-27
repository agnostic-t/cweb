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
    CWEB_IMG_FILL    = 0,   /* object-fit: fill (stretch)   */
    CWEB_IMG_CONTAIN = 1,   /* object-fit: contain          */
    CWEB_IMG_COVER   = 2,   /* object-fit: cover            */
} cweb_image_fit;

int  cweb_image_create(cweb_widget *img, const char *src);
void cweb_image_set_src   (cweb_widget *img, const char *src);
void cweb_image_set_alt   (cweb_widget *img, const char *alt);
void cweb_image_set_size  (cweb_widget *img, int w_px, int h_px);
void cweb_image_set_fit   (cweb_widget *img, cweb_image_fit fit);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_IMAGE_H */
