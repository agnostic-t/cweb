#include "cweb/app.h"
#include "cweb/widget.h"
#include "cweb/html.h"
#include "cweb/http.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

/* ------------------------------------------------------------------ *
 *  Signal handling                                                    *
 *                                                                    *
 *  We use sigaction() (not signal()) because on Linux glibc's         *
 *  signal() sets SA_RESTART, which means blocking syscalls like       *
 *  accept() are auto-restarted instead of returning EINTR. That        *
 *  makes Ctrl+C unable to break out of accept() — the signal handler  *
 *  runs, sets running=0, but accept() keeps blocking.                 *
 *                                                                    *
 *  With sigaction() and sa_flags = 0 (no SA_RESTART), accept()        *
 *  returns -1 with errno=EINTR, our loop's "if (errno == EINTR)        *
 *  continue;" fires, the while(app->running) check returns false, and  *
 *  the server exits cleanly.                                          *
 * ------------------------------------------------------------------ */
static cweb_app *g_app_for_sigint = NULL;

static void on_sigint(int sig) {
    (void)sig;
    if (g_app_for_sigint) {
        g_app_for_sigint->running = 0;
    }
}

static void install_signal_handlers(cweb_app *app) {
    g_app_for_sigint = app;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;  /* NO SA_RESTART — accept() must return EINTR */
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* Ignore SIGPIPE — happens when client disconnects mid-write */
    signal(SIGPIPE, SIG_IGN);
}

/* ------------------------------------------------------------------ *
 *  Forward decl from widget.c (registry helper)                       *
 * ------------------------------------------------------------------ */
void cweb_app_register_tree(cweb_app *app, cweb_widget *root);

/* ------------------------------------------------------------------ *
 *  Lifecycle                                                          *
 * ------------------------------------------------------------------ */
int cweb_app_create(cweb_app *app, const char *host, int port) {
    if (!app) return -1;
    memset(app, 0, sizeof(*app));

    if (host) {
        strncpy(app->host, host, sizeof(app->host) - 1);
    } else {
        strncpy(app->host, "127.0.0.1", sizeof(app->host) - 1);
    }
    app->port = port > 0 ? port : 8080;
    app->running = 0;

    /* Default metadata */
    app->meta.lang = strdup("en");

    return 0;
}

void cweb_app_destroy(cweb_app *app) {
    if (!app) return;
    if (app->root) {
        cweb_widget_free(app->root);
        app->root = NULL;
    }
    free(app->registry);
    free(app->html_cache);
    free(app->meta.title);
    free(app->meta.favicon);
    free(app->meta.lang);
    free(app->meta.description);
    free(app->meta.extra_head);
    free(app->meta.body_class);
    for (size_t i = 0; i < app->routes_count; i++) {
        free(app->routes[i].pattern);
    }
    free(app->routes);
    for (size_t i = 0; i < app->redirects_count; i++) {
        free(app->redirects[i].from);
        free(app->redirects[i].to);
    }
    free(app->redirects);
    cweb_route_params_free(app);
}

int cweb_app_set_root(cweb_app *app, cweb_widget *root) {
    if (!app || !root) return -1;
    /* Free previous root if any (e.g. from a previous request). */
    if (app->root) {
        cweb_widget_free(app->root);
    }
    /* Reset registry — the new tree will be re-registered on render. */
    free(app->registry);
    app->registry = NULL;
    app->registry_count = 0;
    app->registry_cap = 0;
    app->next_id = 0;
    /* Deep-copy the caller's widget into an app-owned heap tree.
       This makes BOTH usages safe:
         - stack-allocated root: storing its address would make the later
           cweb_app_destroy → free() an invalid free (UB / ASAN abort);
         - heap-allocated root: no risk of double-free if the caller also
           frees their copy.
       CONSUMING: on success the original's own heap members are released
       right here — for stack and heap roots alike, so callers have
       nothing to dispose afterwards. On failure (-1) the original is
       left intact and may be reused.                                     */
    app->root = cweb_widget_clone(root);
    if (!app->root) return -1;
    cweb_widget_free_contents(root);
    return 0;
}

int cweb_app_add_route(cweb_app *app, const char *pattern,
                       cweb_route_builder builder) {
    if (!app || !pattern || !builder) return -1;
    if (app->routes_count == app->routes_cap) {
        size_t nc = app->routes_cap ? app->routes_cap * 2 : 8;
        cweb_route *nr = realloc(app->routes, nc * sizeof(cweb_route));
        if (!nr) return -1;
        app->routes = nr;
        app->routes_cap = nc;
    }
    app->routes[app->routes_count].pattern = strdup(pattern);
    app->routes[app->routes_count].builder = builder;
    app->routes_count++;
    return 0;
}

int cweb_app_add_redirect(cweb_app *app, const char *from, const char *to) {
    if (!app || !from || !to) return -1;
    if (app->redirects_count == app->redirects_cap) {
        size_t nc = app->redirects_cap ? app->redirects_cap * 2 : 8;
        cweb_redirect_rule *nr = realloc(app->redirects,
                                         nc * sizeof(cweb_redirect_rule));
        if (!nr) return -1;
        app->redirects = nr;
        app->redirects_cap = nc;
    }
    app->redirects[app->redirects_count].from = strdup(from);
    app->redirects[app->redirects_count].to   = strdup(to);
    app->redirects_count++;
    return 0;
}

int cweb_app_set_not_found_handler(cweb_app *app, cweb_route_builder builder) {
    if (!app) return -1;
    app->not_found_handler = builder;
    return 0;
}

const char *cweb_route_param(cweb_app *app, const char *name) {
    if (!app || !name) return NULL;
    for (size_t i = 0; i < app->param_count; i++) {
        if (strcmp(app->param_names[i], name) == 0) {
            return app->param_values[i];
        }
    }
    return NULL;
}

/* Internal: match a URL path against a route pattern. Returns 1 on match,
   0 on no match. On match, fills app->param_names/param_values with the
   extracted params (caller frees them via cweb_route_params_free).       */
int cweb_route_match(cweb_app *app, const cweb_route *route, const char *path) {
    /* Free previous params */
    cweb_route_params_free(app);

    const char *p = path;
    const char *pat = route->pattern;

    while (*pat && *p) {
        if (*pat == ':') {
            /* Path param: read until next '/' in pattern */
            const char *pat_end = pat + 1;
            while (*pat_end && *pat_end != '/') pat_end++;
            size_t name_len = pat_end - pat - 1;

            /* Read value until next '/' in path */
            const char *val_end = p;
            while (*val_end && *val_end != '/') val_end++;
            size_t val_len = val_end - p;

            /* Store param */
            app->param_count++;
            app->param_names  = realloc(app->param_names,
                                         app->param_count * sizeof(char*));
            app->param_values = realloc(app->param_values,
                                         app->param_count * sizeof(char*));
            char *nm = malloc(name_len + 1);
            memcpy(nm, pat + 1, name_len);
            nm[name_len] = '\0';
            char *vl = malloc(val_len + 1);
            memcpy(vl, p, val_len);
            vl[val_len] = '\0';
            app->param_names[app->param_count - 1]  = nm;
            app->param_values[app->param_count - 1] = vl;

            pat = pat_end;
            p = val_end;
        } else if (*pat == *p) {
            pat++;
            p++;
        } else {
            return 0;
        }
    }
    /* Both must be at end (or pattern has trailing slash, ignore for now) */
    if (*pat == '\0' && *p == '\0') return 1;
    if (*pat == '/' && *(pat+1) == '\0' && *p == '\0') return 1;
    return *pat == *p ? 1 : 0;
}

void cweb_route_params_free(cweb_app *app) {
    if (!app) return;
    for (size_t i = 0; i < app->param_count; i++) {
        free(app->param_names[i]);
        free(app->param_values[i]);
    }
    free(app->param_names);
    free(app->param_values);
    app->param_names = NULL;
    app->param_values = NULL;
    app->param_count = 0;
}

/* ------------------------------------------------------------------ *
 *  Metadata setters                                                   *
 * ------------------------------------------------------------------ */
#define META_SETTER(field) \
    void cweb_meta_set_##field(cweb_app *app, const char *s) { \
        if (!app) return; \
        free(app->meta.field); \
        app->meta.field = s ? strdup(s) : NULL; \
    }

META_SETTER(title)
META_SETTER(favicon)
META_SETTER(lang)
META_SETTER(description)
META_SETTER(extra_head)
META_SETTER(body_class)

/* ------------------------------------------------------------------ *
 *  Rendering + invalidation                                           *
 * ------------------------------------------------------------------ */
char *cweb_app_render_html(cweb_app *app) {
    if (!app) return NULL;
    size_t len = 0;
    return cweb_html_render(app, &len);
}

void cweb_app_invalidate(cweb_app *app) {
    if (!app) return;
    free(app->html_cache);
    app->html_cache = NULL;
    app->html_cache_len = 0;
}

/* ------------------------------------------------------------------ *
 *  Run                                                                *
 * ------------------------------------------------------------------ */

int cweb_app_run(cweb_app *app) {
    if (!app) return -1;
    /* Need either a pre-set root (legacy single-page mode) or at least
       one route (multi-page mode). If neither, there's nothing to serve. */
    if (!app->root && app->routes_count == 0) {
        fprintf(stderr, "[cweb] error: no root widget and no routes registered.\n");
        return -1;
    }
    app->running = 1;

    install_signal_handlers(app);

    int rc = cweb_http_serve(app);

    /* Reset signal handlers to default on exit so a second Ctrl+C
       (e.g. if the server is hung in a callback) kills the process. */
    signal(SIGINT,  SIG_DFL);
    signal(SIGTERM, SIG_DFL);

    return rc;
}

void cweb_app_stop(cweb_app *app) {
    if (!app) return;
    app->running = 0;
}
