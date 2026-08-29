#ifndef CWEB_HTML_H
#define CWEB_HTML_H

#include "cweb/app.h"

/* Render the entire widget tree (root + metadata) into a freshly
   allocated HTML string. Caller must free() the result. The string
   is NUL-terminated; *out_len receives its length (excluding NUL).     */
char *cweb_html_render(cweb_app *app, size_t *out_len);

/* Render just the body of a widget (used for partial responses —
   MVP doesn't use this yet, but we keep the signature for future
   HTMX-style partial updates).                                          */
char *cweb_html_render_widget(cweb_widget *w, size_t *out_len);

#endif
