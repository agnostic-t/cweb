#ifndef CWEB_BOX_H
#define CWEB_BOX_H

#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A box is a rectangular widget that can have a border, background fill,
   padding, and children (text widgets or other boxes).                  */

/* Create a new box on the caller's storage (stack is fine). When later
   passed to cweb_container_add / cweb_app_set_root, it is copied into
   the tree and drained automatically — no manual cleanup needed.        */
int cweb_box_create(cweb_widget *b);

/* Setters — each mutates the user's struct. Negative color channels
   mean "inherit/transparent".                                          */
void cweb_box_set_size     (cweb_widget *b, float w, float h);
void cweb_box_set_color    (cweb_widget *b, int r, int g, int bl);  /* fg + border (legacy) */
void cweb_box_set_bg       (cweb_widget *b, int r, int g, int bl);
void cweb_box_set_border   (cweb_widget *b, cweb_border_style style);
void cweb_box_set_border_color(cweb_widget *b, int r, int g, int bl);  /* border only */
void cweb_box_set_border_radius(cweb_widget *b, int px);   /* override default; 0 = sharp */
void cweb_box_set_padding  (cweb_widget *b, int px);
void cweb_box_set_margin   (cweb_widget *b, int px);
void cweb_box_set_placement(cweb_widget *b, cweb_placement p);
void cweb_box_set_font_family(cweb_widget *b, const char *family);  /* e.g. "monospace" */
void cweb_box_set_font_size (cweb_widget *b, int px);              /* 0 = inherit */

/* Add a text widget inside the box. Consuming add: deep-copies `t`
   into the box and releases the original's heap members in the same
   call. Returns the new child's id, or -1 on failure.                  */
int  cweb_box_add_text(cweb_widget *b, cweb_widget *t);

/* Take ownership of a heap-allocated text widget. No deep copy — `t`
   is attached directly and freed when `b` is freed. Caller must NOT
   touch `t` after this.                                                 */
int  cweb_box_take_text(cweb_widget *b, cweb_widget *t);

/* Make this box clickable. The callback is invoked when the browser
   sends a POST /api/click?id=N request.                                */
void cweb_box_set_on_click(cweb_widget *b, cweb_callback cb);

/* Make this box an input field. Renders as <textarea> if multiline=1,
   else <input type="text">. The callback can be NULL — the input will
   still be rendered and its value stored in the buffer (retrievable
   via cweb_input_get_value).                                          */
void cweb_box_set_input    (cweb_widget *b, cweb_callback cb);
void cweb_box_set_multiline(cweb_widget *b, int enable);
void cweb_box_set_placeholder(cweb_widget *b, const char *text);

/* Get/set the current input buffer value. */
const char *cweb_input_get_value(cweb_widget *b);
void        cweb_input_set_value(cweb_widget *b, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_BOX_H */
