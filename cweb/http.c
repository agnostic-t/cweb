#include "cweb/http.h"
#include "cweb/html.h"
#include "cweb/widget.h"
#include "cweb/app.h"
#include "cweb/box.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <malloc.h>  /* malloc_trim */


typedef struct {
    char    method[8];
    char    path[1024];
    char   *body;
    size_t  body_len;
} http_req;

static int parse_request(const char *raw, size_t raw_len, http_req *req) {
    memset(req, 0, sizeof(*req));

    /* Parse request line: METHOD SP PATH SP HTTP/1.1\r\n */
    const char *p = raw;
    const char *end = raw + raw_len;

    /* Method */
    const char *sp1 = memchr(p, ' ', end - p);
    if (!sp1) return -1;
    size_t mlen = sp1 - p;
    if (mlen >= sizeof(req->method)) return -1;
    memcpy(req->method, p, mlen);
    req->method[mlen] = '\0';

    /* Path: between first and second space */
    const char *sp2 = memchr(sp1 + 1, ' ', end - sp1 - 1);
    if (!sp2) return -1;
    size_t plen = sp2 - (sp1 + 1);
    if (plen >= sizeof(req->path)) plen = sizeof(req->path) - 1;
    memcpy(req->path, sp1 + 1, plen);
    req->path[plen] = '\0';

    /* Find end of headers (\r\n\r\n) */
    const char *body_marker = "\r\n\r\n";
    const char *body_start = memmem(raw, raw_len, body_marker, 4);
    if (!body_start) {
        /* No body — still a valid request if we have headers end */
        return 0;
    }
    body_start += 4;

    /* Look for Content-Length to know body size */
    const char *cl = strcasestr(raw, "Content-Length:");
    if (cl) {
        cl += strlen("Content-Length:");
        while (*cl == ' ' || *cl == '\t') cl++;
        long len = strtol(cl, NULL, 10);
        if (len > 0 && (size_t)len <= (size_t)(end - body_start)) {
            req->body = malloc(len + 1);
            memcpy(req->body, body_start, len);
            req->body[len] = '\0';
            req->body_len = len;
        }
    }
    return 0;
}

static void send_response(int fd, int status, const char *content_type,
                          const char *body, size_t body_len) {
    char header[512];
    const char *status_text = "OK";
    if (status == 301 || status == 302) status_text = "Moved";
    else if (status == 400) status_text = "Bad Request";
    else if (status == 404) status_text = "Not Found";
    else if (status == 500) status_text = "Internal Server Error";
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, content_type, body_len);
    write(fd, header, (size_t)hlen);
    write(fd, body, body_len);
}

/* Same as send_response but lets the caller specify a status code AND
   render the current app->root into HTML. Used by the 404 handler so the
   custom error page comes back with HTTP 404 (not 200).                    */
static void send_html_status(cweb_app *app, int fd, int status) {
    size_t html_len = 0;
    char *html = cweb_html_render(app, &html_len);
    char header[512];
    const char *status_text = "Not Found";
    if (status == 500) status_text = "Internal Server Error";
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, status_text, html_len);
    write(fd, header, (size_t)hlen);
    if (html) {
        write(fd, html, html_len);
        free(html);
    }
}

/* Send a 301 redirect. Location header is mandatory for redirects. */
static void send_redirect(int fd, const char *location) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 301 Moved\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n",
        location);
    write(fd, header, (size_t)hlen);
}

static int extract_query_int(const char *path, const char *key) {
    char needle[32];
    snprintf(needle, sizeof(needle), "%s=", key);
    const char *p = strstr(path, needle);
    if (!p) return -1;
    p += strlen(needle);
    return atoi(p);
}

/* Strip query string from path (in-place — safe because req->path is ours). */
static const char *path_only(char *path) {
    char *q = strchr(path, '?');
    if (q) *q = '\0';
    return path;
}

/* MIME type by file extension. Used by the static file handler. */
static const char *mime_for_path(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(dot, ".css")  == 0) return "text/css; charset=utf-8";
    if (strcmp(dot, ".js")   == 0) return "application/javascript; charset=utf-8";
    if (strcmp(dot, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(dot, ".png")  == 0) return "image/png";
    if (strcmp(dot, ".jpg")  == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".gif")  == 0) return "image/gif";
    if (strcmp(dot, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(dot, ".webp") == 0) return "image/webp";
    if (strcmp(dot, ".ico")  == 0) return "image/x-icon";
    if (strcmp(dot, ".txt")  == 0) return "text/plain; charset=utf-8";
    if (strcmp(dot, ".pdf")  == 0) return "application/pdf";
    return "application/octet-stream";
}

/* Serve a static file from ./static/ directory.
   Path "/static/foo.png" → file "./static/foo.png".
   Returns 1 if served, 0 if not found. Path traversal is blocked.       */
static int serve_static_file(int fd, const char *url_path) {
    if (strncmp(url_path, "/static/", 8) != 0) return 0;
    const char *rel = url_path + 8;
    /* Block path traversal */
    if (strstr(rel, "..") != NULL) return 0;

    char fs_path[1024];
    snprintf(fs_path, sizeof(fs_path), "./static/%s", rel);

    FILE *fp = fopen(fs_path, "rb");
    if (!fp) return 0;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size < 0) { fclose(fp); return 0; }

    char *body = malloc(size);
    if (!body) { fclose(fp); return 0; }
    fread(body, 1, size, fp);
    fclose(fp);

    const char *mime = mime_for_path(fs_path);
    send_response(fd, 200, mime, body, size);
    free(body);
    return 1;
}

/* Try to match a request path against the app's routes.
   Returns the matched route, or NULL. On match, app->param_* is filled. */
static const cweb_route *match_route(cweb_app *app, const char *path) {
    for (size_t i = 0; i < app->routes_count; i++) {
        if (cweb_route_match(app, &app->routes[i], path)) {
            return &app->routes[i];
        }
    }
    return NULL;
}

/* Try to match a redirect rule. Returns the target if matched, NULL otherwise. */
static const char *match_redirect(cweb_app *app, const char *path) {
    for (size_t i = 0; i < app->redirects_count; i++) {
        if (strcmp(app->redirects[i].from, path) == 0) {
            return app->redirects[i].to;
        }
    }
    return NULL;
}

/* Dispatch a request.
   - GET /static/ * → serve static file
   - GET /api/health → JSON health
   - POST /api/click?id=N → invoke callback
   - POST /api/input?id=N → update input buffer
   - GET <route> → render the matched route's builder output
   - GET / (no routes) → render app->root (legacy single-page mode)        */
static void handle_request(cweb_app *app, http_req *req, int fd) {
    /* Static file serving: /static/foo.png → ./static/foo.png */
    if (strncmp(req->path, "/static/", 8) == 0) {
        if (serve_static_file(fd, req->path)) return;
        send_response(fd, 404, "text/plain", "Not Found", 9);
        return;
    }

    /* Health check */
    if (strcmp(req->method, "GET") == 0 && strcmp(req->path, "/api/health") == 0) {
        send_response(fd, 200, "application/json", "{\"status\":\"ok\"}", 15);
        return;
    }

    /* Click callbacks (POST /api/click?id=N).
       These mutate the current page's state — they don't navigate.        */
    if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/api/click", 10) == 0) {
        int id = extract_query_int(req->path, "id");
        cweb_widget *w = cweb_widget_find(app->root, id);
        if (w && w->on_click) {
            cweb_event ev = {0};
            w->on_click(w, &ev);
        }
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        send_response(fd, 200, "text/html; charset=utf-8", html ? html : "", html_len);
        free(html);
        return;
    }

    /* Input callbacks (POST /api/input?id=N) */
    if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/api/input", 10) == 0) {
        int id = extract_query_int(req->path, "id");
        cweb_widget *w = cweb_widget_find(app->root, id);
        if (w && w->inputable) {
            cweb_input_set_value(w, req->body ? req->body : "");
            if (w->on_input) {
                cweb_event ev = {0};
                ev.value = req->body ? req->body : "";
                w->on_input(w, &ev);
            }
        }
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        send_response(fd, 200, "text/html; charset=utf-8", html ? html : "", html_len);
        free(html);
        return;
    }

    /* GET requests: check redirects first, then routes. */
    if (strcmp(req->method, "GET") == 0) {
        char path[1024];
        strncpy(path, req->path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        path_only(path);

        /* Redirects */
        const char *target = match_redirect(app, path);
        if (target) {
            send_redirect(fd, target);
            return;
        }

        /* Routes: if any are registered, match against them. */
        if (app->routes_count > 0) {
            const cweb_route *r = match_route(app, path);
            if (r) {
                /* Free previous root + registry (from previous request). */
                if (app->root) {
                    cweb_widget_free(app->root);
                    app->root = NULL;
                }
                free(app->registry);
                app->registry = NULL;
                app->registry_count = 0;
                app->registry_cap = 0;
                app->next_id = 0;

                /* Pre-allocate the root widget — the builder will
                   initialize it and add children. We own this malloc. */
                cweb_widget *root = malloc(sizeof(cweb_widget));
                if (!root) {
                    send_response(fd, 500, "text/plain", "out of memory", 13);
                    return;
                }
                /* Initialize to a sane default so the builder can call
                   cweb_container_create (which calls cweb_widget_init). */
                cweb_widget_init(root, CWEB_W_CONTAINER);
                app->root = root;

                /* Call the builder — it populates root. */
                r->builder(app, root);

                size_t html_len = 0;
                char *html = cweb_html_render(app, &html_len);
                if (html) {
                    send_response(fd, 200, "text/html; charset=utf-8", html, html_len);
                    free(html);
                } else {
                    send_response(fd, 500, "text/plain", "render error", 12);
                }
                return;
            }
            /* No route matched — try the custom 404 handler if registered. */
            if (app->not_found_handler) {
                if (app->root) {
                    cweb_widget_free(app->root);
                    app->root = NULL;
                }
                free(app->registry);
                app->registry = NULL;
                app->registry_count = 0;
                app->registry_cap = 0;
                app->next_id = 0;

                cweb_widget *root = malloc(sizeof(cweb_widget));
                if (!root) {
                    send_response(fd, 500, "text/plain", "out of memory", 13);
                    return;
                }
                cweb_widget_init(root, CWEB_W_CONTAINER);
                app->root = root;

                app->not_found_handler(app, root);
                send_html_status(app, fd, 404);
                return;
            }
            send_response(fd, 404, "text/plain", "Not Found", 9);
            return;
        }

        /* No routes registered — legacy single-page mode: render app->root
           for "/" and any other GET path.                                  */
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        if (html) {
            send_response(fd, 200, "text/html; charset=utf-8", html, html_len);
            free(html);
        } else {
            send_response(fd, 500, "text/plain", "render error", 12);
        }
        return;
    }

    send_response(fd, 404, "text/plain", "Not Found", 9);
}

int cweb_http_serve(cweb_app *app) {
    if (!app) return -1;

    /* Periodically return freed memory to the OS to avoid apparent
       memory growth from glibc's malloc arena caching.             */
    static int request_counter = 0;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("socket");
        return -1;
    }

    /* Allow quick rebind after restart */
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(app->port);
    if (inet_pton(AF_INET, app->host, &addr.sin_addr) != 1) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv);
        return -1;
    }

    if (listen(srv, 16) < 0) {
        perror("listen");
        close(srv);
        return -1;
    }

    fprintf(stderr, "[cweb] serving on http://%s:%d/\n", app->host, app->port);

    while (app->running) {
        struct sockaddr_in cli;
        socklen_t cli_len = sizeof(cli);
        int cli_fd = accept(srv, (struct sockaddr*)&cli, &cli_len);
        if (cli_fd < 0) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        /* Read request (single read — fine for our small POSTs) */
        char buf[65536];
        ssize_t n = read(cli_fd, buf, sizeof(buf) - 1);
        if (n < 0) {
            close(cli_fd);
            continue;
        }
        buf[n] = '\0';

        http_req req;
        if (parse_request(buf, (size_t)n, &req) == 0) {
            handle_request(app, &req, cli_fd);
        } else {
            send_response(cli_fd, 400, "text/plain", "Bad Request", 11);
        }

        free(req.body);
        close(cli_fd);

        /* Every 20 requests, return freed memory to the OS. glibc's
           malloc arena keeps freed blocks cached, which can look like a
           leak. malloc_trim(0) asks the allocator to release any free
           pages at the top of the heap back to the OS.                  */
        if (++request_counter % 20 == 0) {
            malloc_trim(0);
        }
    }

    close(srv);
    return 0;
}
