/* Demo app — showcases all cweb features in a single page. */

#include "cweb/app.h"
#include "cweb/widget.h"
#include "cweb/box.h"
#include "cweb/text.h"
#include "cweb/container.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---- Click callback: turns the box black ---- */
int on_click(cweb_widget *w, cweb_event *ev) {
    (void)ev;
    cweb_box_set_bg(w, 0, 0, 0);
    return 0;
}

/* ---- Input callback: when value is "Hi", replaces with "Bye" ---- */
int on_input(cweb_widget *w, cweb_event *ev) {
    (void)ev;
    if (strcmp(cweb_input_get_value(w), "Hi") == 0) {
        cweb_input_set_value(w, "Bye");
    }
    return 0;
}

int main(int argc, char **argv) {
    cweb_app app;
    cweb_app_create(&app, "127.0.0.1", 8080);

    cweb_widget root;
    cweb_container_create(&root, CWEB_VERTICAL);
    cweb_container_set_gap(&root, 12);
    cweb_container_set_padding(&root, 24);

    cweb_widget header;
    cweb_box_create(&header);
    cweb_box_set_size(&header, 1.0f, 0.1f);
    cweb_box_set_placement(&header, CWEB_PLACE_CENTER);

    cweb_widget header_text;
    cweb_text_create(&header_text, "cweb demo");
    cweb_text_set_font_size(&header_text, 28);
    cweb_box_add_text(&header, &header_text);

    cweb_container_add(&root, &header);
    cweb_app_set_root(&app, &root);

    cweb_app_run(&app);
    cweb_app_destroy(&app);
    return 0;
}
