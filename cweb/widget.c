#include "cweb/widget.h"
#include "cweb/app.h"
#include "cweb/padnmarg.h"

#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ *
 *  Init / free                                                         *
 * ------------------------------------------------------------------ */
void cweb_widget_init(cweb_widget *w, cweb_widget_kind kind) {
    if (!w) return;
    memset(w, 0, sizeof(*w));
    w->kind   = kind;
    w->w      = 1.0f;
    w->h      = 0.0f;
    w->min_w  = 0.0f;
    w->min_h  = 0.0f;
    w->z_index = -1;
    w->flex_grow = 0;
    w->flex_shrink = -1;   /* -1 = unset (don't emit) */
    w->flex_basis = 0;     /* 0 = auto */
    w->r      = -1;  w->g = -1;  w->b = -1;
    w->bg_r   = -1;  w->bg_g = -1;  w->bg_b = -1;
    w->border_r = -1; w->border_g = -1; w->border_b = -1;
    w->border_radius = -1;
    w->visible = 1;
    w->placement = CWEB_PLACE_TOP_LEFT;
    w->font_size = 0;
}

void cweb_widget_set_min_size(cweb_widget *w, float min_w, float min_h) {
    if (!w) return;
    w->min_w = min_w;
    w->min_h = min_h;
}

void cweb_widget_set_min_width(cweb_widget *w, float min_w) {
    if (!w) return;
    w->min_w = min_w;
}

void cweb_widget_set_min_height(cweb_widget *w, float min_h) {
    if (!w) return;
    w->min_h = min_h;
}

void cweb_widget_set_min_height_vh(cweb_widget *w, int vh) {
    if (!w) return;
    w->min_h_vh = vh;
}

void cweb_widget_set_max_size(cweb_widget *w, float max_w, float max_h) {
    if (!w) return;
    w->max_w = max_w;
    w->max_h = max_h;
}

void cweb_widget_set_max_width(cweb_widget *w, float max_w) {
    if (!w) return;
    w->max_w = max_w;
}

void cweb_widget_set_max_height(cweb_widget *w, float max_h) {
    if (!w) return;
    w->max_h = max_h;
}

void cweb_widget_set_max_height_vh(cweb_widget *w, int vh) {
    if (!w) return;
    w->max_h_vh = vh;
}

void cweb_widget_set_sticky(cweb_widget *w, cweb_sticky s) {
    if (!w) return;
    w->sticky = s;
}

void cweb_widget_set_z_index(cweb_widget *w, int z) {
    if (!w) return;
    w->z_index = z;
}

void cweb_widget_set_clip(cweb_widget *w, int enable) {
    if (!w) return;
    w->clip = enable ? 1 : 0;
}

void cweb_widget_set_flex_grow(cweb_widget *w, int grow) {
    if (!w) return;
    w->flex_grow = grow;
    /* Auto-set flex_shrink to 1 so the element can shrink when needed.
       User can override with cweb_widget_set_flex_shrink. */
    if (w->flex_shrink == 0) w->flex_shrink = 1;
}

void cweb_widget_set_flex_shrink(cweb_widget *w, int shrink) {
    if (!w) return;
    w->flex_shrink = shrink;
}

void cweb_widget_set_flex_basis(cweb_widget *w, float basis) {
    if (!w) return;
    w->flex_basis = basis;
}

void cweb_widget_set_link(cweb_widget *w, const char *href) {
    if (!w) return;
    free(w->href);
    w->href = href ? strdup(href) : NULL;
}

void cweb_widget_set_padding_at(cweb_widget *w, cweb_breakpoint bp, cweb_padding pad) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].pad = pad;
    w->bp[bp].active |= CWEB_BP_PADDING;
}

void cweb_widget_set_margin_at(cweb_widget *w, cweb_breakpoint bp, cweb_margin marg) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].marg = marg;
    w->bp[bp].active |= CWEB_BP_MARGIN;
}

void cweb_widget_set_gap_at(cweb_widget *w, cweb_breakpoint bp, int px) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].gap = px;
    w->bp[bp].active |= CWEB_BP_GAP;
}

void cweb_widget_set_size_at(cweb_widget *w, cweb_breakpoint bp, float w_, float h_) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].w = w_;
    w->bp[bp].h = h_;
    w->bp[bp].active |= CWEB_BP_SIZE;
}

void cweb_widget_set_placement_at(cweb_widget *w, cweb_breakpoint bp, cweb_placement pl) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].placement = pl;
    w->bp[bp].active |= CWEB_BP_PLACEMENT;
}

void cweb_container_set_size_vh_at(cweb_widget *w, cweb_breakpoint bp, float wvh, float hvh){
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].wvh = wvh;
    w->bp[bp].hvh = hvh;
    w->bp[bp].active |= CWEB_BP_SIZE_VH;
}

void cweb_widget_set_direction_at(cweb_widget *w, cweb_breakpoint bp, cweb_direction dir) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].direction = (int)dir;
    w->bp[bp].active |= CWEB_BP_DIRECTION;
}

void cweb_widget_hide_at(cweb_widget *w, cweb_breakpoint bp) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].hidden = 1;
    w->bp[bp].active |= CWEB_BP_HIDDEN;
}

void cweb_widget_show_at(cweb_widget *w, cweb_breakpoint bp) {
    if (!w || bp < 0 || bp > 2) return;
    w->bp[bp].hidden = 0;
    w->bp[bp].active |= CWEB_BP_HIDDEN;
}

/* Releases everything OWNED by w — heap children (recursively), owned
   strings and buffers — but NOT the widget struct itself, then zeroes
   every freed field: calling it twice is harmless, and an accidental
   second *_add on the same source produces an empty copy instead of
   reading freed memory.

   Called AUTOMATICALLY by the consuming adds (cweb_container_add,
   cweb_box_add_text, cweb_widget_add_child) once their deep copy into
   the tree succeeded, and by cweb_app_set_root after cloning the root —
   normal UI code never has to call it by hand. Direct use is still
   useful to clear/refill a widget or to discard one on an early-return
   path that was never passed to a consuming call. */
void cweb_widget_free_contents(cweb_widget *w) {
    if (!w) return;
    /* Free children recursively (full free — heap clones are owned here).
       Stack grandchildren were already consumed/disposed by the caller
       before their parent was consumed, so they never appear in this list. */
    for (size_t i = 0; i < w->children_count; i++) {
        cweb_widget_free(w->children[i]);
    }
    free(w->children);
    free(w->content);
    free(w->font_family);
    free(w->box_font_family);
    free(w->input_buffer);
    free(w->placeholder);
    free(w->img_src);
    free(w->img_alt);
    free(w->href);
    /* Zero out so the widget is inert afterwards (idempotent cleanup). */
    w->children = NULL;
    w->children_count = 0;
    w->children_cap = 0;
    w->content = NULL;
    w->font_family = NULL;
    w->box_font_family = NULL;
    w->input_buffer = NULL;
    w->input_buf_cap = 0;
    w->input_buf_len = 0;
    w->placeholder = NULL;
    w->img_src = NULL;
    w->img_alt = NULL;
    w->href = NULL;
}

void cweb_widget_free(cweb_widget *w) {
    if (!w) return;
    /* Free children recursively. */
    cweb_widget_free_contents(w);
    /* Free the widget struct itself.
       For heap-allocated widgets (via cweb_widget_take_child / malloc),
       this is correct.
       For stack-allocated widgets, use cweb_widget_free_contents instead —
       free() on a non-heap pointer is UB. */
    free(w);
}

/* ------------------------------------------------------------------ *
 *  Deep copy (consumed by the *_add family)                           *
 *                                                                    *
 *  cweb_container_add(parent, &child) makes a full independent copy   *
 *  of `child`, attaches it, and DRAINS the original (frees + zeroes   *
 *  its heap members), so the caller has no cleanup to remember.       *
 *  To attach ONE source to several parents without emptying it, pair  *
 *  clone + take:                                                      *
 *      cweb_widget_take_child(p, cweb_widget_clone(&src));            *
 * ------------------------------------------------------------------ */
cweb_widget *cweb_widget_clone(const cweb_widget *src) {
    if (!src) return NULL;
    cweb_widget *dst = malloc(sizeof(cweb_widget));
    if (!dst) return NULL;
    memcpy(dst, src, sizeof(*dst));
    /* Reset identity / tree state */
    dst->id = 0;
    dst->parent = NULL;
    dst->children = NULL;
    dst->children_count = 0;
    dst->children_cap = 0;

    /* Deep copy owned strings */
    if (src->content) {
        dst->content = strdup(src->content);
        if (!dst->content) { free(dst); return NULL; }
    }
    if (src->font_family) {
        dst->font_family = strdup(src->font_family);
        if (!dst->font_family) { free(dst->content); free(dst); return NULL; }
    }
    if (src->box_font_family) {
        dst->box_font_family = strdup(src->box_font_family);
        if (!dst->box_font_family) {
            free(dst->content); free(dst->font_family); free(dst);
            return NULL;
        }
    }
    if (src->input_buffer) {
        dst->input_buffer = malloc(src->input_buf_cap);
        if (!dst->input_buffer) {
            free(dst->content); free(dst->font_family);
            free(dst->box_font_family); free(dst);
            return NULL;
        }
        memcpy(dst->input_buffer, src->input_buffer, src->input_buf_len + 1);
        dst->input_buf_cap = src->input_buf_cap;
        dst->input_buf_len = src->input_buf_len;
    }
    if (src->placeholder) {
        dst->placeholder = strdup(src->placeholder);
        if (!dst->placeholder) {
            free(dst->content); free(dst->font_family);
            free(dst->box_font_family); free(dst->input_buffer); free(dst);
            return NULL;
        }
    }
    if (src->img_src) {
        dst->img_src = strdup(src->img_src);
        if (!dst->img_src) {
            free(dst->content); free(dst->font_family);
            free(dst->box_font_family); free(dst->input_buffer);
            free(dst->placeholder); free(dst);
            return NULL;
        }
    }
    if (src->img_alt) {
        dst->img_alt = strdup(src->img_alt);
        if (!dst->img_alt) {
            free(dst->content); free(dst->font_family);
            free(dst->box_font_family); free(dst->input_buffer);
            free(dst->placeholder); free(dst->img_src); free(dst);
            return NULL;
        }
    }
    if (src->href) {
        dst->href = strdup(src->href);
        if (!dst->href) {
            free(dst->content); free(dst->font_family);
            free(dst->box_font_family); free(dst->input_buffer);
            free(dst->placeholder); free(dst->img_src); free(dst->img_alt);
            free(dst);
            return NULL;
        }
    }

    /* Deep copy children (recursive) */
    if (src->children_count > 0) {
        dst->children = malloc(src->children_count * sizeof(cweb_widget*));
        if (!dst->children) {
            free(dst->content); free(dst->input_buffer); free(dst);
            return NULL;
        }
        for (size_t i = 0; i < src->children_count; i++) {
            cweb_widget *child_copy = cweb_widget_clone(src->children[i]);
            if (!child_copy) {
                /* Rollback */
                for (size_t j = 0; j < i; j++) cweb_widget_free(dst->children[j]);
                free(dst->children);
                free(dst->content);
                free(dst->font_family);
                free(dst->box_font_family);
                free(dst->input_buffer);
                free(dst);
                return NULL;
            }
            child_copy->parent = dst;
            dst->children[i] = child_copy;
        }
        dst->children_count = src->children_count;
        dst->children_cap   = src->children_count;
    }
    return dst;
}

/* ------------------------------------------------------------------ *
 *  Registry (session-aware)                                           *
 *                                                                    *
 *  Widget ids are assigned lazily at render time. When a request is   *
 *  being served the ids live in THAT visitor's session (each session  *
 *  tree is numbered from 1, so every visitor sees identical HTML);    *
 *  without a session (--html mode, custom 404 rendering) the app-     *
 *  level registry is used, exactly like before sessions existed.      *
 * ------------------------------------------------------------------ */
static int registry_add(cweb_app *app, cweb_widget *w) {
    if (!app || !w) return -1;
    cweb_session *sess = app->cur_sess;   /* NULL outside request handling */
    if (sess) {
        if (sess->registry_count == sess->registry_cap) {
            size_t nc = sess->registry_cap ? sess->registry_cap * 2 : 16;
            cweb_widget **na = realloc(sess->registry, nc * sizeof(cweb_widget*));
            if (!na) return -1;
            sess->registry = na;
            sess->registry_cap = nc;
        }
        w->id = ++sess->next_id;
        sess->registry[sess->registry_count++] = w;
        return w->id;
    }
    if (app->registry_count == app->registry_cap) {
        size_t nc = app->registry_cap ? app->registry_cap * 2 : 16;
        cweb_widget **na = realloc(app->registry, nc * sizeof(cweb_widget*));
        if (!na) return -1;
        app->registry = na;
        app->registry_cap = nc;
    }
    w->id = ++app->next_id;
    app->registry[app->registry_count++] = w;
    return w->id;
}

cweb_widget *cweb_widget_find(cweb_widget *root, int id) {
    if (!root) return NULL;
    if (root->id == id) return root;
    for (size_t i = 0; i < root->children_count; i++) {
        cweb_widget *f = cweb_widget_find(root->children[i], id);
        if (f) return f;
    }
    return NULL;
}

/* ------------------------------------------------------------------ *
 *  Add child (deep copy, drain source + register with the app)        *
 * ------------------------------------------------------------------ */
int cweb_widget_add_child(cweb_widget *parent, cweb_widget *child) {
    if (!parent || !child) return -1;
    if (parent->children_count == parent->children_cap) {
        size_t nc = parent->children_cap ? parent->children_cap * 2 : 4;
        cweb_widget **na = realloc(parent->children, nc * sizeof(cweb_widget*));
        if (!na) return -1;
        parent->children = na;
        parent->children_cap = nc;
    }
    cweb_widget *copy = cweb_widget_clone(child);
    if (!copy) return -1;   /* child stays intact — caller may retry */
    copy->parent = parent;
    parent->children[parent->children_count++] = copy;

    /* Consuming add: the tree now owns an independent copy, so release
       the original's heap members right here (and zero them — see
       cweb_widget_free_contents). The struct storage itself stays with
       the caller: stack widgets remain valid until scope exit.         */
    cweb_widget_free_contents(child);
    return copy->id;
}

int cweb_widget_take_child(cweb_widget *parent, cweb_widget *child) {
    if (!parent || !child) return -1;
    if (parent->children_count == parent->children_cap) {
        size_t nc = parent->children_cap ? parent->children_cap * 2 : 4;
        cweb_widget **na = realloc(parent->children, nc * sizeof(cweb_widget*));
        if (!na) return -1;
        parent->children = na;
        parent->children_cap = nc;
    }
    /* No deep copy — take the pointer directly. The library now owns it
       and will free it via cweb_widget_free when the parent is freed.     */
    child->parent = parent;
    parent->children[parent->children_count++] = child;
    return child->id;
}

/* Walk the tree and ensure every widget has a registered id. */
static void register_recursive(cweb_app *app, cweb_widget *w) {
    if (!w) return;
    if (w->id == 0) {
        registry_add(app, w);
    }
    for (size_t i = 0; i < w->children_count; i++) {
        register_recursive(app, w->children[i]);
    }
}

void cweb_app_register_tree(cweb_app *app, cweb_widget *root) {
    register_recursive(app, root);
}
