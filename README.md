![[CWEB logo]](./assets/README/logo.png)

[![C](https://img.shields.io/badge/language-C-A8B9CC.svg)](https://www.iso.org/standard/82075.html)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![Version](https://img.shields.io/badge/version-0.0.1-orange.svg)
![Dependencies](https://img.shields.io/badge/dependencies-0-green.svg)

# CWEB

Want to build a web site, but you are the C developer. Well I got you...

With CWEB project you can:

- Generate *frontend* (HTML and CSS code) from C code directly.
- Link *input text areas* and *clicks* callbacks to C code
- Change the state and appereance of the current page from C code.

It also provides adaptive designs, so your pages will look normal even on mobile devices.

As example of simple "Hello-world" page:

```c
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
```

As you can see, no `malloc()` and `free()` used.
