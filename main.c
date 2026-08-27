/* Demo app — showcases all cweb features in a single page.

   Every visitor gets a PRIVATE clone of this page plus a private state
   block (see demo_state / cweb_app_set_state_callbacks): the click
   counter below is per-user, two browsers never see each other's
   changes. */

#include "cweb/app.h"
#include "cweb/widget.h"
#include "cweb/box.h"
#include "cweb/text.h"
#include "cweb/container.h"
#include "cweb/image.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- Per-session user state ----------------------------------------
   Allocated by the app's state_new hook when a NEW visitor arrives
   (new cweb_sid cookie), released by state_free when the session is
   evicted or the app shuts down.                                      */
typedef struct {
    int clicks;   /* this visitor's click counter */
} demo_state;

static void *demo_state_new(void) {
    return calloc(1, sizeof(demo_state));
}

static void demo_state_free(void *state) {
    free(state);
}

/* ---- Click callback: turns the box black, counts clicks per user ---- */
int on_click(cweb_widget *w, cweb_event *ev, void *state) {
    (void)ev;
    demo_state *st = state;
    st->clicks++;
    cweb_box_set_bg(w, 0, 0, 0);
    if (w->children_count > 0) {
        char label[96];
        snprintf(label, sizeof(label),
                 "Clicks: %d — this counter is YOURS (open a 2nd browser)",
                 st->clicks);
        cweb_text_set_content(w->children[0], label);
    }
    return 0;
}

/* ---- Input callback: when value is "Hi", replaces with "Bye" ---- */
int on_input(cweb_widget *w, cweb_event *ev, void *state) {
    (void)ev;
    (void)state;
    if (strcmp(cweb_input_get_value(w), "Hi") == 0) {
        cweb_input_set_value(w, "Bye");
    }
    return 0;
}

int main(int argc, char **argv) {
    cweb_app app;
    cweb_app_create(&app, "127.0.0.1", 8080);
    /* Per-session state: every new visitor gets a fresh demo_state. */
    cweb_app_set_state_callbacks(&app, demo_state_new, demo_state_free);

    /* Metadata */
    cweb_meta_set_title(&app, "cweb demo");
    cweb_meta_set_description(&app, "A minimal HTML UI built with cweb");
    cweb_meta_set_lang(&app, "en");
    /* Inline SVG favicon (no external file needed) */
    cweb_meta_set_favicon(&app,
        "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16'>"
        "<rect width='16' height='16' fill='%2360a5fa'/>"
        "<text x='8' y='12' font-family='monospace' font-size='10' "
        "font-weight='bold' fill='%23fff' text-anchor='middle'>c</text>"
        "</svg>");

    /* Root: vertical flex filling the whole viewport.
       min_height_vh(100) must be set EXPLICITLY on the user's root:
       the host #cweb-root only min-heights the outer page shell, while
       the widget tree itself would otherwise stay content-sized and
       a stick-to-bottom footer would have nothing to push against.      */
    cweb_widget root;
    cweb_container_create(&root, CWEB_VERTICAL);
    cweb_container_set_gap(&root, 12);
    cweb_container_set_padding(&root, 24);
    cweb_widget_set_min_height_vh(&root, 100);

    /* 1) Header: a colored box with bold white text — no border, rounded corners */
    cweb_widget header;
    cweb_box_create(&header);
    cweb_box_set_size(&header, 1.0f, 0.1f);
    cweb_box_set_bg(&header, 60, 100, 200);
    cweb_box_set_padding(&header, 16);
    cweb_box_set_border_radius(&header, 12);
    cweb_box_set_placement(&header, CWEB_PLACE_CENTER);
    cweb_box_set_font_family(&header, "Georgia, serif");

    cweb_widget header_text;
    cweb_text_create(&header_text, "cweb demo");
    cweb_text_set_style(&header_text, CWEB_TEXT_BOLD);
    cweb_text_set_color(&header_text, 255, 255, 255);
    cweb_text_set_font_size(&header_text, 28);
    cweb_box_add_text(&header, &header_text);

    cweb_container_add(&root, &header);

    /* 2) Click row: a horizontal flex with two clickable color boxes */
    cweb_widget click_row;
    cweb_container_create(&click_row, CWEB_HORIZONTAL);
    cweb_container_set_gap(&click_row, 12);
    cweb_container_set_size(&click_row, 1.0f, 0.15f);

    /* "OK" — green button, rounded */
    cweb_widget ok;
    cweb_box_create(&ok);
    cweb_box_set_size(&ok, 0.5f, 1.0f);
    cweb_box_set_bg(&ok, 60, 180, 80);
    cweb_box_set_color(&ok, 255, 255, 255);
    cweb_box_set_padding(&ok, 16);
    cweb_box_set_border_radius(&ok, 8);
    cweb_box_set_placement(&ok, CWEB_PLACE_CENTER);
    cweb_box_set_on_click(&ok, on_click);
    {
        cweb_widget t;
        cweb_text_create(&t, "Click me — I turn black");
        cweb_text_set_color(&t, 255, 255, 255);
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_wrap(&t, 0);
        cweb_box_add_text(&ok, &t);
    }
    cweb_container_add(&click_row, &ok);

    /* "Cancel" — red button with a contrasting border */
    cweb_widget cancel;
    cweb_box_create(&cancel);
    cweb_box_set_size(&cancel, 0.5f, 1.0f);
    cweb_box_set_bg(&cancel, 200, 60, 60);
    cweb_box_set_border(&cancel, CWEB_BORDER_SOLID);
    cweb_box_set_border_color(&cancel, 255, 255, 255);
    cweb_box_set_border_radius(&cancel, 20);
    cweb_box_set_color(&cancel, 255, 255, 255);
    cweb_box_set_padding(&cancel, 16);
    cweb_box_set_placement(&cancel, CWEB_PLACE_CENTER);
    cweb_box_set_on_click(&cancel, on_click);
    {
        cweb_widget t;
        cweb_text_create(&t, "Or click me");
        cweb_text_set_color(&t, 255, 255, 255);
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_wrap(&t, 0);
        cweb_box_add_text(&cancel, &t);
    }
    cweb_container_add(&click_row, &cancel);

    cweb_container_add(&root, &click_row);

    /* 3) Input box — single-line, monospace font */
    cweb_widget input_box;
    cweb_box_create(&input_box);
    cweb_box_set_size(&input_box, 1.0f, 0.1f);
    cweb_box_set_border(&input_box, CWEB_BORDER_ROUND);
    cweb_box_set_color(&input_box, 100, 200, 100);
    cweb_box_set_padding(&input_box, 12);
    cweb_box_set_input(&input_box, on_input);
    cweb_box_set_font_family(&input_box, "'SF Mono', Consolas, monospace");
    {
        cweb_widget t;
        cweb_text_create(&t, "Type 'Hi' and watch it become 'Bye':");
        cweb_text_set_color(&t, 180, 180, 180);
        cweb_text_set_font_size(&t, 14);
        cweb_box_add_text(&input_box, &t);
    }
    cweb_container_add(&root, &input_box);

    /* 4) Multiline input — textarea with custom border color + radius */
    cweb_widget ml_box;
    cweb_box_create(&ml_box);
    cweb_box_set_size(&ml_box, 1.0f, 0.3f);
    cweb_box_set_border(&ml_box, CWEB_BORDER_SOLID);
    cweb_box_set_border_color(&ml_box, 255, 200, 100);
    cweb_box_set_border_radius(&ml_box, 12);
    cweb_box_set_color(&ml_box, 230, 230, 230);
    cweb_box_set_padding(&ml_box, 12);
    cweb_box_set_input(&ml_box, on_input);
    cweb_box_set_multiline(&ml_box, 1);
    {
        cweb_widget t;
        cweb_text_create(&t, "Multiline textarea (type 'Hi' for a surprise):");
        cweb_text_set_color(&t, 180, 180, 180);
        cweb_text_set_font_size(&t, 14);
        cweb_box_add_text(&ml_box, &t);
    }
    cweb_container_add(&root, &ml_box);

    /* 4b) Text flow demo — two modes in one dark box:
       wrap=0 → ONE line clipped with a trailing "…" when the box is
                 too narrow (that's what keeps button labels inside);
       default  → pre-wrap: keeps manual \n breaks, wraps long lines.      */
    cweb_widget wrap_box;
    cweb_box_create(&wrap_box);
    cweb_box_set_size(&wrap_box, 1.0f, 0.15f);
    cweb_box_set_bg(&wrap_box, 40, 40, 60);
    cweb_box_set_padding(&wrap_box, 12);
    cweb_box_set_border_radius(&wrap_box, 6);
    {
        cweb_widget t;
        cweb_text_create(&t,
            "Default text: one line — narrow the window and it ends with "
            "an ellipsis right at the box edge instead of spilling over.");
        cweb_text_set_color(&t, 200, 200, 200);
        cweb_text_set_font_size(&t, 14);
        cweb_box_add_text(&wrap_box, &t);
    }
    {
        cweb_widget t;
        cweb_text_create(&t,
            "wrap=1 keeps manual breaks:\nsecond line,\nthird line — "
            "and a long tail that still auto-wraps when it reaches the box edge.");
        cweb_text_set_color(&t, 200, 200, 200);
        cweb_text_set_font_size(&t, 14);
        cweb_box_add_text(&wrap_box, &t);
    }
    cweb_container_add(&root, &wrap_box);

    /* 4c) Rounded-clip demo — the image is bigger than its box, and
       set_clip(1) (= CSS overflow:hidden) cuts it off along the box's
       ROUNDED corners instead of letting it spill out.                   */
    cweb_widget clip_box;
    cweb_box_create(&clip_box);
    cweb_box_set_size(&clip_box, 1.0f, 0.16f);
    cweb_box_set_bg(&clip_box, 70, 130, 180);
    cweb_box_set_border_radius(&clip_box, 24);
    cweb_widget_set_clip(&clip_box, 1);
    {
        cweb_widget img;
        cweb_image_create(&img,
            "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='600' height='240'>"
            "<rect width='600' height='240' fill='%23e67e22'/>"
            "<circle cx='80' cy='120' r='75' fill='%23ffffff' opacity='0.55'/>"
            "<circle cx='230' cy='70' r='50' fill='%232c3e50' opacity='0.45'/>"
            "<text x='340' y='140' font-family='monospace' font-size='48' "
            "fill='%23ffffff'>BIG IMG</text></svg>");
        cweb_image_set_alt(&img, "wide picture clipped by a rounded box");
        cweb_image_set_size(&img, 600, 240);
        cweb_container_add(&clip_box, &img);
    }
    cweb_container_add(&root, &clip_box);

    /* 5) Scrollable container with many items — capped by max-height so
       only a few items fit and the rest scroll. Height percentages don't
       bite here (#cweb-root is min-height-based → children resolve % as
       auto), so the cap uses fixed pixels (>1) or _vh units.            */
    cweb_widget scroll_area;
    cweb_container_create(&scroll_area, CWEB_VERTICAL);
    cweb_container_set_gap(&scroll_area, 8);
    cweb_container_set_size(&scroll_area, 1.0f, 0);
    cweb_widget_set_max_height(&scroll_area, 240);   /* >1 = px; try _vh too */
    cweb_container_set_scrollable(&scroll_area, 1);
    cweb_container_set_bg(&scroll_area, 30, 30, 50);
    cweb_container_set_padding(&scroll_area, 12);

    char label[64];
    for (int i = 0; i < 20; i++) {
        snprintf(label, sizeof(label), "Item %d — scroll to see more", i + 1);
        cweb_widget item;
        cweb_box_create(&item);
        cweb_box_set_size(&item, 1.0f, 0);
        cweb_box_set_bg(&item, 60 + i*5, 80, 120);
        cweb_box_set_padding(&item, 8);
        cweb_box_set_border_radius(&item, 4);
        cweb_widget t;
        cweb_text_create(&t, label);
        cweb_text_set_color(&t, 255, 255, 255);
        cweb_box_add_text(&item, &t);
        cweb_container_add(&scroll_area, &item);
    }
    cweb_container_add(&root, &scroll_area);

    /* Spacer with flex_grow=1 absorbs any free vertical space, pressing
       the footer to the bottom of the viewport (same trick as the
       flex_grow wrapper in article.c). When content is taller than the
       screen it collapses to zero and just stops contributing. */
    cweb_widget spacer;
    cweb_container_create(&spacer, CWEB_VERTICAL);
    cweb_widget_set_flex_grow(&spacer, 1);
    cweb_container_add(&root, &spacer);

    /* 6) Footer — invisible box with small grey text */
    cweb_widget footer;
    cweb_box_create(&footer);
    cweb_box_set_size(&footer, 1.0f, 0.05f);
    {
        cweb_widget t;
        cweb_text_create(&t, "Built with cweb — Ctrl+C to stop the server");
        cweb_text_set_color(&t, 120, 120, 140);
        cweb_text_set_font_size(&t, 12);
        cweb_text_set_style(&t, CWEB_TEXT_ITALIC);
        cweb_box_add_text(&footer, &t);
    }
    cweb_container_add(&root, &footer);

    cweb_app_set_root(&app, &root);

    /* If --html is passed, just dump the HTML to stdout (no server). */
    if (argc > 1 && strcmp(argv[1], "--html") == 0) {
        char *html = cweb_app_render_html(&app);
        if (html) {
            fputs(html, stdout);
            free(html);
        }
        cweb_app_destroy(&app);
        return 0;
    }

    fprintf(stderr, "Press Ctrl+C to stop the server.\n");
    fprintf(stderr, "Open http://127.0.0.1:8080/ in your browser.\n");

    cweb_app_run(&app);
    cweb_app_destroy(&app);
    return 0;
}
