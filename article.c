/* Demo: multi-page news site with stack-allocated widgets.

   Builders receive a pre-owned root widget; children are stack-allocated
   and attached with cweb_container_add / cweb_box_add_text. Both are
   CONSUMING adds: each makes a deep copy into the tree and releases the
   original's heap members in the same call - so although user code has
   no malloc/free at all, nothing leaks across served requests.
*/

#include "cweb/app.h"
#include "cweb/widget.h"
#include "cweb/box.h"
#include "cweb/text.h"
#include "cweb/container.h"
#include "cweb/divider.h"
#include "cweb/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *path);

/* Helper: build the sticky header (reused across pages) */
static void build_header_into(cweb_widget *parent) {
    cweb_widget header;
    cweb_box_create(&header);
    cweb_box_set_size(&header, 1.0f, 0);
    cweb_box_set_bg(&header, (cweb_rgb){33, 47, 61});
    cweb_box_set_padding(&header, cweb_pad(16));
    cweb_widget_set_sticky(&header, CWEB_STICKY_TOP);
    cweb_widget_set_z_index(&header, 100);
    {
        cweb_widget t;
        cweb_text_create(&t, "cweb news");
        cweb_text_set_color(&t, CWEB_CLR_WHITE);
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_font_size(&t, 22);
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_widget_set_link(&t, "/");
        cweb_box_add_text(&header, &t);
    }
    cweb_container_add(parent, &header);
}

/* ---- Front page ---- */
static void build_front_page(cweb_app *app, cweb_widget *root) {
    cweb_container_create(root, CWEB_VERTICAL);
    cweb_box_set_size(root, 1.0f, 0);
    cweb_widget_set_min_height_vh(root, 100);
    cweb_container_set_bg(root, (cweb_rgb){245, 245, 248});
    cweb_container_set_padding(root, cweb_pad(24));
    cweb_container_set_gap(root, 24);

    build_header_into(root);

    /* Main content wrapper — flex_grow: 1 so footer sticks to bottom */
    cweb_widget main;
    cweb_container_create(&main, CWEB_VERTICAL);
    cweb_box_set_size(&main, 1.0f, 0);
    cweb_container_set_gap(&main, 24);
    cweb_widget_set_flex_grow(&main, 1);

    /* Hero */
    cweb_widget hero;
    cweb_box_create(&hero);
    cweb_box_set_size(&hero, 1.0f, 0);
    cweb_box_set_bg(&hero, CWEB_CLR_WHITE);
    cweb_box_set_padding(&hero, cweb_pad(32));
    cweb_box_set_border_radius(&hero, 8);
    {
        cweb_widget t;
        cweb_text_create(&t, "Latest stories");
        cweb_text_set_color(&t, (cweb_rgb){30, 30, 40});
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_font_size(&t, 32);
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_box_add_text(&hero, &t);
    }
    cweb_container_add(&main, &hero);

    /* Cards row */
    cweb_widget cards;
    cweb_container_create(&cards, CWEB_HORIZONTAL);
    cweb_box_set_size(&cards, 1.0f, 0);
    cweb_container_set_gap(&cards, 16);
    cweb_widget_set_direction_at(&cards, CWEB_MOBILE, CWEB_VERTICAL);

    /* Card 1 */
    cweb_widget card1;
    cweb_box_create(&card1);
    cweb_box_set_size(&card1, 0.5f, 0);
    cweb_box_set_bg(&card1, CWEB_CLR_WHITE);
    cweb_box_set_padding(&card1, cweb_pad(20));
    cweb_box_set_border_radius(&card1, 8);
    cweb_widget_set_link(&card1, "/article/1");
    cweb_widget_set_size_at(&card1, CWEB_MOBILE, 1.0f, 0);
    {
        cweb_widget t;
        cweb_text_create(&t, "AmazonAtlas — 11 October 2018");
        cweb_text_set_color(&t, (cweb_rgb){30, 30, 40});
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_font_size(&t, 20);
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&card1, &t);
    }
    {
        cweb_widget t;
        cweb_text_create(&t, "WikiLeaks publishes a Highly Confidential internal document from Amazon.");
        cweb_text_set_color(&t, (cweb_rgb){80, 80, 90});
        cweb_text_set_font_size(&t, 14);
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&card1, &t);
    }
    cweb_container_add(&cards, &card1);

    /* Card 2 */
    cweb_widget card2;
    cweb_box_create(&card2);
    cweb_box_set_size(&card2, 0.5f, 0);
    cweb_box_set_bg(&card2, CWEB_CLR_WHITE);
    cweb_box_set_padding(&card2, cweb_pad(20));
    cweb_box_set_border_radius(&card2, 8);
    cweb_widget_set_link(&card2, "/article/2");
    cweb_widget_set_size_at(&card2, CWEB_MOBILE, 1.0f, 0);
    {
        cweb_widget t;
        cweb_text_create(&t, "About cweb");
        cweb_text_set_color(&t, (cweb_rgb){30, 30, 40});
        cweb_text_set_style(&t, CWEB_TEXT_BOLD);
        cweb_text_set_font_size(&t, 20);
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&card2, &t);
    }
    {
        cweb_widget t;
        cweb_text_create(&t, "A minimal HTML UI framework in C. This page describes the project.");
        cweb_text_set_color(&t, (cweb_rgb){80, 80, 90});
        cweb_text_set_font_size(&t, 14);
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&card2, &t);
    }
    cweb_container_add(&cards, &card2);

    cweb_container_add(&main, &cards);

    /* Search input */
    cweb_widget search;
    cweb_box_create(&search);
    cweb_box_set_size(&search, 1.0f, 0);
    cweb_box_set_bg(&search, CWEB_CLR_WHITE);
    cweb_box_set_padding(&search, cweb_pad(16));
    cweb_box_set_border_radius(&search, 8);
    cweb_box_set_input(&search, NULL);
    cweb_box_set_placeholder(&search, "Search articles...");
    cweb_container_add(&main, &search);

    cweb_container_add(root, &main);

    /* Footer */
    cweb_widget footer;
    cweb_box_create(&footer);
    cweb_box_set_size(&footer, 1.0f, 0);
    cweb_box_set_bg(&footer, (cweb_rgb){33, 47, 61});
    cweb_box_set_padding(&footer, cweb_pad(16));
    {
        cweb_widget t;
        cweb_text_create(&t, "Built with cweb");
        cweb_text_set_color(&t, (cweb_rgb){200, 200, 220});
        cweb_text_set_font_size(&t, 13);
        cweb_text_set_style(&t, CWEB_TEXT_ITALIC);
        cweb_box_add_text(&footer, &t);
    }
    cweb_container_add(root, &footer);

    (void)app;
}

/* ---- Article page ---- */
static void build_article_page(cweb_app *app, cweb_widget *root) {
    const char *id_str = cweb_route_param(app, "id");
    int article_id = id_str ? atoi(id_str) : 1;

    cweb_container_create(root, CWEB_VERTICAL);
    cweb_box_set_size(root, 1.0f, 0);
    cweb_widget_set_min_height_vh(root, 100);
    cweb_container_set_bg(root, (cweb_rgb){245, 245, 248});
    cweb_container_set_padding(root, cweb_pad(24));
    cweb_container_set_gap(root, 24);

    /* Header with back link */
    cweb_widget header;
    cweb_box_create(&header);
    cweb_box_set_size(&header, 1.0f, 0);
    cweb_box_set_bg(&header, (cweb_rgb){33, 47, 61});
    cweb_box_set_padding(&header, cweb_pad(16));
    cweb_widget_set_sticky(&header, CWEB_STICKY_TOP);
    cweb_widget_set_z_index(&header, 100);
    {
        cweb_widget t;
        cweb_text_create(&t, "← back to cweb news");
        cweb_text_set_color(&t, (cweb_rgb){200, 200, 220});
        cweb_text_set_font_size(&t, 14);
        cweb_widget_set_link(&t, "/");
        cweb_box_add_text(&header, &t);
    }
    cweb_container_add(root, &header);

    /* Article card */
    cweb_widget article;
    cweb_box_create(&article);
    cweb_box_set_size(&article, 1.0f, 0);
    cweb_box_set_bg(&article, CWEB_CLR_WHITE);
    cweb_box_set_padding(&article, cweb_pad(32));
    cweb_box_set_border_radius(&article, 8);
    cweb_widget_set_padding_at(&article, CWEB_MOBILE, cweb_pad(16));

    if (article_id == 1) {
        cweb_widget t;
        cweb_text_create(&t, "AmazonAtlas\n11 October, 2018");
        cweb_text_set_color(&t, (cweb_rgb){20, 20, 30});
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_text_set_font_size(&t, 36);
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&article, &t);

        cweb_widget d;
        cweb_divider_create(&d);
        cweb_divider_set_color(&d, (cweb_rgb){100, 100, 110});
        cweb_widget_add_child(&article, &d);

        char *body = read_file("./assets/article_plain.txt");
        if (body) {
            cweb_widget bt;
            cweb_text_create(&bt, body);
            cweb_text_set_color(&bt, (cweb_rgb){30, 30, 40});
            cweb_text_set_font_family(&bt, "Georgia, serif");
            cweb_text_set_font_size(&bt, 18);
            cweb_text_set_wrap(&bt, 1);
            cweb_box_add_text(&article, &bt);
            free(body);
        }
    } else {
        cweb_widget t;
        cweb_text_create(&t, "About cweb");
        cweb_text_set_color(&t, (cweb_rgb){20, 20, 30});
        cweb_text_set_font_family(&t, "Georgia, serif");
        cweb_text_set_font_size(&t, 36);
        cweb_text_set_wrap(&t, 1);
        cweb_box_add_text(&article, &t);

        cweb_widget body;
        cweb_text_create(&body,
            "cweb is a minimal HTML UI framework written in C.\n\n"
            "It lets you build web pages using a widget tree — boxes, "
            "text, containers, images — and serves them over HTTP.\n\n"
            "Features: routes with path params, sticky headers/footers, "
            "responsive breakpoints, input fields with placeholders, "
            "image embedding, and natural page scrolling.");
        cweb_text_set_color(&body, (cweb_rgb){30, 30, 40});
        cweb_text_set_font_family(&body, "Georgia, serif");
        cweb_text_set_font_size(&body, 18);
        cweb_text_set_wrap(&body, 1);
        cweb_box_add_text(&article, &body);
    }
    cweb_container_add(root, &article);

    /* Inline SVG image */
    cweb_widget img;
    cweb_image_create(&img,
        "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='100' height='40'>"
        "<rect width='100' height='40' fill='%23333'/>"
        "<text x='50' y='25' font-family='monospace' font-size='14' "
        "fill='white' text-anchor='middle'>cweb</text></svg>");
    cweb_image_set_alt(&img, "cweb logo");
    cweb_container_add(root, &img);
}

/* ---- About page (alias for article 2) ---- */
static void build_about_page(cweb_app *app, cweb_widget *root) {
    build_article_page(app, root);
}

/* ---- 404 page ---- */
static void build_not_found_page(cweb_app *app, cweb_widget *root) {
    cweb_container_create(root, CWEB_VERTICAL);
    cweb_box_set_size(root, 1.0f, 1.0f);
    cweb_container_set_bg(root, (cweb_rgb){33, 47, 61});
    cweb_container_set_padding(root, cweb_pad(48));
    cweb_container_set_gap(root, 24);
    cweb_box_set_placement(root, CWEB_PLACE_CENTER);

    cweb_widget code;
    cweb_text_create(&code, "404");
    cweb_text_set_color(&code, (cweb_rgb){255, 100, 100});
    cweb_text_set_style(&code, CWEB_TEXT_BOLD);
    cweb_text_set_font_size(&code, 120);
    cweb_text_set_font_family(&code, "Georgia, serif");
    cweb_container_add(root, &code);

    cweb_widget msg;
    cweb_text_create(&msg, "This page got lost in the cloud.");
    cweb_text_set_color(&msg, (cweb_rgb){200, 200, 220});
    cweb_text_set_font_size(&msg, 18);
    cweb_text_set_wrap(&msg, 1);
    cweb_text_set_font_family(&msg, "Georgia, serif");
    cweb_container_add(root, &msg);

    cweb_widget link;
    cweb_text_create(&link, "← back to cweb news");
    cweb_text_set_color(&link, (cweb_rgb){100, 180, 255});
    cweb_text_set_font_size(&link, 16);
    cweb_widget_set_link(&link, "/");
    cweb_container_add(root, &link);

    (void)app;
}

int main(void) {
    cweb_app app;
    cweb_app_create(&app, "127.0.0.1", 8080);
    cweb_meta_set_title(&app, "cweb news — demo");
    cweb_meta_set_lang(&app, "en");
    cweb_meta_set_description(&app, "Multi-page news site built with cweb");

    cweb_app_add_route(&app, "/",            build_front_page);
    cweb_app_add_route(&app, "/article/:id", build_article_page);
    cweb_app_add_route(&app, "/about",       build_about_page);
    cweb_app_add_redirect(&app, "/old", "/about");
    cweb_app_set_not_found_handler(&app, build_not_found_page);

    fprintf(stderr, "Routes:\n");
    fprintf(stderr, "  /              — front page\n");
    fprintf(stderr, "  /article/1     — AmazonAtlas article\n");
    fprintf(stderr, "  /article/2     — about cweb\n");
    fprintf(stderr, "  /about         — about page\n");
    fprintf(stderr, "  /old           → redirects to /about\n");
    fprintf(stderr, "  /nonexistent   → custom 404 page\n");
    fprintf(stderr, "\nPress Ctrl+C to stop the server.\n");
    fprintf(stderr, "Open http://127.0.0.1:8080/ in your browser.\n");

    cweb_app_run(&app);
    cweb_app_destroy(&app);
    return 0;
}

char *read_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;
    if (fseek(fp, 0L, SEEK_END) != 0) { fclose(fp); return NULL; }
    long bufsize = ftell(fp);
    if (bufsize == -1) { fclose(fp); return NULL; }
    char *src = malloc((size_t)bufsize + 1);
    if (!src) { fclose(fp); return NULL; }
    fseek(fp, 0L, SEEK_SET);
    size_t n = fread(src, 1, (size_t)bufsize, fp);
    if (ferror(fp)) { free(src); fclose(fp); return NULL; }
    src[n] = '\0';
    fclose(fp);
    return src;
}
