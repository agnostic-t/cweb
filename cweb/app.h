#ifndef CWEB_APP_H
#define CWEB_APP_H

#include "widget.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ *
 *  Page metadata                                                      *
 * ------------------------------------------------------------------ */
typedef struct {
    char *title;        /* <title> — defaults to "cweb app"           */
    char *favicon;      /* URL or data: URI for <link rel="icon">     */
    char *lang;         /* <html lang="...">  — defaults to "en"      */
    char *description;  /* <meta name="description">                 */
    char *extra_head;   /* raw HTML to inject at end of <head>        */
    char *body_class;   /* class attribute for <body>                */
} cweb_metadata;

/* ------------------------------------------------------------------ *
 *  Routes                                                             *
 *                                                                    *
 *  A route is a URL path → C builder function. When the browser       *
 *  requests the path, the builder runs and produces a widget tree,    *
 *  which the server renders to HTML and returns.                     *
 *                                                                    *
 *  Path patterns:                                                    *
 *    "/about"      — exact match                                     *
 *    "/article/:id" — path parameter, available via cweb_route_param  *
 *                                                                    *
 *  Redirects:                                                        *
 *    cweb_redirect(&app, "/old", "/new")                              *
 *  emits a 301 redirect from /old to /new.                            *
 * ------------------------------------------------------------------ */
typedef struct cweb_app cweb_app;

/* Route builder function. Receives:
   - app: the app (so it can read state, route params, etc.)
   - root: a pre-allocated, pre-owned root widget. The builder must
     initialize it (e.g. cweb_container_create(root, CWEB_VERTICAL))
     and add children to it. Children should be stack-allocated and
     added via cweb_container_add / cweb_box_add_text — these deep-copy
     the child into the tree AND release the original's heap strings in
     the same call (consuming adds), so builders need NO manual cleanup
     at all.
     The builder must NOT call cweb_app_set_root — root is already set. */
typedef void (*cweb_route_builder)(cweb_app *app, cweb_widget *root);

typedef struct {
    char             *pattern;   /* e.g. "/article/:id"               */
    cweb_route_builder builder; /* function that builds the page       */
} cweb_route;

typedef struct {
    char *from;   /* e.g. "/old" */
    char *to;     /* e.g. "/new" */
} cweb_redirect_rule;

/* ------------------------------------------------------------------ *
 *  Sessions — one isolated widget tree + user state per visitor       *
 *                                                                    *
 *  The first time a browser hits the server it gets a `cweb_sid`      *
 *  cookie. The cookie maps to a cweb_session holding:                *
 *    - its OWN clone of the widget tree (legacy set_root mode) or    *
 *      its own builder-produced tree (route mode), and               *
 *    - a `state` pointer produced by the app's state_new callback.   *
 *  Callbacks receive that state, so two users clicking the same      *
 *  button mutate two different trees: user A never sees user B's     *
 *  changes.                                                          *
 *                                                                    *
 *  Sessions are capped (LRU eviction by last use, default 64) so     *
 *  cookie-less clients (curl, bots) cannot grow memory forever.      *
 *  Everything here is single-threaded by design (one accept loop).   *
 * ------------------------------------------------------------------ */
typedef struct cweb_session cweb_session;

struct cweb_session {
    char          sid[33];        /* 32 hex chars + NUL, from cookie    */
    cweb_widget  *root;           /* THIS visitor's live widget tree    */
    void         *state;          /* from state_new, freed by state_free*/
    char         *page_key;       /* route mode: path that built root   */

    /* Per-session widget id registry (same role as the app-level one) */
    cweb_widget **registry;
    size_t        registry_count;
    size_t        registry_cap;
    int           next_id;

    unsigned long long last_used;  /* monotonic counter, for LRU eviction */
};

/* State lifecycle hooks. state_new() allocates one user state block per
   session (return NULL to run with state == NULL); state_free() releases
   it when the session is evicted or the app is destroyed. Example:

     typedef struct { int clicks; } counter;
     static void *counter_new(void) { return calloc(1, sizeof(counter)); }
     static void  counter_free(void *s) { free(s); }
     cweb_app_set_state_callbacks(&app, counter_new, counter_free);     */
typedef void *(*cweb_state_new_fn)(void);
typedef void  (*cweb_state_free_fn)(void *state);

/* ------------------------------------------------------------------ *
 *  App                                                                *
 * ------------------------------------------------------------------ */
typedef struct cweb_app {
    char           host[64];       /* "127.0.0.1" by default */
    int            port;           /* 8080 by default        */
    cweb_widget   *root;           /* PROTOTYPE tree: legacy-mode pages   */
    cweb_metadata  meta;
    int            running;        /* set to 0 to stop      */

    /* Internal: registry of the CURRENT tree being rendered. Used only
       when no session is active (--html mode, custom 404 rendering);
       per-visitor trees register ids into their cweb_session instead. */
    cweb_widget  **registry;
    size_t         registry_count;
    size_t         registry_cap;
    int            next_id;

    /* Per-visitor sessions (see struct cweb_session above). cur_sess
       points at the session whose tree is being rendered/dispatched
       right now; NULL outside request handling. Internal.            */
    cweb_session **sessions;
    size_t         sessions_count;
    size_t         sessions_cap;
    size_t         max_sessions;      /* LRU cap, default 64            */
    unsigned long long req_counter;   /* monotonic, feeds last_used     */
    cweb_session  *cur_sess;

    /* Per-session state factory/destructor (NULL = state stays NULL). */
    cweb_state_new_fn  state_new;
    cweb_state_free_fn state_free;

    /* Internal: render cache. */
    char          *html_cache;
    size_t         html_cache_len;

    /* Routes: list of (pattern, builder) pairs. */
    cweb_route    *routes;
    size_t         routes_count;
    size_t         routes_cap;

    /* Redirects: list of (from, to) pairs. */
    cweb_redirect_rule *redirects;
    size_t         redirects_count;
    size_t         redirects_cap;

    /* Custom 404 handler. If NULL, a plain-text "Not Found" is returned. */
    cweb_route_builder not_found_handler;

    /* Current request: matched route's path params. For URL "/article/42"
       matched against pattern "/article/:id", params[":id"] = "42".    */
    char         **param_names;
    char         **param_values;
    size_t         param_count;

    /* User state (free-form). The app doesn't touch this; routes can
       store whatever they want here. Useful for sharing DB handles,
       cached data, etc.                                                  */
    void          *user_data;
} cweb_app;

/* Lifecycle */
int  cweb_app_create(cweb_app *app, const char *host, int port);
void cweb_app_destroy(cweb_app *app);

/* Set the root widget. CONSUMING: the app makes a DEEP COPY of `root`
   into its own heap tree and, in the same call, releases the original's
   heap members (strings, child clones) — whether it lives on the stack
   or on the heap. No further cleanup is required from the caller;
   cweb_app_destroy frees the app's copy and never touches your
   original. On failure (-1) the original stays intact. */
int  cweb_app_set_root(cweb_app *app, cweb_widget *root);

/* Register a route. The builder will be called whenever a request matches
   the pattern. Patterns support path params: "/article/:id".              */
int  cweb_app_add_route(cweb_app *app, const char *pattern,
                         cweb_route_builder builder);

/* Register a redirect: requests to `from` get a 301 to `to`. */
int  cweb_app_add_redirect(cweb_app *app, const char *from, const char *to);

/* Register a custom 404 handler. The builder will be called for any
   GET request that doesn't match a route or redirect. If not set, a
   plain-text "Not Found" is returned.                                    */
int  cweb_app_set_not_found_handler(cweb_app *app, cweb_route_builder builder);

/* Inside a route builder, look up a path param by name (without the colon).
   Returns NULL if not found. Example: for pattern "/article/:id" and URL
   "/article/42", cweb_route_param(app, "id") returns "42".                  */
const char *cweb_route_param(cweb_app *app, const char *name);

/* Internal — used by http.c to dispatch requests. Not for user code. */
int  cweb_route_match       (cweb_app *app, const cweb_route *route, const char *path);
void cweb_route_params_free (cweb_app *app);

/* Metadata setters (each takes a copy of the string). */
void cweb_meta_set_title      (cweb_app *app, const char *s);
void cweb_meta_set_favicon    (cweb_app *app, const char *s);
void cweb_meta_set_lang       (cweb_app *app, const char *s);
void cweb_meta_set_description(cweb_app *app, const char *s);
void cweb_meta_set_extra_head (cweb_app *app, const char *s);
void cweb_meta_set_body_class (cweb_app *app, const char *s);

/* Run the HTTP server. Blocks until cweb_app_stop() is called (e.g.
   from a callback or signal).                                          */
int  cweb_app_run(cweb_app *app);

/* Signal the event loop to stop. Safe to call from any thread. */
void cweb_app_stop(cweb_app *app);

/* Per-session state factory (see cweb_state_new_fn). Call BEFORE
   cweb_app_run; every session created afterwards gets state_new()'d
   state, and its callback signature is:
       int on_click(cweb_widget *w, cweb_event *ev, void *state);      */
void cweb_app_set_state_callbacks(cweb_app *app,
                                  cweb_state_new_fn state_new,
                                  cweb_state_free_fn state_free);

/* Sessions ----------------------------------------------------------- */
/* Get (or lazily create) the session for `sid`. sid == NULL or an
   unknown id creates a fresh session (new random sid, tree seeded from
   the prototype root in legacy mode, state via state_new). *is_new is
   set to 1 when the session was just created. Never returns NULL
   (fatal exit on OOM). Used internally by http.c.                     */
cweb_session *cweb_app_session_get(cweb_app *app, const char *sid,
                                   int *is_new);
/* Destroy one session: runs state_free, frees the tree, removes it
   from the app's list.                                                */
void          cweb_app_session_destroy(cweb_app *app, cweb_session *sess);
/* Free ALL sessions (called by cweb_app_destroy).                     */
void          cweb_app_sessions_free(cweb_app *app);
/* Current number of live sessions.                                    */
size_t        cweb_app_session_count(cweb_app *app);
/* LRU cap on live sessions (default 64). When exceeded, the least
   recently used session is destroyed before a new one is created.     */
void          cweb_app_set_max_sessions(cweb_app *app, size_t max_n);

/* Force a re-render of the HTML cache. Called automatically when a
   request mutates the tree (e.g. a click callback changes a color). */
void cweb_app_invalidate(cweb_app *app);

/* Render the current tree to a freshly allocated HTML string.
   Caller must free() the result. Useful for testing or for the
   --html mode that just dumps a snapshot.                            */
char *cweb_app_render_html(cweb_app *app);

#ifdef __cplusplus
}
#endif

#endif /* CWEB_APP_H */
