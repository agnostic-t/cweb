#ifndef CWEB_CONTAINER_H
#define CWEB_CONTAINER_H

#include "colors.h"
#include "padnmarg.h"
#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A container is a flex layout widget. Children are laid out either
   vertically (column) or horizontally (row) according to `direction`.
   Containers can be nested arbitrarily.                                 */

int cweb_container_create(cweb_widget *c, cweb_direction dir);
void cweb_container_set_direction(cweb_widget *c, cweb_direction dir);
void cweb_container_set_gap(cweb_widget *c, int px);
void cweb_container_set_padding(cweb_widget *c, cweb_padding pad);
void cweb_container_set_margin(cweb_widget *c, cweb_margin marg);
void cweb_container_set_scrollable(cweb_widget *c, int enable);
void cweb_container_set_size(cweb_widget *c, float w, float h);
void cweb_container_set_placement(cweb_widget *c, cweb_placement pl);
void cweb_container_set_size_vh(cweb_widget *c, float wvh, float hvh);
void cweb_container_set_border_radius(cweb_widget *c, int radius);
void cweb_container_set_border(cweb_widget *c, cweb_border_style style);
void cweb_container_set_bg(cweb_widget *c, cweb_rgb rgb);
void cweb_container_set_border_color(cweb_widget *c, cweb_rgb rgb);

/* Add a child (box, text, or another container). Owning/consuming add:
   makes a deep copy of `child`, attaches it and releases the original's
   heap members — afterwards the original is inert; just let it go out
   of scope (or re-init it with *_create). Returns the new child's id,
   or -1 on failure (original left intact).                              */
int cweb_container_add(cweb_widget *c, cweb_widget *child);

/* Take ownership of a heap-allocated child. No deep copy — the pointer
   is moved into the tree and freed together with the parent. Caller
   must NOT touch `child` again. Pair with cweb_widget_clone to attach
   ONE source to several parents while keeping it intact.                */
int cweb_container_take(cweb_widget *c, cweb_widget *child);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_CONTAINER_H */
