#ifndef CWEB_WIDGET_H
#define CWEB_WIDGET_H

#include "padnmarg.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 *  Widget kinds                                                       *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_W_NONE = 0,
  CWEB_W_BOX,
  CWEB_W_TEXT,
  CWEB_W_CONTAINER,
  CWEB_W_IMAGE,
} cweb_widget_kind;

/* ------------------------------------------------------------------ *
 *  Border styles (mapped to CSS)                                      *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_BORDER_NONE = 0,
  CWEB_BORDER_SOLID,  /* 1px solid  */
  CWEB_BORDER_DOUBLE, /* 3px double */
  CWEB_BORDER_DASHED, /* 1px dashed */
  CWEB_BORDER_DOTTED, /* 1px dotted */
  CWEB_BORDER_ROUND,  /* 1px solid + border-radius: 6px */
} cweb_border_style;

/* ------------------------------------------------------------------ *
 *  Text style bitmask                                                 *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_TEXT_NORMAL = 0,
  CWEB_TEXT_BOLD = 1 << 0,
  CWEB_TEXT_ITALIC = 1 << 1,
  CWEB_TEXT_UNDER = 1 << 2,
  CWEB_TEXT_STRIKE = 1 << 3,
  CWEB_TEXT_MONO = 1 << 4, /* monospace font */
} cweb_text_style;

/* ------------------------------------------------------------------ *
 *  Placement of a child inside its parent                             *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_PLACE_NOT_SET = -1,
  CWEB_PLACE_TOP_LEFT = 0,
  CWEB_PLACE_TOP,
  CWEB_PLACE_TOP_RIGHT,
  CWEB_PLACE_LEFT,
  CWEB_PLACE_CENTER,
  CWEB_PLACE_RIGHT,
  CWEB_PLACE_CENTER_RIGHT,
  CWEB_PLACE_CENTER_LEFT,
  CWEB_PLACE_BOTTOM_LEFT,
  CWEB_PLACE_BOTTOM,
  CWEB_PLACE_BOTTOM_RIGHT,
} cweb_placement;

/* ------------------------------------------------------------------ *
 *  Flex direction                                                     *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_VERTICAL = 0,
  CWEB_HORIZONTAL,
} cweb_direction;

/* ------------------------------------------------------------------ *
 *  Sticky positioning                                                 *
 *  Used to make headers stick to the top of the viewport while the    *
 *  page scrolls, or footers stick to the bottom. Maps to CSS           *
 *  `position: sticky; top: 0` (or `bottom: 0`).                         *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_STICKY_NONE = 0,
  CWEB_STICKY_TOP,
  CWEB_STICKY_BOTTOM,
} cweb_sticky;

/* ------------------------------------------------------------------ *
 *  Responsive breakpoints                                              *
 *  Used by cweb_widget_set_*_at and cweb_widget_hide_at to emit CSS     *
 *  @media queries.                                                    *
 *    CWEB_MOBILE  → @media (max-width: 767px)                          *
 *    CWEB_TABLET  → @media (max-width: 1023px)                        *
 *    CWEB_DESKTOP → no media query (default)                          *
 *  Note: this is mobile-first as "max-width", so a value set at        *
 *  CWEB_DESKTOP applies always, and a value set at CWEB_MOBILE         *
 *  overrides it only on narrow screens.                              *
 * ------------------------------------------------------------------ */
typedef enum {
  CWEB_DESKTOP = 0,
  CWEB_TABLET,
  CWEB_MOBILE,
} cweb_breakpoint;

/* ------------------------------------------------------------------ *
 *  Responsive overrides storage                                        *
 *  Holds per-breakpoint overrides for one widget. We keep it small    *
 *  (max 4 active fields per breakpoint) for MVP.                       *
 * ------------------------------------------------------------------ */
typedef struct {
  int active; /* bitmask: which fields are set on this bp */

  cweb_padding pad;
  cweb_margin marg;
  cweb_placement placement;

  int gap;
  float w, h;
  float wvh, hvh;
  int hidden;    /* 1 = display:none at this breakpoint */
  int direction; /* -1 = unset; 0 = vertical; 1 = horizontal */
} cweb_bp_override;

#define CWEB_BP_PADDING (1 << 0)
#define CWEB_BP_MARGIN (1 << 1)
#define CWEB_BP_GAP (1 << 2)
#define CWEB_BP_SIZE (1 << 3)
#define CWEB_BP_HIDDEN (1 << 4)
#define CWEB_BP_DIRECTION (1 << 5)
#define CWEB_BP_SIZE_VH (1 << 6)
#define CWEB_BP_PLACEMENT (1 << 7)

/* ------------------------------------------------------------------ *
 *  Callbacks                                                          *
 *  Wired by http.c: POST /api/click?id=N invokes on_click and         *
 *  POST /api/input?id=N invokes on_input on the SESSION's copy of     *
 *  the tree, then the page is re-rendered and returned.               *
 *                                                                    *
 *  The `state` argument is that visitor's private state block         *
 *  produced by the app's state_new hook (see app.h) — or NULL when    *
 *  no state hooks are installed. Mutate it freely: each user gets     *
 *  their own tree AND their own state, so changes never leak across   *
 *  visitors.                                                          *
 * ------------------------------------------------------------------ */
typedef struct cweb_widget cweb_widget;
typedef struct cweb_event cweb_event;
typedef struct cweb_app cweb_app;

struct cweb_event {
  int x, y;    /* for mouse events (pixel coords in browser) */
  char *value; /* for input events (UTF-8 string)            */
  char *key;   /* for key events (KeyboardEvent.key)         */
};

typedef int (*cweb_callback)(cweb_widget *w, cweb_event *ev, void *state);

/* ------------------------------------------------------------------ *
 *  Image widget                                                       *
 * ------------------------------------------------------------------ */
typedef struct {
  char *src;  /* URL or data: URI */
  char *alt;  /* alt text for accessibility */
  int width;  /* px, 0 = auto */
  int height; /* px, 0 = auto */
  int fit;    /* 0=fill, 1=contain, 2=cover */
} cweb_image_props;

/* ------------------------------------------------------------------ *
 *  Unified widget type                                                *
 *                                                                    *
 *  cweb_box / cweb_text / cweb_container are aliases of cweb_widget  *
 *  so the user can keep the type_create/set_value style.             *
 *                                                                    *
 *  Geometry:                                                          *
 *    .w, .h  — floats 0..1 (fraction of parent's client size).        *
 *              In MVP we don't compute pixels in C — we emit CSS      *
 *              flex-basis / width / height percentages and let the     *
 *              browser do the layout.                                 *
 * ------------------------------------------------------------------ */
struct cweb_widget {
  /* Identity */
  int id;
  cweb_widget_kind kind;

  /* Geometry (CSS percentages) */
  float w, h;         /* 0 = auto/flex-grow */
  float wvh, hvh;     /* 0 = no vh */
  float min_w, min_h; /* 0 = no min; >0 = min-width/height */
  int min_h_vh;       /* 0 = no vh constraint; >0 = min-height: Xvh */
  float max_w, max_h; /* 0 = no cap; >0 = max-width/height */
  int max_h_vh;       /* 0 = none; >0 = max-height: Xvh */
  cweb_placement placement;
  cweb_sticky sticky; /* NONE / TOP / BOTTOM */
  int z_index;        /* stacking order; -1 = auto */
  int flex_grow;      /* default 0 */
  int flex_shrink;    /* default 0; -1 = unset */
  float flex_basis;   /* 0 = auto; >0 interpreted like size */

  /* Colors. -1 = inherit / transparent. */
  int r, g, b; /* foreground (text + border) */
  int bg_r, bg_g, bg_b;

  /* Visibility */
  int visible;

  /* ---- Box ---- */
  cweb_border_style border;
  int border_radius; /* px, 0 = sharp corners; -1 = inherit style default */
  int border_r, border_g, border_b; /* -1 = inherit from r/g/b */
  cweb_padding pad;                 /* px (>1) and % (<1), default 0 */
  cweb_margin marg;                 /* px (>1) and % (<1), default 0 */
  int inputable; /* 1 = renders <input>/<textarea> even without on_input */
  char *box_font_family; /* NULL = inherit; e.g. "monospace", "Inter" */

  /* ---- Text ---- */
  int style;         /* cweb_text_style bitmask */
  char *content;     /* owned UTF-8 string */
  int font_size;     /* px, 0 = inherit */
  char *font_family; /* NULL = inherit; e.g. "monospace" */
  /* Text flow: 0 (default) = single line, clipped with a trailing "…"
     (white-space: nowrap + text-overflow: ellipsis).
     1 = pre-wrap: keep manual \n breaks and wrap long lines.           */
  int wrap;

  /* ---- Container ---- */
  cweb_direction direction;
  int gap;        /* px between children, default 0 */
  int scrollable; /* 1 = overflow: auto */
  int clip;       /* 1 = overflow: hidden — children
                     (e.g. big images) are clipped to
                     this widget's bounds, including
                     its border-radius corners.      */

  /* Children (for box and container) */
  struct cweb_widget **children;
  size_t children_count;
  size_t children_cap;

  /* ---- Inputable mixin ---- */
  cweb_callback on_click;
  cweb_callback on_input;
  char *input_buffer;
  size_t input_buf_cap;
  size_t input_buf_len;
  int multiline;
  char *placeholder; /* placeholder text (grey) */

  /* ---- Image ---- */
  char *img_src;  /* URL or data: URI */
  char *img_alt;  /* alt text */
  int img_width;  /* px, 0 = auto */
  int img_height; /* px, 0 = auto */
  int img_fit;    /* 0=fill, 1=contain, 2=cover */

  /* ---- Link (URL target for the whole widget) ---- */
  char *href; /* if set, wrap widget in <a href> */

  /* ---- Responsive overrides (per breakpoint) ---- */
  cweb_bp_override bp[3]; /* indexed by cweb_breakpoint */

  /* Tree */
  struct cweb_widget *parent;
};

/* ---- Lifecycle (used by box/text/container create functions) ---- */
void cweb_widget_init(cweb_widget *w, cweb_widget_kind kind);

/* Releases everything OWNED by w (heap child clones, owned strings,
   input buffer) but NOT the widget struct itself, then zeroes the freed
   fields — calling it twice is harmless.
   Called AUTOMATICALLY by every consuming add (cweb_container_add,
   cweb_box_add_text, cweb_widget_add_child) once the copy succeeded,
   and by cweb_app_set_root after cloning the root, so normal UI code
   never needs this. Direct use remains useful to:
   - clear a widget you want to refill with new content, or
   - discard a partially-built widget on an early-return path.        */
void cweb_widget_free_contents(cweb_widget *w);

/* Fully frees w AND its storage: use only for heap-allocated widgets
   (taken with cweb_*_take_* or produced by clone/add). For stack-allocated
   widgets call cweb_widget_free_contents instead — free() on non-heap
   storage is UB. */
void cweb_widget_free(cweb_widget *w);

/* Min-size — works on any widget (box, container, text). Values:
   - 0 = no min constraint (default)
   - 0 < v <= 1 → percentage of parent (e.g. 1.0 = "100% of parent")
   - v > 1      → fixed pixels
   Setting min_h = 1.0 on a root container produces the "natural page
   scroll" pattern: page fills viewport when content is short, grows
   beyond viewport (browser scrollbar appears) when content is long.

   For the root widget specifically, use cweb_widget_set_min_height_vh(w, 100)
   to emit "min-height: 100vh" — this works regardless of parent height
   (useful when #cweb-root has min-height but no fixed height).          */
void cweb_widget_set_min_size(cweb_widget *w, float min_w, float min_h);
void cweb_widget_set_min_width(cweb_widget *w, float min_w);
void cweb_widget_set_min_height(cweb_widget *w, float min_h);

/* Viewport-relative min-height (in vh units, 1..100 typically).
   Emits "min-height: Xvh". Useful for root widgets that should fill
   the browser viewport. */
void cweb_widget_set_min_height_vh(cweb_widget *w, int vh);

/* Max-size — caps the widget FROM ABOVE. Same value semantics as the
   set_min_* family:
   - 0            = unconstrained (default)
   - 0 < v <= 1   = percentage of the parent's size
   - v > 1        = fixed pixels
   NOTE: percentage caps only bite when the parent has a DEFINITE height;
   with an auto-sized parent (the usual cweb "natural page scroll" layout)
   use pixels or the _vh variant, otherwise the cap resolves to 'none'.
   The main use case is scrollable containers (scrollable=1): a fixed-
   unit max-height + overflow:auto makes the list scroll instead of
   stretching to fit every child.                                        */
void cweb_widget_set_max_size(cweb_widget *w, float max_w, float max_h);
void cweb_widget_set_max_width(cweb_widget *w, float max_w);
void cweb_widget_set_max_height(cweb_widget *w, float max_h);
/* Viewport-relative max-height in vh units. Emits "max-height: Xvh" —
   always definite regardless of parent sizing.                          */
void cweb_widget_set_max_height_vh(cweb_widget *w, int vh);

/* Sticky positioning.
   - CWEB_STICKY_TOP:    element sticks to top of viewport when scrolling down.
   - CWEB_STICKY_BOTTOM: element sticks to bottom of viewport when scrolling up.
   - CWEB_STICKY_NONE:   normal flow (default).
   Maps to CSS `position: sticky; top: 0` (or `bottom: 0`). */
void cweb_widget_set_sticky(cweb_widget *w, cweb_sticky s);

/* Flex grow/shrink — controls how a child takes up free space in its
   parent's main axis. Use cweb_widget_set_flex_grow(main, 1) on a
   "main content" container so it expands to fill the viewport height,
   pushing subsequent siblings (like a footer) to the bottom.           */
void cweb_widget_set_flex_grow(cweb_widget *w, int grow);
void cweb_widget_set_flex_shrink(cweb_widget *w, int shrink);
void cweb_widget_set_flex_basis(cweb_widget *w, float basis); /* px or % */

/* Z-index (stacking order). Use to ensure sticky elements appear above
   their siblings. Default is -1 (CSS auto). Set to e.g. 100 for headers. */
void cweb_widget_set_z_index(cweb_widget *w, int z);

/* Clip children to this widget's bounds — emits CSS `overflow: hidden`.
   Combined with border-radius this is how you make an oversized image
   get cut off along the rounded corners of its box instead of spilling
   out:
       cweb_box_set_border_radius(&card, 16);
       cweb_widget_set_clip(&card, 1);          // children clipped round
   Works on boxes and containers. Mutually exclusive with scrollable
   (scrollable wins on containers).                                     */
void cweb_widget_set_clip(cweb_widget *w, int enable);

/* Wrap the widget in an <a href="..."> link. Useful for navigation:
   clicking the widget navigates to the given URL (server route or
   external). Works on any widget kind. */
void cweb_widget_set_link(cweb_widget *w, const char *href);

/* Responsive overrides.
   Each setter applies only at the given breakpoint, emitted as a CSS
   @media query. The base value (set without _at suffix) applies at all
   sizes; the _at version overrides it on narrower screens.            */
void cweb_widget_set_padding_at(cweb_widget *w, cweb_breakpoint bp,
                                cweb_padding pad);
void cweb_widget_set_margin_at(cweb_widget *w, cweb_breakpoint bp,
                               cweb_margin marg);
void cweb_widget_set_gap_at(cweb_widget *w, cweb_breakpoint bp, int px);
void cweb_widget_set_size_at(cweb_widget *w, cweb_breakpoint bp, float w_,
                             float h_);
void cweb_widget_set_placement_at(cweb_widget *w, cweb_breakpoint bp,
                                  cweb_placement pl);
void cweb_container_set_size_vh_at(cweb_widget *w, cweb_breakpoint bp,
                                   float wvh, float hvh);
/* Override the flex direction at a breakpoint. Use to switch a
   horizontal cards row to vertical stack on mobile.                    */
void cweb_widget_set_direction_at(cweb_widget *w, cweb_breakpoint bp,
                                  cweb_direction dir);
/* Hide / show a widget at a breakpoint. Useful for hiding sidebars on
   mobile or showing a hamburger menu only on narrow screens.         */
void cweb_widget_hide_at(cweb_widget *w, cweb_breakpoint bp);
void cweb_widget_show_at(cweb_widget *w, cweb_breakpoint bp);

/* ---- Tree ops ------------------------------------------------------ */
/* CONSUMING add: makes a deep copy of `child`, attaches it to `parent`,
   then releases the original's own heap members (content strings set by
   *_create/set_*, first-generation clones in its children array).
   Returns the new child's id, or -1 on failure (`child` left intact).

   After a successful call the original is inert: just let it go out of
   scope or re-init it with *_create. One consequence: the SAME widget
   cannot be copied into two parents via _add calls — to share one
   source between parents use
   cweb_widget_take_child(parent, cweb_widget_clone(&src)).             */
int cweb_widget_add_child(cweb_widget *parent, cweb_widget *child);

/* Takes ownership of `child` — NO deep copy. The pointer is attached
   directly to `parent`'s children array and will be freed when `parent`
   is freed (via cweb_widget_free or cweb_app_destroy).
   Use this for heap-allocated widgets (malloc'd) to avoid the deep-copy
   overhead, and pair it with cweb_widget_clone when you want to attach
   ONE source widget to several parents without emptying it.
   After this call, the caller MUST NOT touch `child` — the library owns
   it. Returns the child's id (assigned lazily during render), or -1 on
   failure. */
int cweb_widget_take_child(cweb_widget *parent, cweb_widget *child);

/* ---- Lookup (the user can also keep their own id variables) ---- */
cweb_widget *cweb_widget_find(cweb_widget *root, int id);

/* Deep-copy a widget subtree into a fresh heap-allocated tree. Owned
   strings and buffers are duplicated; ids/parent links are reset.
   Returns NULL on allocation failure. Used internally by the *_add
   functions and by cweb_app_set_root. */
cweb_widget *cweb_widget_clone(const cweb_widget *src);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_WIDGET_H */
