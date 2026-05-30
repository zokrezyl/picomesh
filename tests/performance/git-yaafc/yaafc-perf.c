/* yaafc-perf — load generator + latency/throughput harness for the
 * git-yaafc gateway.
 *
 * Drives the GATEWAY only (the production hot path, per CLAUDE.md): it
 * never touches a backend port directly. Each worker is a real OS
 * thread with its own keep-alive TCP connection issuing blocking
 * request/response round-trips — independent threads give accurate
 * concurrency and per-request latency without sharing (or being gated
 * by) the system-under-test's event loop.
 *
 * Deliberately self-contained: plain C + pthreads + POSIX sockets, no
 * yaafc libraries linked in. The same binary therefore measures the
 * yaafc C gateway or yaapp's Python gateway — the HTTP contract is
 * identical.
 *
 * Scenarios (selected with --scenario), each timed per iteration:
 *   rpc_count    POST /_rpc git_repo.store.count_total  (read; gateway→1 backend)
 *   login        POST /login                             (auth composite; 4 backends)
 *   repo_create  POST /repos/new                         (write; storage + git_repo)
 *   register     POST /register (new user each time)     (write; accounts + authn)
 *   full         register → login → repo_create → /repos (end-to-end journey)
 *
 * Adding a scenario: write a `scenario_step` and add a row to
 * SCENARIOS in scenario_lookup(). Keep each step one timed unit of
 * work so the latency stats stay meaningful.
 *
 * Usage:
 *   yaafc-perf [--host H] [--port P] [--connections N]
 *              [--duration SECS | --requests R] [--scenario NAME]
 */

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* ---- config ---------------------------------------------------------- */

enum scenario_id {
    SCENARIO_RPC_COUNT = 0,
    SCENARIO_LOGIN,
    SCENARIO_REPO_CREATE,
    SCENARIO_REGISTER,
    SCENARIO_FULL,
    SCENARIO_STORE_SET,   /* storage.db.set, unique key/iter (single-env write) */
    SCENARIO_SHARD_SET,   /* sharded_storage.db.set, unique key/iter (sharded write) */
};

struct perf_config {
    const char *host;
    int port;
    int connections;        /* worker threads */
    double duration_secs;   /* used when requests_per_worker == 0 */
    long requests_per_worker; /* >0 overrides duration */
    enum scenario_id scenario;
    const char *scenario_name;
    long run_nonce;         /* keeps usernames unique across runs */
};

/* ---- monotonic clock ------------------------------------------------- */

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* ---- a keep-alive connection to the gateway -------------------------- */

struct conn {
    int fd;
    const char *host;
    int port;
    char rbuf[1 << 16]; /* response scratch (headers + drain) */
};

static int conn_open(struct conn *c)
{
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char portbuf[16];
    snprintf(portbuf, sizeof(portbuf), "%d", c->port);
    struct addrinfo *res = NULL;
    if (getaddrinfo(c->host, portbuf, &hints, &res) != 0 || !res) return -1;
    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return -1; }
    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    c->fd = fd;
    return 0;
}

static void conn_close(struct conn *c)
{
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
}

static int write_all(int fd, const char *buf, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(fd, buf + off, n - off);
        if (w > 0) { off += (size_t)w; continue; }
        if (w < 0 && errno == EINTR) continue;
        return -1;
    }
    return 0;
}

/* Issue one HTTP request on the keep-alive connection and consume the
 * full response (headers + Content-Length body). Returns the HTTP
 * status code, or -1 on transport failure. The first `yaafc-sid`
 * cookie value, if any, is copied into out_sid (out_sid_cap). */
static int http_request(struct conn *c, const char *method, const char *path,
                        const char *content_type, const char *cookie_sid,
                        const char *body, size_t body_len,
                        char *out_sid, size_t out_sid_cap)
{
    char hdr[1024];
    int n = snprintf(hdr, sizeof(hdr),
        "%s %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: keep-alive\r\n",
        method, path, c->host, c->port);
    if (n <= 0 || (size_t)n >= sizeof(hdr)) return -1;
    if (content_type && body_len) {
        n += snprintf(hdr + n, sizeof(hdr) - n,
                      "Content-Type: %s\r\nContent-Length: %zu\r\n",
                      content_type, body_len);
    } else {
        n += snprintf(hdr + n, sizeof(hdr) - n, "Content-Length: 0\r\n");
    }
    if (cookie_sid && *cookie_sid) {
        n += snprintf(hdr + n, sizeof(hdr) - n,
                      "Cookie: yaafc-sid=%s\r\n", cookie_sid);
    }
    n += snprintf(hdr + n, sizeof(hdr) - n, "\r\n");
    if (n <= 0 || (size_t)n >= sizeof(hdr)) return -1;

    if (write_all(c->fd, hdr, (size_t)n) < 0) return -1;
    if (body_len && write_all(c->fd, body, body_len) < 0) return -1;

    /* Read until the header terminator is in, then drain the body. */
    size_t total = 0;
    char *hdr_end = NULL;
    while (total < sizeof(c->rbuf) - 1) {
        ssize_t r = read(c->fd, c->rbuf + total, sizeof(c->rbuf) - 1 - total);
        if (r > 0) {
            total += (size_t)r;
            c->rbuf[total] = 0;
            hdr_end = strstr(c->rbuf, "\r\n\r\n");
            if (hdr_end) break;
            continue;
        }
        if (r < 0 && errno == EINTR) continue;
        return -1;
    }
    if (!hdr_end) return -1;

    int status = 0;
    if (sscanf(c->rbuf, "HTTP/1.%*d %d", &status) != 1) return -1;

    if (out_sid && out_sid_cap) {
        out_sid[0] = 0;
        /* Case-insensitive scan for a Set-Cookie: yaafc-sid=<value>. */
        for (char *p = c->rbuf; p < hdr_end; ++p) {
            if ((p == c->rbuf || p[-1] == '\n') &&
                strncasecmp(p, "set-cookie:", 11) == 0) {
                char *v = strstr(p, "yaafc-sid=");
                if (v && v < hdr_end) {
                    v += 10;
                    size_t i = 0;
                    while (v[i] && v[i] != ';' && v[i] != '\r' && v[i] != '\n'
                           && i < out_sid_cap - 1) { out_sid[i] = v[i]; i++; }
                    out_sid[i] = 0;
                }
                break;
            }
        }
    }

    /* Content-Length → drain the body so the connection stays clean. */
    long content_length = 0;
    for (char *p = c->rbuf; p < hdr_end; ++p) {
        if ((p == c->rbuf || p[-1] == '\n') &&
            strncasecmp(p, "content-length:", 15) == 0) {
            content_length = strtol(p + 15, NULL, 10);
            break;
        }
    }
    size_t header_len = (size_t)(hdr_end - c->rbuf) + 4;
    size_t body_have = total - header_len;
    long remaining = content_length - (long)body_have;
    while (remaining > 0) {
        size_t want = (size_t)remaining < sizeof(c->rbuf) ? (size_t)remaining
                                                          : sizeof(c->rbuf);
        ssize_t r = read(c->fd, c->rbuf, want);
        if (r > 0) { remaining -= r; continue; }
        if (r < 0 && errno == EINTR) continue;
        return -1;
    }
    return status;
}

/* A request with one transparent reconnect on transport failure — a
 * dropped keep-alive shouldn't count as an application error. */
static int http_try(struct conn *c, const char *method, const char *path,
                    const char *ctype, const char *sid,
                    const char *body, size_t body_len,
                    char *out_sid, size_t out_sid_cap)
{
    int st = http_request(c, method, path, ctype, sid, body, body_len,
                          out_sid, out_sid_cap);
    if (st >= 0) return st;
    conn_close(c);
    if (conn_open(c) != 0) return -1;
    return http_request(c, method, path, ctype, sid, body, body_len,
                        out_sid, out_sid_cap);
}

/* ---- per-worker latency capture -------------------------------------- */

struct latencies {
    uint64_t *v;
    size_t len;
    size_t cap;
};

static void lat_push(struct latencies *l, uint64_t ns)
{
    if (l->len == l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 8192;
        uint64_t *nv = realloc(l->v, nc * sizeof(*nv));
        if (!nv) return; /* drop the sample rather than abort the run */
        l->v = nv;
        l->cap = nc;
    }
    l->v[l->len++] = ns;
}

/* ---- worker ---------------------------------------------------------- */

struct worker {
    pthread_t thread;
    int id;
    const struct perf_config *cfg;
    volatile int *stop;     /* deadline flag set by the main thread */

    struct conn conn;
    char sid[64];           /* session token for this worker */
    int session_ok;

    /* results */
    struct latencies lat;
    uint64_t ok;
    uint64_t errors;
};

/* Build a username unique to (run, worker[, counter]); fits the
 * gateway's [a-z0-9._-]{1,32} rule. */
static void make_user(char *out, size_t cap, long nonce, int worker, long counter)
{
    if (counter < 0)
        snprintf(out, cap, "p%ldw%d", nonce % 100000, worker);
    else
        snprintf(out, cap, "p%ldw%dn%ld", nonce % 100000, worker, counter);
}

/* Register (idempotent — "already taken" is fine) + login, capturing
 * the session cookie. Returns 1 on a usable session. */
static int worker_establish_session(struct worker *w)
{
    char user[40];
    make_user(user, sizeof(user), w->cfg->run_nonce, w->id, -1);
    char body[128];
    int bn = snprintf(body, sizeof(body), "username=%s&password=x", user);

    /* Register once (idempotent), then log in. Retry a few times: under
     * heavy concurrency a transient backend hiccup shouldn't leave the
     * worker session-less and silently degrade into measuring redirects. */
    http_try(&w->conn, "POST", "/register",
             "application/x-www-form-urlencoded", NULL, body, (size_t)bn,
             NULL, 0);
    for (int attempt = 0; attempt < 3; ++attempt) {
        w->sid[0] = 0;
        int st = http_try(&w->conn, "POST", "/login",
                          "application/x-www-form-urlencoded", NULL,
                          body, (size_t)bn, w->sid, sizeof(w->sid));
        if ((st == 303 || st == 200) && w->sid[0]) return 1;
    }
    return 0;
}

/* One timed unit of work. Returns 1 on success, 0 on app-level failure. */
static int scenario_step(struct worker *w, long counter)
{
    struct conn *c = &w->conn;
    switch (w->cfg->scenario) {
    case SCENARIO_RPC_COUNT: {
        static const char body[] =
            "{\"path\":\"git_repo.store.count_total\",\"args\":[]}";
        int st = http_try(c, "POST", "/_rpc", "application/json", w->sid,
                          body, sizeof(body) - 1, NULL, 0);
        return st == 200;
    }
    case SCENARIO_LOGIN: {
        char user[40], body[128];
        make_user(user, sizeof(user), w->cfg->run_nonce, w->id, -1);
        int bn = snprintf(body, sizeof(body), "username=%s&password=x", user);
        char sid[64];
        int st = http_try(c, "POST", "/login",
                          "application/x-www-form-urlencoded", NULL,
                          body, (size_t)bn, sid, sizeof(sid));
        return st == 303 || st == 200;
    }
    case SCENARIO_REPO_CREATE: {
        /* Needs a real session — otherwise /repos/new just 303s back to
         * /login and we'd be timing redirects, not repo creation. Treat
         * a missing session as a failure so it shows up honestly. */
        if (!w->sid[0]) return 0;
        char body[64];
        int bn = snprintf(body, sizeof(body), "name=r%ld", counter);
        int st = http_try(c, "POST", "/repos/new",
                          "application/x-www-form-urlencoded", w->sid,
                          body, (size_t)bn, NULL, 0);
        /* A real create lands on the repo page (303 to /<acct>/<name>).
         * A 303 to /login means the session was rejected → not a success. */
        return st == 303;
    }
    case SCENARIO_REGISTER: {
        char user[40], body[128];
        make_user(user, sizeof(user), w->cfg->run_nonce, w->id, counter);
        int bn = snprintf(body, sizeof(body), "username=%s&password=x", user);
        int st = http_try(c, "POST", "/register",
                          "application/x-www-form-urlencoded", NULL,
                          body, (size_t)bn, NULL, 0);
        return st == 303 || st == 200;
    }
    case SCENARIO_FULL: {
        char user[40], body[128], sid[64];
        make_user(user, sizeof(user), w->cfg->run_nonce, w->id, counter);
        int bn = snprintf(body, sizeof(body), "username=%s&password=x", user);
        if (http_try(c, "POST", "/register", "application/x-www-form-urlencoded",
                     NULL, body, (size_t)bn, NULL, 0) < 0) return 0;
        sid[0] = 0;
        int st = http_try(c, "POST", "/login", "application/x-www-form-urlencoded",
                          NULL, body, (size_t)bn, sid, sizeof(sid));
        if (!(st == 303 || st == 200) || !sid[0]) return 0;
        char rbody[64];
        int rn = snprintf(rbody, sizeof(rbody), "name=r%ld", counter);
        if (http_try(c, "POST", "/repos/new", "application/x-www-form-urlencoded",
                     sid, rbody, (size_t)rn, NULL, 0) < 0) return 0;
        st = http_try(c, "GET", "/repos", NULL, sid, "", 0, NULL, 0);
        return st == 200;
    }
    case SCENARIO_STORE_SET:
    case SCENARIO_SHARD_SET: {
        /* Direct KV write load: unique key per iteration into namespace
         * "bench". store_set targets the (now retired) single-env
         * storage service; shard_set targets sharded_storage. */
        const char *path = (w->cfg->scenario == SCENARIO_SHARD_SET)
                               ? "sharded_storage.db.set" : "storage.db.set";
        char body[160];
        int bn = snprintf(body, sizeof(body),
            "{\"path\":\"%s\",\"args\":[\"bench\",\"k%d_%ld\",%ld]}",
            path, w->id, counter, counter);
        int st = http_try(c, "POST", "/_rpc", "application/json", w->sid,
                          body, (size_t)bn, NULL, 0);
        return st == 200;
    }
    }
    return 0;
}

static int scenario_needs_session(enum scenario_id s)
{
    return s == SCENARIO_RPC_COUNT || s == SCENARIO_REPO_CREATE;
}

static void *worker_main(void *arg)
{
    struct worker *w = arg;
    w->conn.fd = -1;
    w->conn.host = w->cfg->host;
    w->conn.port = w->cfg->port;
    if (conn_open(&w->conn) != 0) return NULL;

    w->session_ok = 1;
    if (scenario_needs_session(w->cfg->scenario)) {
        w->session_ok = worker_establish_session(w);
        /* Without a session this scenario can only fail; spinning on it
         * would record millions of instant no-op "errors" and report a
         * nonsense throughput. Bail so the result reads honestly as
         * "0 sessions / 0 requests" — a clear signal the gateway buckled
         * during setup. */
        if (!w->session_ok) { conn_close(&w->conn); return NULL; }
    } else {
        worker_establish_session(w); /* best-effort; cookie still useful */
    }

    long counter = 0;
    if (w->cfg->requests_per_worker > 0) {
        for (long i = 0; i < w->cfg->requests_per_worker; ++i) {
            uint64_t t0 = now_ns();
            int ok = scenario_step(w, counter++);
            lat_push(&w->lat, now_ns() - t0);
            if (ok) w->ok++; else w->errors++;
        }
    } else {
        while (!*w->stop) {
            uint64_t t0 = now_ns();
            int ok = scenario_step(w, counter++);
            lat_push(&w->lat, now_ns() - t0);
            if (ok) w->ok++; else w->errors++;
        }
    }
    conn_close(&w->conn);
    return NULL;
}

/* ---- stats ----------------------------------------------------------- */

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static double pctl_ms(const uint64_t *sorted, size_t n, double p)
{
    if (!n) return 0.0;
    size_t idx = (size_t)(p / 100.0 * (double)(n - 1) + 0.5);
    if (idx >= n) idx = n - 1;
    return (double)sorted[idx] / 1e6;
}

static enum scenario_id scenario_lookup(const char *name, const char **canonical)
{
    struct row { const char *name; enum scenario_id id; };
    static const struct row SCENARIOS[] = {
        {"rpc_count",   SCENARIO_RPC_COUNT},
        {"login",       SCENARIO_LOGIN},
        {"repo_create", SCENARIO_REPO_CREATE},
        {"register",    SCENARIO_REGISTER},
        {"full",        SCENARIO_FULL},
        {"store_set",   SCENARIO_STORE_SET},
        {"shard_set",   SCENARIO_SHARD_SET},
    };
    for (size_t i = 0; i < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); ++i) {
        if (strcmp(name, SCENARIOS[i].name) == 0) {
            *canonical = SCENARIOS[i].name;
            return SCENARIOS[i].id;
        }
    }
    *canonical = NULL;
    return SCENARIO_RPC_COUNT;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [options]\n"
        "  --host H            gateway host (default 127.0.0.1)\n"
        "  --port P            gateway port (default 8080)\n"
        "  --connections N     concurrent worker threads (default 8)\n"
        "  --duration SECS     run for this many seconds (default 10)\n"
        "  --requests R        fixed requests PER WORKER (overrides --duration)\n"
        "  --scenario NAME     rpc_count | login | repo_create | register | full\n"
        "                      (default rpc_count)\n",
        prog);
}

int main(int argc, char **argv)
{
    struct perf_config cfg = {
        .host = "127.0.0.1",
        .port = 8080,
        .connections = 8,
        .duration_secs = 10.0,
        .requests_per_worker = 0,
        .scenario = SCENARIO_RPC_COUNT,
        .scenario_name = "rpc_count",
        .run_nonce = (long)time(NULL),
    };

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (!strcmp(a, "--host") && next) { cfg.host = next; i++; }
        else if (!strcmp(a, "--port") && next) { cfg.port = atoi(next); i++; }
        else if (!strcmp(a, "--connections") && next) { cfg.connections = atoi(next); i++; }
        else if (!strcmp(a, "--duration") && next) { cfg.duration_secs = atof(next); i++; }
        else if (!strcmp(a, "--requests") && next) { cfg.requests_per_worker = atol(next); i++; }
        else if (!strcmp(a, "--scenario") && next) {
            const char *canon = NULL;
            cfg.scenario = scenario_lookup(next, &canon);
            if (!canon) { fprintf(stderr, "unknown scenario '%s'\n", next); usage(argv[0]); return 2; }
            cfg.scenario_name = canon;
            i++;
        }
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(argv[0]); return 0; }
        else { fprintf(stderr, "unknown arg '%s'\n", a); usage(argv[0]); return 2; }
    }
    if (cfg.connections < 1) cfg.connections = 1;

    struct worker *workers = calloc((size_t)cfg.connections, sizeof(*workers));
    if (!workers) { fprintf(stderr, "oom\n"); return 1; }
    volatile int stop = 0;

    fprintf(stderr, "yaafc-perf — scenario=%s host=%s:%d connections=%d ",
            cfg.scenario_name, cfg.host, cfg.port, cfg.connections);
    if (cfg.requests_per_worker > 0)
        fprintf(stderr, "requests=%ld/worker\n", cfg.requests_per_worker);
    else
        fprintf(stderr, "duration=%.1fs\n", cfg.duration_secs);

    uint64_t wall0 = now_ns();
    for (int i = 0; i < cfg.connections; ++i) {
        workers[i].id = i;
        workers[i].cfg = &cfg;
        workers[i].stop = &stop;
        if (pthread_create(&workers[i].thread, NULL, worker_main, &workers[i]) != 0) {
            fprintf(stderr, "pthread_create failed for worker %d\n", i);
            cfg.connections = i;
            break;
        }
    }

    if (cfg.requests_per_worker <= 0) {
        struct timespec ts = {
            .tv_sec = (time_t)cfg.duration_secs,
            .tv_nsec = (long)((cfg.duration_secs - (double)(time_t)cfg.duration_secs) * 1e9),
        };
        nanosleep(&ts, NULL);
        stop = 1;
    }

    for (int i = 0; i < cfg.connections; ++i)
        pthread_join(workers[i].thread, NULL);
    double wall_s = (double)(now_ns() - wall0) / 1e9;

    /* Merge latencies + tally. */
    uint64_t total_ok = 0, total_err = 0;
    size_t total_samples = 0;
    int sessions_ok = 0;
    for (int i = 0; i < cfg.connections; ++i) {
        total_ok += workers[i].ok;
        total_err += workers[i].errors;
        total_samples += workers[i].lat.len;
        sessions_ok += workers[i].session_ok ? 1 : 0;
    }
    uint64_t *all = malloc(total_samples * sizeof(*all) + 1);
    size_t off = 0;
    if (all) {
        for (int i = 0; i < cfg.connections; ++i) {
            memcpy(all + off, workers[i].lat.v, workers[i].lat.len * sizeof(*all));
            off += workers[i].lat.len;
        }
        qsort(all, off, sizeof(*all), cmp_u64);
    }

    double mean_ms = 0.0;
    if (off) {
        uint64_t sum = 0;
        for (size_t i = 0; i < off; ++i) sum += all[i];
        mean_ms = (double)sum / (double)off / 1e6;
    }
    uint64_t total_req = total_ok + total_err;
    double thr = wall_s > 0 ? (double)total_req / wall_s : 0.0;

    printf("\n");
    printf("scenario       : %s\n", cfg.scenario_name);
    printf("connections    : %d (%d sessions established)\n", cfg.connections, sessions_ok);
    printf("wall time      : %.3f s\n", wall_s);
    printf("requests       : %llu ok, %llu errors\n",
           (unsigned long long)total_ok, (unsigned long long)total_err);
    printf("throughput     : %.1f req/s\n", thr);
    if (all && off) {
        printf("latency (ms)   : mean=%.3f min=%.3f p50=%.3f p90=%.3f "
               "p99=%.3f p99.9=%.3f max=%.3f\n",
               mean_ms, (double)all[0] / 1e6,
               pctl_ms(all, off, 50), pctl_ms(all, off, 90),
               pctl_ms(all, off, 99), pctl_ms(all, off, 99.9),
               (double)all[off - 1] / 1e6);
    }
    printf("\n");

    int exit_code = (total_err > 0 || total_ok == 0) ? 1 : 0;
    free(all);
    for (int i = 0; i < cfg.connections; ++i) free(workers[i].lat.v);
    free(workers);
    return exit_code;
}
