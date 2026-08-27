#include "cweb/html.h"
#include "cweb/widget.h"
#include "cweb/app.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Defined in widget.c — assigns ids to all widgets in the tree. */
extern void cweb_app_register_tree(cweb_app *app, cweb_widget *root);

/* ------------------------------------------------------------------ *
 *  Dynamic string builder                                             *
 * ------------------------------------------------------------------ */
typedef struct {
    char  *data;
    size_t len;
    size_t cap;
} strbuf;

static void sb_init(strbuf *s) {
    s->cap = 4096;
    s->data = malloc(s->cap);
    s->len = 0;
    if (s->data) s->data[0] = '\0';
}

static void sb_reserve(strbuf *s, size_t extra) {
    size_t need = s->len + extra + 1;
    if (need <= s->cap) return;
    /* Keep doubling (amortized O(1) appends), but commit the new capacity
       ONLY on a successful realloc — otherwise s->cap would claim space
       s->data doesn't have and the next memcpy would overflow the buffer. */
    size_t nc = s->cap;
    while (nc < need) nc *= 2;
    char *nd = realloc(s->data, nc);
    if (!nd) {
        /* Out of memory: the old buffer is still valid, but rendering
           cannot continue safely — fail loudly instead of corrupting. */
        fprintf(stderr, "[cweb] out of memory growing render buffer\n");
        abort();
    }
    s->data = nd;
    s->cap = nc;
}

static void sb_append(strbuf *s, const char *str) {
    if (!str) return;
    size_t n = strlen(str);
    sb_reserve(s, n);
    memcpy(s->data + s->len, str, n);
    s->len += n;
    s->data[s->len] = '\0';
}

static void sb_appendf(strbuf *s, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return; }
    sb_reserve(s, (size_t)n);
    vsnprintf(s->data + s->len, s->cap - s->len, fmt, ap2);
    va_end(ap2);
    s->len += n;
}

/* ------------------------------------------------------------------ *
 *  HTML escaping                                                       *
 * ------------------------------------------------------------------ */
static void sb_append_escaped(strbuf *s, const char *str) {
    if (!str) return;
    for (const unsigned char *p = (const unsigned char*)str; *p; p++) {
        switch (*p) {
            case '<':  sb_append(s, "&lt;");   break;
            case '>':  sb_append(s, "&gt;");   break;
            case '&':  sb_append(s, "&amp;");  break;
            case '"':  sb_append(s, "&quot;");  break;
            case '\'': sb_append(s, "&#39;");   break;
            default:
                if (*p < 32 && *p != '\n' && *p != '\t') continue;
                sb_reserve(s, 1);
                s->data[s->len++] = (char)*p;
                s->data[s->len] = '\0';
        }
    }
}

/* ------------------------------------------------------------------ *
 *  CSS helpers                                                         *
 * ------------------------------------------------------------------ */
static void sb_append_color(strbuf *s, const char *prop, int r, int g, int b) {
    if (r < 0 || g < 0 || b < 0) return;
    sb_appendf(s, "%s: rgb(%d,%d,%d); ", prop, r, g, b);
}

static const char *border_css(cweb_border_style b) {
    switch (b) {
        case CWEB_BORDER_SOLID:  return "1px solid";
        case CWEB_BORDER_DOUBLE: return "3px double";
        case CWEB_BORDER_DASHED: return "1px dashed";
        case CWEB_BORDER_DOTTED: return "1px dotted";
        case CWEB_BORDER_ROUND:  return "1px solid";
        case CWEB_BORDER_NONE:
        default:                 return NULL;
    }
}

static const char *placement_align(cweb_placement p) {
    switch (p) {
        case CWEB_PLACE_TOP_LEFT:
        case CWEB_PLACE_LEFT:
        case CWEB_PLACE_BOTTOM_LEFT:
            return "flex-start";
        case CWEB_PLACE_TOP:
        case CWEB_PLACE_CENTER:
        case CWEB_PLACE_BOTTOM:
            return "center";
        case CWEB_PLACE_TOP_RIGHT:
        case CWEB_PLACE_RIGHT:
        case CWEB_PLACE_BOTTOM_RIGHT:
            return "flex-end";
        default: return "flex-start";
    }
}

static const char *placement_valign(cweb_placement p) {
    switch (p) {
        case CWEB_PLACE_TOP_LEFT:
        case CWEB_PLACE_TOP:
        case CWEB_PLACE_TOP_RIGHT:
            return "flex-start";
        case CWEB_PLACE_LEFT:
        case CWEB_PLACE_CENTER:
        case CWEB_PLACE_RIGHT:
            return "center";
        case CWEB_PLACE_BOTTOM_LEFT:
        case CWEB_PLACE_BOTTOM:
        case CWEB_PLACE_BOTTOM_RIGHT:
            return "flex-end";
        default: return "flex-start";
    }
}

/* ------------------------------------------------------------------ *
 *  Widget → HTML                                                       *
 * ------------------------------------------------------------------ */
static void render_widget(cweb_widget *w, strbuf *out);

static void render_size(strbuf *out, cweb_widget *w) {
    /* width */
    if (w->w > 0 && w->w <= 1.0f) sb_appendf(out, "width: %d%%; ", (int)(w->w * 100));
    else if (w->w > 1.0f)         sb_appendf(out, "width: %dpx; ", (int)w->w);
    /* w == 0 → auto (no width rule) */

    /* height */
    if (w->h > 0 && w->h <= 1.0f) sb_appendf(out, "height: %d%%; ", (int)(w->h * 100));
    else if (w->h > 1.0f)         sb_appendf(out, "height: %dpx; ", (int)w->h);
    /* h == 0 → auto (grow with content) */

    /* min-width */
    if (w->min_w > 0 && w->min_w <= 1.0f)
        sb_appendf(out, "min-width: %d%%; ", (int)(w->min_w * 100));
    else if (w->min_w > 1.0f)
        sb_appendf(out, "min-width: %dpx; ", (int)w->min_w);

    /* min-height */
    if (w->min_h > 0 && w->min_h <= 1.0f)
        sb_appendf(out, "min-height: %d%%; ", (int)(w->min_h * 100));
    else if (w->min_h > 1.0f)
        sb_appendf(out, "min-height: %dpx; ", (int)w->min_h);

    /* Viewport-relative min-height (overrides the above if set). */
    if (w->min_h_vh > 0) {
        sb_appendf(out, "min-height: %dvh; ", w->min_h_vh);
    }

    /* max-width */
    if (w->max_w > 0 && w->max_w <= 1.0f)
        sb_appendf(out, "max-width: %d%%; ", (int)(w->max_w * 100));
    else if (w->max_w > 1.0f)
        sb_appendf(out, "max-width: %dpx; ", (int)w->max_w);

    /* max-height */
    if (w->max_h > 0 && w->max_h <= 1.0f)
        sb_appendf(out, "max-height: %d%%; ", (int)(w->max_h * 100));
    else if (w->max_h > 1.0f)
        sb_appendf(out, "max-height: %dpx; ", (int)w->max_h);

    /* Viewport-relative max-height (overrides the above if set). */
    if (w->max_h_vh > 0) {
        sb_appendf(out, "max-height: %dvh; ", w->max_h_vh);
    }

    /* Flex grow/shrink/basis — only emit if set.
       flex_grow > 0 means "take up remaining space in parent's main axis",
       which is how we push footers to the bottom of the viewport.        */
    if (w->flex_grow > 0 || w->flex_shrink >= 0 || w->flex_basis > 0) {
        int   g = w->flex_grow   > 0 ? w->flex_grow   : 0;
        int   s = w->flex_shrink >= 0 ? w->flex_shrink : 1;
        float b = w->flex_basis;
        if (b > 0 && b <= 1.0f) {
            sb_appendf(out, "flex: %d %d %d%%; ", g, s, (int)(b * 100));
        } else if (b > 1.0f) {
            sb_appendf(out, "flex: %d %d %dpx; ", g, s, (int)b);
        } else {
            /* basis = auto */
            sb_appendf(out, "flex: %d %d auto; ", g, s);
        }
    }

    /* Sticky positioning — maps to CSS `position: sticky; top: 0` (or bottom).
       Useful for headers that stay visible while the page scrolls, or
       footers that stick to the bottom of the viewport.                  */
    if (w->sticky == CWEB_STICKY_TOP) {
        sb_append(out, "position: sticky; top: 0; ");
    } else if (w->sticky == CWEB_STICKY_BOTTOM) {
        sb_append(out, "position: sticky; bottom: 0; ");
    }

    /* Z-index (stacking order). Default -1 means "auto" — don't emit. */
    if (w->z_index >= 0) {
        sb_appendf(out, "z-index: %d; ", w->z_index);
    }
}

static void render_box(cweb_widget *w, strbuf *out) {
    /* For inputable boxes:
       - align-items: stretch — so the input/textarea fills the box's width.
       - justify-content: flex-start — so the input is anchored to the top
         of the box (text in a single-line input starts at the top, not
         centered vertically).
       For non-inputable boxes, honour the user's placement.              */
    const char *halign = placement_align(w->placement);
    const char *valign = placement_valign(w->placement);
    if (w->inputable) {
        halign = "stretch";
        valign = "flex-start";
    }

    sb_appendf(out, "<div id=\"w%d\" style=\"display: flex; flex-direction: column; align-items: %s; justify-content: %s; ",
                w->id,
                halign,
                valign);

    render_size(out, w);
    if (w->padding > 0) sb_appendf(out, "padding: %dpx; ", w->padding);
    if (w->margin  > 0) sb_appendf(out, "margin: %dpx; ",  w->margin);
    sb_append_color(out, "color", w->r, w->g, w->b);
    sb_append_color(out, "background-color", w->bg_r, w->bg_g, w->bg_b);
    if (w->box_font_family) {
        sb_appendf(out, "font-family: %s; ", w->box_font_family);
    }
    if (w->font_size > 0) sb_appendf(out, "font-size: %dpx; ", w->font_size);

    const char *bc = border_css(w->border);
    if (bc) {
        /* Border color: explicit border_* if set, else fallback to fg (r/g/b) */
        int br = w->border_r >= 0 ? w->border_r : (w->r >= 0 ? w->r : 0);
        int bg = w->border_g >= 0 ? w->border_g : (w->g >= 0 ? w->g : 0);
        int bb = w->border_b >= 0 ? w->border_b : (w->b >= 0 ? w->b : 0);
        sb_appendf(out, "border: %s rgb(%d,%d,%d); ", bc, br, bg, bb);

        /* Border radius: explicit value if set, else default 6px for ROUND */
        if (w->border_radius >= 0) {
            sb_appendf(out, "border-radius: %dpx; ", w->border_radius);
        } else if (w->border == CWEB_BORDER_ROUND) {
            sb_append(out, "border-radius: 6px; ");
        }
    } else if (w->border_radius > 0) {
        /* Even without a border, allow rounded corners (useful for bg-only boxes) */
        sb_appendf(out, "border-radius: %dpx; ", w->border_radius);
    }

    /* overflow: hidden — clips overflowing children (big images, wide
       content) to this box's bounds, INCLUDING its rounded corners.      */
    if (w->clip) sb_append(out, "overflow: hidden; ");

    /* Close the style attribute */
    sb_append(out, "\"");

    /* onclick attribute (frontend JS will hook this) */
    if (w->on_click) {
        sb_appendf(out, " onclick=\"cweb_click(%d)\"", w->id);
    }
    sb_append(out, ">");

    /* Children (text widgets) */
    for (size_t i = 0; i < w->children_count; i++) {
        render_widget(w->children[i], out);
    }

    /* Input element if this box is inputable (set via cweb_box_set_input).
       The on_input callback may be NULL — we still render the field and
       buffer its value server-side.

       The input/textarea fills the box's remaining client area (flex: 1)
       so it scales with the box, not a hardcoded min-height.          */
    if (w->inputable) {
        if (w->multiline) {
            sb_appendf(out, "<textarea id=\"in%d\" oninput=\"cweb_input(%d, this.value)\" "
                            "style=\"flex: 1 1 auto; box-sizing: border-box; "
                            "background: transparent; color: inherit; border: none; outline: none; "
                            "resize: none; font: inherit; padding: 4px 0; "
                            "min-height: 0;\" rows=\"4\"", w->id, w->id);
            if (w->placeholder) {
                sb_append(out, " placeholder=\"");
                sb_append_escaped(out, w->placeholder);
                sb_append(out, "\"");
            }
            sb_append(out, ">");
            sb_append_escaped(out, w->input_buffer ? w->input_buffer : "");
            sb_append(out, "</textarea>");
        } else {
            /* Single-line input: NO flex-grow. The input keeps its natural
               single-line height (~30px) and is anchored to the top of the
               box. This prevents the text from appearing vertically centered
               when the box is taller than one line.                          */
            sb_appendf(out, "<input id=\"in%d\" type=\"text\" value=\"", w->id);
            sb_append_escaped(out, w->input_buffer ? w->input_buffer : "");
            sb_appendf(out, "\" oninput=\"cweb_input(%d, this.value)\" "
                            "style=\"flex: 0 0 auto; width: 100%%; box-sizing: border-box; "
                            "background: transparent; color: inherit; border: none; outline: none; "
                            "font: inherit; padding: 4px 0;\"", w->id);
            if (w->placeholder) {
                sb_append(out, " placeholder=\"");
                sb_append_escaped(out, w->placeholder);
                sb_append(out, "\"");
            }
            sb_append(out, " />");
        }
    }
    sb_append(out, "</div>");
}

static void render_text(cweb_widget *w, strbuf *out) {
    /* The main text element is ALWAYS a block <div>: text-overflow /
       white-space only bite on a block that clips itself, and as a flex
       child a div is already the flex item (an inline span inside <b>
       would dodge the clipping). Inline style tags therefore live INSIDE
       the div, not around it.
       - default: one line, clipped with a trailing "…"
         (white-space: nowrap + overflow: hidden + text-overflow: ellipsis;
         overflow: hidden also zeroes the flex automatic min-size, so the
         label shrinks with its button/box instead of pushing it wide).
       - wrap=1:  pre-wrap — keeps manual \n breaks and wraps long lines. */
    sb_appendf(out, "<div id=\"w%d\" style=\"", w->id);
    sb_append_color(out, "color", w->r, w->g, w->b);
    if (w->font_size > 0) sb_appendf(out, "font-size: %dpx; ", w->font_size);
    if (w->font_family) sb_appendf(out, "font-family: %s; ", w->font_family);
    if (w->wrap) {
        sb_append(out, "word-break: break-word; overflow-wrap: break-word; white-space: pre-wrap; ");
    } else {
        /* max-width:100% is essential: in a COLUMN flex the width is the
           cross axis and its floor is min-content (= the whole nowrap
           line) — overflow:hidden alone cannot cap it there. Capping the
           div at the parent's content box makes the ellipsis actually
           fire inside buttons/boxes of any flex direction.             */
        sb_append(out, "white-space: nowrap; overflow: hidden; "
                       "max-width: 100%; text-overflow: ellipsis; ");
    }
    sb_append(out, "\">");

    /* Inline styling tags: <b> <i> <u> <s> <code> as needed (in order). */
    if (w->style & CWEB_TEXT_BOLD)   sb_append(out, "<b>");
    if (w->style & CWEB_TEXT_ITALIC) sb_append(out, "<i>");
    if (w->style & CWEB_TEXT_UNDER)  sb_append(out, "<u>");
    if (w->style & CWEB_TEXT_STRIKE) sb_append(out, "<s>");
    if (w->style & CWEB_TEXT_MONO)   sb_append(out, "<code>");

    /* The actual text content (HTML-escaped). */
    sb_append_escaped(out, w->content ? w->content : "");

    /* Close inline style tags in reverse order, then the block itself. */
    if (w->style & CWEB_TEXT_MONO)   sb_append(out, "</code>");
    if (w->style & CWEB_TEXT_STRIKE) sb_append(out, "</s>");
    if (w->style & CWEB_TEXT_UNDER)  sb_append(out, "</u>");
    if (w->style & CWEB_TEXT_ITALIC) sb_append(out, "</i>");
    if (w->style & CWEB_TEXT_BOLD)   sb_append(out, "</b>");
    sb_append(out, "</div>");
}

static void render_image(cweb_widget *w, strbuf *out) {
    sb_appendf(out, "<img id=\"w%d\" src=\"", w->id);
    sb_append_escaped(out, w->img_src ? w->img_src : "");
    sb_append(out, "\"");
    if (w->img_alt) {
        sb_append(out, " alt=\"");
        sb_append_escaped(out, w->img_alt);
        sb_append(out, "\"");
    }
    sb_append(out, " style=\"");
    if (w->img_width  > 0) sb_appendf(out, "width: %dpx; ",  w->img_width);
    if (w->img_height > 0) sb_appendf(out, "height: %dpx; ", w->img_height);
    switch (w->img_fit) {
        case 1: sb_append(out, "object-fit: contain; "); break;
        case 2: sb_append(out, "object-fit: cover; ");   break;
        default: break;
    }
    /* Round the image's own corners (e.g. avatars/thumbnails). To clip
       the image against the PARENT's rounded box instead, set the parent
       clip: cweb_widget_set_clip(parent, 1).                             */
    if (w->border_radius > 0) sb_appendf(out, "border-radius: %dpx; ", w->border_radius);
    sb_append(out, "max-width: 100%; height: auto;\" />");
}

static void render_container(cweb_widget *w, strbuf *out) {
    sb_appendf(out, "<div id=\"w%d\" style=\"display: flex; flex-direction: %s; ",
                w->id,
                w->direction == CWEB_VERTICAL ? "column" : "row");
    render_size(out, w);
    if (w->gap > 0)     sb_appendf(out, "gap: %dpx; ", w->gap);
    if (w->padding > 0) sb_appendf(out, "padding: %dpx; ", w->padding);
    sb_append_color(out, "background-color", w->bg_r, w->bg_g, w->bg_b);
    if (w->scrollable) {
        sb_append(out, "overflow: auto; ");
    } else if (w->clip) {
        /* scrollable wins — a scroll area already constrains its children. */
        sb_append(out, "overflow: hidden; ");
    }

    /* Close the style attribute */
    sb_append(out, "\"");

    if (w->on_click) {
        sb_appendf(out, " onclick=\"cweb_click(%d)\"", w->id);
    }
    sb_append(out, ">");

    for (size_t i = 0; i < w->children_count; i++) {
        render_widget(w->children[i], out);
    }

    sb_append(out, "</div>");
}

static void render_widget(cweb_widget *w, strbuf *out) {
    if (!w || !w->visible) return;

    /* Wrap in <a href="..."> if a link is set on this widget. */
    int wrapped_in_link = 0;
    if (w->href) {
        sb_append(out, "<a href=\"");
        sb_append_escaped(out, w->href);
        sb_append(out, "\" style=\"text-decoration: none; color: inherit; display: block;\">");
        wrapped_in_link = 1;
    }

    switch (w->kind) {
        case CWEB_W_BOX:       render_box(w, out); break;
        case CWEB_W_TEXT:      render_text(w, out); break;
        case CWEB_W_CONTAINER: render_container(w, out); break;
        case CWEB_W_IMAGE:     render_image(w, out); break;
        default: break;
    }

    if (wrapped_in_link) {
        sb_append(out, "</a>");
    }
}

/* Emit CSS @media rules for any widget that has responsive overrides.
   Walks the tree and produces rules like:
     @media (max-width: 767px) { #w5 { padding: 8px; } }
   Called once after rendering all widgets, before </head>.            */
static void render_media_queries(cweb_widget *w, strbuf *out);

static const char *bp_media_query(cweb_breakpoint bp) {
    switch (bp) {
        case CWEB_MOBILE: return "(max-width: 767px)";
        case CWEB_TABLET: return "(max-width: 1023px)";
        default:          return NULL;
    }
}

static void emit_bp_rules(cweb_widget *w, cweb_breakpoint bp, strbuf *out) {
    const char *mq = bp_media_query(bp);
    if (!mq) return;
    const cweb_bp_override *o = &w->bp[bp];
    if (o->active == 0) return;

    sb_appendf(out, "@media %s { #w%d { ", mq, w->id);
    int any = 0;
    if (o->active & CWEB_BP_HIDDEN) {
        sb_appendf(out, "display: %s !important; ", o->hidden ? "none" : "flex");
        any = 1;
    }
    if (o->active & CWEB_BP_PADDING) {
        sb_appendf(out, "padding: %dpx !important; ", o->padding);
        any = 1;
    }
    if (o->active & CWEB_BP_MARGIN) {
        sb_appendf(out, "margin: %dpx !important; ", o->margin);
        any = 1;
    }
    if (o->active & CWEB_BP_GAP) {
        sb_appendf(out, "gap: %dpx !important; ", o->gap);
        any = 1;
    }
    if (o->active & CWEB_BP_SIZE) {
        if (o->w > 0 && o->w <= 1.0f) sb_appendf(out, "width: %d%% !important; ", (int)(o->w * 100));
        else if (o->w > 1.0f)         sb_appendf(out, "width: %dpx !important; ", (int)o->w);
        if (o->h > 0 && o->h <= 1.0f) sb_appendf(out, "height: %d%% !important; ", (int)(o->h * 100));
        else if (o->h > 1.0f)         sb_appendf(out, "height: %dpx !important; ", (int)o->h);
        any = 1;
    }
    if (o->active & CWEB_BP_DIRECTION) {
        sb_appendf(out, "flex-direction: %s !important; ",
                   o->direction == CWEB_HORIZONTAL ? "row" : "column");
        any = 1;
    }
    if (any) {
        sb_append(out, "} }\n");
    } else {
        sb_append(out, "} }\n");
    }
}

static void render_media_queries(cweb_widget *w, strbuf *out) {
    if (!w) return;
    for (int bp = CWEB_TABLET; bp <= CWEB_MOBILE; bp++) {
        emit_bp_rules(w, (cweb_breakpoint)bp, out);
    }
    for (size_t i = 0; i < w->children_count; i++) {
        render_media_queries(w->children[i], out);
    }
}

/* ------------------------------------------------------------------ *
 *  Public                                                              *
 * ------------------------------------------------------------------ */
char *cweb_html_render_widget(cweb_widget *w, size_t *out_len) {
    if (!w) return NULL;
    strbuf out;
    sb_init(&out);
    render_widget(w, &out);
    if (out_len) *out_len = out.len;
    return out.data;
}

char *cweb_html_render(cweb_app *app, size_t *out_len) {
    if (!app) return NULL;

    /* Render the ACTIVE tree: during request handling that is the
       current visitor's session tree; outside requests (--html mode,
       tests) it is the app's own root.                                     */
    cweb_widget *tree = app->cur_sess ? app->cur_sess->root : app->root;

    /* Ensure every widget has an id (ids go into the active registry) */
    cweb_app_register_tree(app, tree);

    strbuf out;
    sb_init(&out);

    /* <!DOCTYPE html> + head */
    sb_append(&out, "<!DOCTYPE html>\n<html lang=\"");
    sb_append_escaped(&out, app->meta.lang ? app->meta.lang : "en");
    sb_append(&out, "\">\n<head>\n  <meta charset=\"utf-8\">\n");
    /* Critical for mobile: without this, mobile browsers use a virtual
       980px viewport, making everything tiny and preventing media queries
       from triggering.                                              */
    sb_append(&out, "  <meta name=\"viewport\" content=\"width=device-width, "
                    "initial-scale=1, maximum-scale=5\">\n");

    if (app->meta.title) {
        sb_append(&out, "  <title>");
        sb_append_escaped(&out, app->meta.title);
        sb_append(&out, "</title>\n");
    } else {
        sb_append(&out, "  <title>cweb app</title>\n");
    }
    if (app->meta.description) {
        sb_append(&out, "  <meta name=\"description\" content=\"");
        sb_append_escaped(&out, app->meta.description);
        sb_append(&out, "\">\n");
    }
    if (app->meta.favicon) {
        sb_append(&out, "  <link rel=\"icon\" href=\"");
        sb_append_escaped(&out, app->meta.favicon);
        sb_append(&out, "\">\n");
    }

    /* Reset + base CSS.
       Key: html/body and #cweb-root use min-height instead of height so
       the page grows naturally with content. The browser scrollbar handles
       scrolling — no need for overflow:auto on inner containers.

       Background: body inherits the root widget's background color so
       overscroll (the bounce area above/below the content) matches the
       page background instead of showing a default dark color.

       overscroll-behavior: none — forbids the browser from letting the
       user drag the page past its top/bottom edge (no rubber-band bounce,
       no parent scroll chaining). Combined with the JS scroll-clamp below,
       this guarantees sticky headers/footers stay glued to the viewport
       edges regardless of how hard the user scrolls past the content.    */
    sb_append(&out,
        "  <style id=\"cweb-base\">\n"
        "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
        "    html, body { width: 100%; min-height: 100%; overscroll-behavior: none; }\n"
        "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
        "           font-size: 16px; line-height: 1.4; overflow-y: auto; }\n"
        "    #cweb-root { width: 100vw; min-height: 100vh; }\n"
        "    code { font-family: 'SF Mono', 'Consolas', monospace; }\n"
        "  </style>\n");

    /* Set body bg from root widget so overscroll area matches. */
    if (tree &&
        (tree->bg_r >= 0 || tree->bg_g >= 0 || tree->bg_b >= 0)) {
        int r = tree->bg_r >= 0 ? tree->bg_r : 26;
        int g = tree->bg_g >= 0 ? tree->bg_g : 26;
        int b = tree->bg_b >= 0 ? tree->bg_b : 46;
        sb_appendf(&out,
            "  <style>\n"
            "    html, body { background-color: rgb(%d,%d,%d); }\n"
            "  </style>\n",
            r, g, b);
    } else {
        sb_append(&out,
            "  <style>\n"
            "    html, body { background: #1a1a2e; color: #e0e0e0; }\n"
            "  </style>\n");
    }

    if (app->meta.extra_head) {
        sb_append(&out, app->meta.extra_head);
        sb_append(&out, "\n");
    }

    /* Emit @media queries for any widget with responsive overrides.
       Wrap in <style> so the browser parses them as CSS, not text. */
    if (tree) {
        sb_append(&out, "  <style id=\"cweb-responsive\">\n");
        render_media_queries(tree, &out);
        sb_append(&out, "  </style>\n");
    }

    sb_append(&out, "</head>\n<body class=\"");
    sb_append_escaped(&out, app->meta.body_class ? app->meta.body_class : "");
    sb_append(&out, "\">\n  <div id=\"cweb-root\">\n");

    /* Render the widget tree */
    if (tree) {
        render_widget(tree, &out);
    }

    sb_append(&out, "\n  </div>\n");

    /* Inline JS for interactivity.
       - Saves activeElement + selection before fetch.
       - Restores focus + cursor position after DOM swap.
       - Saves/restores scroll position of ALL scrollable elements.
       - Debounces input events (50ms) so fast typing doesn't flood the server.
       - Request counter prevents out-of-order responses from clobbering
         newer state.
       - Input value preservation: if the user typed more after the debounce
         fired (e.g. pressed backspace 3 times quickly), the server's stale
         response is overridden with the user's current value. This prevents
         the "flash back to old value" bug during fast typing.
       - Scroll clamp: blocks wheel/touch scrolling past the page bounds. */
    sb_append(&out,
        "  <script>\n"
        "    let cweb_seq = 0;\n"
        "    let cweb_input_timer = null;\n"
        "    let cweb_input_pending = null;\n"
        "    let cweb_sent_value = null;  /* value sent to server, for stale detection */\n"
        "\n"
        "    // Clamp wheel scroll so the page can't be dragged past its\n"
        "    // top/bottom edge — but ONLY when there's actual scrollable\n"
        "    // content (maxScroll > 0). When content fits the viewport,\n"
        "    // we let the browser handle overscroll naturally so that\n"
        "    // mobile browsers (iOS Safari) can show/hide their URL bar.\n"
        "    window.addEventListener('wheel', function(e) {\n"
        "      const maxScroll = document.documentElement.scrollHeight - window.innerHeight;\n"
        "      if (maxScroll <= 0) return;\n"
        "      if (window.scrollY <= 0 && e.deltaY < 0) { e.preventDefault(); window.scrollTo(0, 0); }\n"
        "      if (window.scrollY >= maxScroll && e.deltaY > 0) { e.preventDefault(); window.scrollTo(0, maxScroll); }\n"
        "    }, { passive: false });\n"
        "\n"
        "    let cweb_touch_start_y = null;\n"
        "    window.addEventListener('touchstart', function(e) {\n"
        "      cweb_touch_start_y = e.touches[0].clientY;\n"
        "    }, { passive: true });\n"
        "    window.addEventListener('touchmove', function(e) {\n"
        "      const maxScroll = document.documentElement.scrollHeight - window.innerHeight;\n"
        "      // Don't clamp when content fits the viewport — let the\n"
        "      // browser manage its own overscroll + URL bar show/hide.\n"
        "      if (maxScroll <= 0) return;\n"
        "      const dy = e.touches[0].clientY - cweb_touch_start_y;\n"
        "      if (window.scrollY <= 0 && dy > 0) { e.preventDefault(); window.scrollTo(0, 0); }\n"
        "      if (window.scrollY >= maxScroll && dy < 0) { e.preventDefault(); window.scrollTo(0, maxScroll); }\n"
        "    }, { passive: false });\n"
        "\n"
        "    function saveFocus() {\n"
        "      const el = document.activeElement;\n"
        "      if (!el) return null;\n"
        "      if (el.tagName !== 'INPUT' && el.tagName !== 'TEXTAREA') return null;\n"
        "      return {\n"
        "        id: el.id,\n"
        "        start: el.selectionStart,\n"
        "        end: el.selectionEnd,\n"
        "        value: el.value\n"
        "      };\n"
        "    }\n"
        "\n"
        "    function restoreFocus(saved) {\n"
        "      if (!saved || !saved.id) return;\n"
        "      const el = document.getElementById(saved.id);\n"
        "      if (!el) return;\n"
        "      if (el.tagName !== 'INPUT' && el.tagName !== 'TEXTAREA') return;\n"
        "      el.focus();\n"
        "      if (el.value !== saved.value) {\n"
        "        const n = el.value.length;\n"
        "        el.setSelectionRange(n, n);\n"
        "      } else {\n"
        "        const s = Math.min(saved.start, el.value.length);\n"
        "        const e = Math.min(saved.end,   el.value.length);\n"
        "        el.setSelectionRange(s, e);\n"
        "      }\n"
        "    }\n"
        "\n"
        "    function saveScrolls() {\n"
        "      const scrolls = { _window: {x: window.scrollX, y: window.scrollY} };\n"
        "      document.querySelectorAll('div, textarea').forEach(el => {\n"
        "        if (el.scrollTop > 0 || el.scrollLeft > 0) {\n"
        "          if (el.id) scrolls[el.id] = {x: el.scrollLeft, y: el.scrollTop};\n"
        "        }\n"
        "      });\n"
        "      return scrolls;\n"
        "    }\n"
        "\n"
        "    function restoreScrolls(scrolls) {\n"
        "      if (!scrolls) return;\n"
        "      if (scrolls._window) {\n"
        "        window.scrollTo(scrolls._window.x, scrolls._window.y);\n"
        "      }\n"
        "      for (const key in scrolls) {\n"
        "        if (key === '_window') continue;\n"
        "        const el = document.getElementById(key);\n"
        "        if (el && scrolls[key]) {\n"
        "          el.scrollLeft = scrolls[key].x;\n"
        "          el.scrollTop  = scrolls[key].y;\n"
        "        }\n"
        "      }\n"
        "    }\n"
        "\n"
        "    function swapDOM(html, saved) {\n"
        "      const scrolls = saveScrolls();\n"
        "      /* Capture the CURRENT input value BEFORE swap. If the user\n"
        "         typed more after the debounce fired (e.g. pressed backspace\n"
        "         3 times quickly while only 1 request was in flight), the\n"
        "         server's response will have the OLD value. We detect this\n"
        "         by comparing the current value with what we sent (cweb_sent_value).\n"
        "         If they differ, the user typed more — override the server's\n"
        "         value with the user's current one.                          */\n"
        "      let currentUserValue = null;\n"
        "      let currentInputId = null;\n"
        "      if (saved && saved.id) {\n"
        "        const el = document.getElementById(saved.id);\n"
        "        if (el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) {\n"
        "          currentUserValue = el.value;\n"
        "          currentInputId = saved.id;\n"
        "        }\n"
        "      }\n"
        "\n"
        "      document.documentElement.innerHTML = html;\n"
        "      restoreScrolls(scrolls);\n"
        "\n"
        "      /* If the user typed more after we sent the value, restore\n"
        "         their version instead of the (stale) server response.     */\n"
        "      if (currentInputId && currentUserValue !== null && cweb_sent_value !== null\n"
        "          && currentUserValue !== cweb_sent_value) {\n"
        "        const newEl = document.getElementById(currentInputId);\n"
        "        if (newEl && (newEl.tagName === 'INPUT' || newEl.tagName === 'TEXTAREA')) {\n"
        "          newEl.value = currentUserValue;\n"
        "          newEl.focus();\n"
        "          const n = currentUserValue.length;\n"
        "          newEl.setSelectionRange(n, n);\n"
        "          cweb_sent_value = null;\n"
        "          return;\n"
        "        }\n"
        "      }\n"
        "\n"
        "      /* Normal case: restore focus + cursor from saved state. */\n"
        "      restoreFocus(saved);\n"
        "      cweb_sent_value = null;\n"
        "    }\n"
        "\n"
        "    async function cweb_click(id) {\n"
        "      const seq = ++cweb_seq;\n"
        "      const saved = saveFocus();\n"
        "      try {\n"
        "        const r = await fetch('/api/click?id=' + id, {method:'POST'});\n"
        "        if (seq !== cweb_seq) return;\n"
        "        const html = await r.text();\n"
        "        if (seq !== cweb_seq) return;\n"
        "        swapDOM(html, saved);\n"
        "      } catch (e) { console.error('cweb_click:', e); }\n"
        "    }\n"
        "\n"
        "    function cweb_input(id, value) {\n"
        "      cweb_input_pending = {id, value};\n"
        "      if (cweb_input_timer) clearTimeout(cweb_input_timer);\n"
        "      cweb_input_timer = setTimeout(async () => {\n"
        "        cweb_input_timer = null;\n"
        "        const pending = cweb_input_pending;\n"
        "        cweb_input_pending = null;\n"
        "        const seq = ++cweb_seq;\n"
        "        const saved = saveFocus();\n"
        "        cweb_sent_value = pending.value;  /* remember what we sent */\n"
        "        try {\n"
        "          const r = await fetch('/api/input?id=' + pending.id, {\n"
        "            method:'POST', body: pending.value\n"
        "          });\n"
        "          if (seq !== cweb_seq) { cweb_sent_value = null; return; }\n"
        "          const html = await r.text();\n"
        "          if (seq !== cweb_seq) { cweb_sent_value = null; return; }\n"
        "          swapDOM(html, saved);\n"
        "        } catch (e) { console.error('cweb_input:', e); cweb_sent_value = null; }\n"
        "      }, 50);\n"
        "    }\n"
        "  </script>\n");

    sb_append(&out, "</body>\n</html>\n");

    if (out_len) *out_len = out.len;
    return out.data;
}
