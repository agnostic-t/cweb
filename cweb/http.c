#include "cweb/http.h"
#include "cweb/html.h"
#include "cweb/widget.h"
#include "cweb/app.h"
#include "cweb/box.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strncasecmp */
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>  /* struct timeval for SO_RCVTIMEO/SO_SNDTIMEO */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <limits.h>    /* PATH_MAX */
#include <malloc.h>  /* malloc_trim */

/* ------------------------------------------------------------------ *
 *  Minimal HTTP/1.1 server on raw sockets.                            *
 *                                                                    *
 *  For MVP we support:                                                *
 *    - GET /                  → full HTML page (per-session tree)    *
 *    - POST /api/click?id=N   → invoke on_click(w, ev, state), return *
 *                               the session's re-rendered page       *
 *    - POST /api/input?id=N   → invoke on_input(w, ev, state), return *
 *                               the session's re-rendered page       *
 *                                                                    *
 *  Sessions: the first visit gets a `cweb_sid` cookie (Set-Cookie,    *
 *  HttpOnly, SameSite=Lax). The sid maps to a cweb_session holding    *
 *  the visitor's OWN widget tree and state block, so concurrent       *
 *  users never see each other's mutations. Static files and          *
 *  /api/health are stateless and never create sessions. Sessions      *
 *  are capped with LRU eviction (see cweb_app_set_max_sessions).      *
 *                                                                    *
 *  Robustness contract (hardened):                                   *
 *    - The request is received by a LOOPED read with a per-socket     *
 *      idle timeout (SO_RCVTIMEO/SO_SNDTIMEO). A stalled/slowloris   *
 *      client gets 408 and frees the single-threaded server instead  *
 *      of blocking it forever on read().                              *
 *    - Header block is capped at CWEB_HTTP_MAX_HEADER_BYTES (→ 431),  *
 *      declared body is capped at CWEB_HTTP_MAX_BODY_BYTES (→ 413).   *
 *      The 413 fires as soon as the over-limit Content-Length header  *
 *      is parsed — no need to wait for the huge body first.            *
 *    - Body is READ TO COMPLETION per Content-Length (verified against*
 *      the amount of data promised) before dispatch; requests are no   *
 *      longer truncated by the old single 64 KB read().                *
 *    - Content-Length is taken ONLY from the header section, never    *
 *      matched inside the body text.                                   *
 *    - Partial writes handled by send_all() with MSG_NOSIGNAL          *
 *      (SIGPIPE is additionally ignored globally in app startup).      *
 *    - Static files are served only if realpath() keeps them under     *
 *      ./static/ — blocks traversal and symlink escape.               *
 *                                                                    *
 *  Known limitations (by design for MVP):                             *
 *    - No Transfer-Encoding: chunked support; bodies without         *
 *      Content-Length are treated as empty.                            *
 *    - One request per connection (Connection: close always).          *
 *    - Single-threaded accept loop: requests are served serially.      *
 *                                                                    *
 *  For production use, swap this file out for an implementation      *
 *  based on libmicrohttpd — the public API (cweb_http_serve) stays   *
 *  the same.                                                          *
 * ------------------------------------------------------------------ */

/* ---- Hardening limits -------------------------------------------------
 * These are intentionally internal to this translation unit: they shape
 * the transport layer only and do not affect the public API.
 *                                                                       */
#define CWEB_HTTP_MAX_HEADER_BYTES (32u * 1024u)   /* max header block   */
#define CWEB_HTTP_MAX_BODY_BYTES   (1024u * 1024u) /* 1 MiB body ceiling */
#define CWEB_HTTP_IO_TIMEOUT_SEC   8               /* idle socket timeout*/

typedef struct {
    char    method[8];
    char    path[1024];
    char   *body;
    size_t  body_len;
    char    sid[33];      /* cweb_sid cookie value (empty = none/invalid) */
} http_req;

/* ------------------------------------------------------------------ *
 *  Hardened request receive                                           *
 * ------------------------------------------------------------------ */

typedef enum {
    RR_OK = 0,
    RR_BAD_REQUEST,        /* malformed or incomplete request       */
    RR_HEADERS_TOO_LARGE,  /* header block > CWEB_HTTP_MAX_HEADER_BYTES */
    RR_BODY_TOO_LARGE,     /* declared Content-Length over the cap  */
    RR_TIMEOUT,            /* stalled client (slowloris protection) */
    RR_DISCONNECTED        /* peer closed/errors before any reply   */
} recv_status;

/* Apply idle timeouts so a silent client can never block the
 * single-threaded server forever. Called once per accepted socket. */
static void set_client_timeouts(int fd) {
    struct timeval tv;
    tv.tv_sec  = CWEB_HTTP_IO_TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/* Best-effort full write with MSG_NOSIGNAL. Returns 0 when all bytes
 * were pushed, -1 if the peer went away / timed out mid-write (the
 * connection is dead then — the caller just abandons it).           */
static int send_all(int fd, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(fd, data + off, len - off, MSG_NOSIGNAL);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return -1;
        }
        off += (size_t)n;
    }
    return 0;
}

/* Grow a heap buffer geometrically. Returns 0 on success, -1 on OOM. */
static int grow_buf(char **buf, size_t *cap, size_t need) {
    if (*buf && *cap >= need) return 0;
    size_t nc = (*cap >= 8192u) ? *cap : 8192u;
    while (nc < need) nc *= 2u;
    char *nb = realloc(*buf, nc);
    if (!nb) return -1;
    *buf = nb;
    *cap = nc;
    return 0;
}

/* Extract Content-Length from the HEADER section only.
 * Scans lines case-insensitively; duplicate headers must agree,
 * otherwise returns LLONG_MIN to signal ambiguity.
 * Returns -1 when no valid header is present.                        */
static long long get_content_length(const char *head, size_t head_len) {
    long long val = -1;
    const char *p    = head;
    const char *end  = head + head_len;
    while (p < end) {
        /* Find end of this line (exclusive of CRLF). */
        const char *eol = memmem(p, (size_t)(end - p), "\r\n", 2);
        size_t linelen = eol ? (size_t)(eol - p) : (size_t)(end - p);
        if (linelen >= 15 && strncasecmp(p, "content-length:", 15) == 0) {
            const char *q  = p + 15;
            const char *le = p + linelen;
            while (q < le && (*q == ' ' || *q == '\t')) q++;
            if (q >= le || *q < '0' || *q > '9') {
                val = -1;             /* no digits → treat as absent */
                break;
            }
            long long v = strtoll(q, NULL, 10);
            if (v < 0) v = -1;
            if (val != -1 && val != v)
                return LLONG_MIN;     /* conflicting duplicates      */
            val = v;
        }
        if (!eol) break;
        p = eol + 2;
    }
    return val;
}

#define CWEB_CL_AMBIGUOUS (LLONG_MIN)

/* Extract the cweb_sid cookie from the HEADER section only.
 * Accepts strictly [0-9a-f]{1,32} (our own sid format); anything else
 * is treated as "no cookie" → the caller creates a fresh session.
 * Cookie attributes after ';' and other cookies are ignored.          */
static void extract_session_sid(const char *head, size_t head_len,
                                char out_sid[33]) {
    out_sid[0] = '\0';
    const char *p   = head;
    const char *end = head + head_len;
    while (p < end) {
        const char *eol = memmem(p, (size_t)(end - p), "\r\n", 2);
        size_t linelen = eol ? (size_t)(eol - p) : (size_t)(end - p);
        if (linelen >= 7 && strncasecmp(p, "cookie:", 7) == 0) {
            const char *q  = p + 7;
            const char *le = p + linelen;
            while (q < le && (*q == ' ' || *q == '\t')) q++;
            /* scan cookie pairs separated by ';' */
            for (const char *s = q; s + 9 <= le; s++) {
                if (strncasecmp(s, "cweb_sid=", 9) == 0) {
                    const char *v = s + 9;
                    size_t n = 0;
                    while (v < le && *v != ';' && *v != ' ' && n < 32) {
                        char c = *v;
                        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                            return;            /* malformed → no session  */
                        out_sid[n++] = c;
                        v++;
                    }
                    out_sid[n] = '\0';
                    return;
                }
            }
        }
        if (!eol) break;
        p = eol + 2;
    }
}

/* Read one full HTTP request: looped read() until "\r\n\r\n" (headers),
 * then exactly Content-Length more body bytes. Enforces caps and
 * applies socket timeouts; never lets a client stall the server.
 *
 * On RR_OK the caller receives a NUL-terminated buffer in *out_buf
 * (heap, free() when done), total byte count in *out_len and the
 * offset of the body start (header block size incl. CRLFCRLF) in
 * *head_len_out.                                                     */
static recv_status recv_request(int fd, char **out_buf, size_t *out_len,
                                size_t *head_len_out) {
    char   *buf = NULL;
    size_t  cap = 0, len = 0;

    /* ---- Phase 1: read until end of headers ----------------------- */
    for (;;) {
        if (grow_buf(&buf, &cap, len + 4096) != 0) { free(buf); return RR_DISCONNECTED; }
        ssize_t n = read(fd, buf + len, cap - len - 1); /* keep room for '\0' */
        if (n > 0) {
            len += (size_t)n;
            if (memmem(buf, len, "\r\n\r\n", 4)) break;
            if (len > CWEB_HTTP_MAX_HEADER_BYTES) { free(buf); return RR_HEADERS_TOO_LARGE; }
        } else if (n == 0) {
            free(buf);
            return len ? RR_BAD_REQUEST : RR_DISCONNECTED;
        } else {
            if (errno == EINTR) continue;
            free(buf);
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)
                return RR_TIMEOUT;
            return RR_DISCONNECTED;
        }
    }

    char *he = memmem(buf, len, "\r\n\r\n", 4);         /* guaranteed non-NULL here */
    size_t head_len = (size_t)(he - buf) + 4;

    /* ---- Validate declared length BEFORE waiting for any body ------ */
    long long cl = get_content_length(buf, head_len);
    if (cl == CWEB_CL_AMBIGUOUS) { free(buf); return RR_BAD_REQUEST; }
    if (cl > (long long)CWEB_HTTP_MAX_BODY_BYTES) { free(buf); return RR_BODY_TOO_LARGE; }

    size_t want_total = head_len + ((cl > 0) ? (size_t)cl : 0u);

    /* ---- Phase 2: read the promised body to completion ------------- */
    while (len < want_total) {
        if (grow_buf(&buf, &cap, want_total + 1) != 0) { free(buf); return RR_DISCONNECTED; }
        ssize_t n = read(fd, buf + len, want_total - len);
        if (n > 0) {
            len += (size_t)n;
        } else if (n == 0) {
            free(buf);
            return RR_BAD_REQUEST;   /* EOF mid-body → incomplete */
        } else {
            if (errno == EINTR) continue;
            free(buf);
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT)
                return RR_TIMEOUT;
            return RR_DISCONNECTED;
        }
    }

    buf[len] = '\0';
    *out_buf       = buf;
    *out_len       = len;
    *head_len_out  = head_len;
    return RR_OK;
}

static int parse_request(const char *raw, size_t raw_len, size_t head_len,
                         http_req *req) {
    memset(req, 0, sizeof(*req));

    /* Parse request line: METHOD SP PATH SP HTTP/1.1 CRLF */
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

    /* Body starts right after the header block. recv_request() has
       already validated and fully received the declared length.      */
    const char *body_start = (head_len > 0 && head_len <= raw_len)
                               ? raw + head_len : NULL;

    /* Session cookie from the header section (never from the body). */
    extract_session_sid(raw, head_len, req->sid);

    /* Content-Length from the HEADER slice only — a value written into
       the body text must not be able to fake the real length.
       Conflicting duplicate headers were already rejected at recv time
       (RR_BAD_REQUEST), so a clean request yields one unambiguous CL. */
    long long cl = get_content_length(raw, head_len);
    if (body_start && cl > 0) {
        size_t avail = (size_t)(end - body_start);
        size_t take  = ((size_t)cl <= avail) ? (size_t)cl : avail;
        req->body = malloc(take + 1);
        if (!req->body) return -1;
        memcpy(req->body, body_start, take);
        req->body[take] = '\0';
        req->body_len = take;
    }
    return 0;
}

/* Core response sender. `extra_headers` (may be NULL) is injected verbatim
   before the final CRLF — used for Set-Cookie on session-creating
   responses. Every header string passed here MUST already end with CRLF. */
static void send_response_ex(int fd, int status, const char *content_type,
                             const char *body, size_t body_len,
                             const char *extra_headers) {
    char header[640];
    const char *status_text = "OK";
    if (status == 301 || status == 302) status_text = "Moved";
    else if (status == 400) status_text = "Bad Request";
    else if (status == 403) status_text = "Forbidden";
    else if (status == 404) status_text = "Not Found";
    else if (status == 408) status_text = "Request Timeout";
    else if (status == 413) status_text = "Payload Too Large";
    else if (status == 431) status_text = "Request Header Fields Too Large";
    else if (status == 500) status_text = "Internal Server Error";
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        status, status_text, content_type, body_len,
        extra_headers ? extra_headers : "");
    send_all(fd, header, (size_t)hlen);
    send_all(fd, body, body_len);
}

static void send_response(int fd, int status, const char *content_type,
                          const char *body, size_t body_len) {
    send_response_ex(fd, status, content_type, body, body_len, NULL);
}

/* Same as send_response_ex but lets the caller specify a status code AND
   render the ACTIVE tree (the current session's, or app->root when no
   session is set — used by the 404 handler) into HTML. The error page
   comes back with HTTP 404 (not 200).                                    */
static void send_html_status(cweb_app *app, int fd, int status,
                             const char *extra_headers) {
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
        "%s"
        "\r\n",
        status, status_text, html_len, extra_headers ? extra_headers : "");
    send_all(fd, header, (size_t)hlen);
    if (html) {
        send_all(fd, html, html_len);
        free(html);
    }
}

/* Send a 301 redirect. Location header is mandatory for redirects.
   extra_headers carries Set-Cookie when this request created a session
   (the browser stores it BEFORE following the redirect).               */
static void send_redirect(int fd, const char *location,
                          const char *extra_headers) {
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
        "HTTP/1.1 301 Moved\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        location, extra_headers ? extra_headers : "");
    send_all(fd, header, (size_t)hlen);
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

    char fs_path[PATH_MAX];   /* roomy: url_path ≤1KiB + "./static/" always fits */
    if (strlen(url_path) >= sizeof(fs_path) - strlen("./static")) return 0;
    snprintf(fs_path, sizeof(fs_path), "./static/%s", rel);

    /* Containment double-check: resolve the path (follows symlinks,
       collapses any residual '..') and require the result to stay
       under ./static/. Defense in depth on top of the literal ".."
       string filter above.                                            */
    char root_resolved[PATH_MAX];
    char resolved[PATH_MAX];
    if (!realpath("./static", root_resolved)) return 0;
    if (!realpath(fs_path, resolved)) return 0;
    size_t rl = strlen(root_resolved);
    if (strncmp(resolved, root_resolved, rl) != 0 ||
        (resolved[rl] != '\0' && resolved[rl] != '/')) {
        errno = EACCES;
        return 0;
    }

    FILE *fp = fopen(resolved, "rb");
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

/* Drop a session's tree + id registry (used when a route rebuilds the
   page — the session itself and its user state survive).               */
static void session_drop_tree(cweb_session *sess) {
    if (!sess) return;
    if (sess->root) {
        cweb_widget_free(sess->root);
        sess->root = NULL;
    }
    free(sess->registry);
    sess->registry = NULL;
    sess->registry_count = 0;
    sess->registry_cap = 0;
    sess->next_id = 0;
    free(sess->page_key);
    sess->page_key = NULL;
}

/* Dispatch a request.
   - GET /static/…       → static asset from ./static/ (stateless, no session)
   - GET /api/health     → JSON health (stateless, no session)
   - everything else resolves to the caller's cweb_session FIRST, so:
   - POST /api/click?id=N → invoke on_click(w, ev, state) on the SESSION's tree
   - POST /api/input?id=N → update input buffer + on_input on the SESSION's tree
   - GET <route>          → reuse / (re)build THIS session's tree via builder
   - GET / (no routes)    → render the session's private clone of app->root

   A session created by this request is announced via Set-Cookie so the
   browser sticks to it (fetch() sends same-origin cookies by default).  */
static void handle_request(cweb_app *app, http_req *req, int fd) {
    /* Static file serving: /static/foo.png → ./static/foo.png. Kept
       BEFORE session resolution: asset floods from cookie-less clients
       must not churn the session LRU.                                     */
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

    /* Resolve this visitor's session (creates it on first visit) and
       make its tree the ACTIVE one for callbacks, id registration and
       rendering. Cleared by the caller (cweb_http_serve) afterwards.     */
    int created = 0;
    cweb_session *sess = cweb_app_session_get(app,
                                              req->sid[0] ? req->sid : NULL,
                                              &created);
    sess->last_used = ++app->req_counter;
    app->cur_sess = sess;

    /* First visit: hand the browser its session id. HttpOnly keeps it
       away from page JS; SameSite=Lax is the safe default.               */
    char set_cookie[96];
    const char *cookie_hdr = NULL;
    if (created) {
        snprintf(set_cookie, sizeof(set_cookie),
                 "Set-Cookie: cweb_sid=%s; Path=/; HttpOnly; SameSite=Lax\r\n",
                 sess->sid);
        cookie_hdr = set_cookie;
    }

    /* Click callbacks (POST /api/click?id=N).
       They mutate THIS visitor's tree with THIS visitor's state — two
       users clicking the same button never see each other's changes.     */
    if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/api/click", 10) == 0) {
        int id = extract_query_int(req->path, "id");
        cweb_widget *w = cweb_widget_find(sess->root, id);
        if (w && w->on_click) {
            cweb_event ev = {0};
            w->on_click(w, &ev, sess->state);
        }
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        send_response_ex(fd, 200, "text/html; charset=utf-8",
                         html ? html : "", html_len, cookie_hdr);
        free(html);
        return;
    }

    /* Input callbacks (POST /api/input?id=N) — buffer + callback live in
       the session's tree, so typed text is private per user too.         */
    if (strcmp(req->method, "POST") == 0 && strncmp(req->path, "/api/input", 10) == 0) {
        int id = extract_query_int(req->path, "id");
        cweb_widget *w = cweb_widget_find(sess->root, id);
        if (w && w->inputable) {
            cweb_input_set_value(w, req->body ? req->body : "");
            if (w->on_input) {
                cweb_event ev = {0};
                ev.value = req->body ? req->body : "";
                w->on_input(w, &ev, sess->state);
            }
        }
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        send_response_ex(fd, 200, "text/html; charset=utf-8",
                         html ? html : "", html_len, cookie_hdr);
        free(html);
        return;
    }

    /* GET requests: check redirects first, then routes. */
    if (strcmp(req->method, "GET") == 0) {
        char path[1024];
        snprintf(path, sizeof(path), "%s", req->path);
        path_only(path);

        /* Redirects */
        const char *target = match_redirect(app, path);
        if (target) {
            send_redirect(fd, target, cookie_hdr);
            return;
        }

        /* Routes: if any are registered, match against them. */
        if (app->routes_count > 0) {
            const cweb_route *r = match_route(app, path);
            if (r) {
                /* Reuse the session's existing tree when the visitor is
                   re-requesting the page it was built from — per-user
                   changes (clicks, typed text) survive reloads. A DIFFERENT
                   path drops the old page and builds a fresh one; the user
                   state itself survives navigation (it belongs to the
                   session, not to a page).                                */
                if (!sess->root || !sess->page_key ||
                    strcmp(sess->page_key, path) != 0) {
                    session_drop_tree(sess);
                    cweb_widget *root = malloc(sizeof(cweb_widget));
                    if (!root) {
                        send_response_ex(fd, 500, "text/plain",
                                         "out of memory", 13, cookie_hdr);
                        return;
                    }
                    cweb_widget_init(root, CWEB_W_CONTAINER);
                    sess->root = root;
                    sess->page_key = strdup(path);
                    r->builder(app, root);
                }

                size_t html_len = 0;
                char *html = cweb_html_render(app, &html_len);
                if (html) {
                    send_response_ex(fd, 200, "text/html; charset=utf-8",
                                     html, html_len, cookie_hdr);
                    free(html);
                } else {
                    send_response_ex(fd, 500, "text/plain", "render error",
                                     12, cookie_hdr);
                }
                return;
            }
            /* No route matched — try the custom 404 handler if registered. */
            if (app->not_found_handler) {
                /* The 404 page is stateless: build it into a THROWAWAY
                   tree rendered against the app-level registry (no
                   session), leaving the visitor's own tree untouched.     */
                cweb_widget *saved_root = app->root;  /* NULL in route mode */
                app->cur_sess = NULL;
                free(app->registry);
                app->registry = NULL;
                app->registry_count = 0;
                app->registry_cap = 0;
                app->next_id = 0;

                cweb_widget *troot = malloc(sizeof(cweb_widget));
                if (!troot) {
                    app->root = saved_root;
                    app->cur_sess = sess;
                    send_response_ex(fd, 500, "text/plain", "out of memory",
                                     13, cookie_hdr);
                    return;
                }
                cweb_widget_init(troot, CWEB_W_CONTAINER);
                app->root = troot;

                app->not_found_handler(app, troot);
                send_html_status(app, fd, 404, cookie_hdr);

                cweb_widget_free(troot);
                app->root = saved_root;
                app->cur_sess = sess;
                return;
            }
            send_response_ex(fd, 404, "text/plain", "Not Found", 9, cookie_hdr);
            return;
        }

        /* No routes registered — legacy single-page mode: every visitor
           renders their own private clone of the prototype root (made
           when the session was created; re-seeded here if set_root was
           called after the session already existed).                       */
        if (!sess->root && app->root) {
            sess->root = cweb_widget_clone(app->root);
        }
        size_t html_len = 0;
        char *html = cweb_html_render(app, &html_len);
        if (html) {
            send_response_ex(fd, 200, "text/html; charset=utf-8",
                             html, html_len, cookie_hdr);
            free(html);
        } else {
            send_response_ex(fd, 500, "text/plain", "render error", 12, cookie_hdr);
        }
        return;
    }

    send_response_ex(fd, 404, "text/plain", "Not Found", 9, cookie_hdr);
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

        /* Harden every connection up-front: idle timeouts make a stalled
           client incapable of parking the single-threaded server forever. */
        set_client_timeouts(cli_fd);

        /* Read the full request (looped reads + caps). Replaces the old
           single 64 KB read() which could truncate requests and had no
           timeout or size limits whatsoever.                              */
        char   *raw      = NULL;
        size_t  raw_len  = 0;
        size_t  head_len = 0;
        recv_status rs  = recv_request(cli_fd, &raw, &raw_len, &head_len);

        switch (rs) {
        case RR_OK: {
            http_req req;
            if (parse_request(raw, raw_len, head_len, &req) == 0) {
                handle_request(app, &req, cli_fd);
                app->cur_sess = NULL;   /* no active tree outside requests */
                free(req.body);
            } else {
                send_response(cli_fd, 400, "text/plain", "Bad Request", 11);
            }
            break;
        }
        case RR_BAD_REQUEST:
            send_response(cli_fd, 400, "text/plain", "Bad Request", 11);
            break;
        case RR_HEADERS_TOO_LARGE:
            send_response(cli_fd, 431, "text/plain", "Headers Too Large", 17);
            break;
        case RR_BODY_TOO_LARGE:
            send_response(cli_fd, 413, "text/plain", "Payload Too Large", 17);
            break;
        case RR_TIMEOUT:
            send_response(cli_fd, 408, "text/plain", "Request Timeout", 15);
            break;
        default:
            break; /* RR_DISCONNECTED: peer gone — nothing to reply to */
        }
        free(raw);

        /* When we answer with an error but never drained the promised
           request body, close() may raise RST before our response
           reaches the client. A short grace period lets the status code
           flush out first.                                                */
        if (rs != RR_OK && rs != RR_DISCONNECTED)
            usleep(100000);

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
