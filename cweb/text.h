#ifndef CWEB_TEXT_H
#define CWEB_TEXT_H

#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

/* A text widget — a single string with optional styling. Always lives
   inside a parent box (or container — but typically a box). The text
   is laid out according to the parent's child placement rules.         */

int  cweb_text_create(cweb_widget *t, const char *content);
void cweb_text_set_content (cweb_widget *t, const char *s);
void cweb_text_set_style   (cweb_widget *t, int style_bitmask);   /* CWEB_TEXT_* */
void cweb_text_set_color   (cweb_widget *t, int r, int g, int b);
void cweb_text_set_placement(cweb_widget *t, cweb_placement p);
void cweb_text_set_font_size(cweb_widget *t, int px);
void cweb_text_set_font_family(cweb_widget *t, const char *family);
/* Text flow. Default: ONE line, clipped with a trailing "…" when it
   doesn't fit its parent (never overlaps neighbours).
   set_wrap(1) → pre-wrap: keeps manual \n line breaks AND wraps long
   lines — use for paragraphs, descriptions, log output.                  */
void cweb_text_set_wrap    (cweb_widget *t, int enable);   /* 1 = pre-wrap: keep \n + wrap */

#ifdef __cplusplus
}
#endif

#endif /* CWEB_TEXT_H */
