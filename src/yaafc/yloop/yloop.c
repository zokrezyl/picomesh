/* yloop — libuv-backed event loop + coroutine-yielding streams.
 *
 * One yloop owns one uv_loop_t. A serve coroutine runs on the loop's
 * stack via yaafc_coro_resume; when the coro yields (inside yloop_read /
 * yloop_write), control returns to uv_run, which keeps the loop alive.
 *
 * Read model: we keep `uv_read_start` active for the whole lifetime of
 * the stream. Each callback appends incoming bytes into a per-stream
 * ring; if the serve coro is suspended in yloop_read waiting for at
 * least N bytes, we resume it as soon as the ring has N. EOF / error
 * sets sticky flags so subsequent reads return 0 immediately.
 *
 * Write model: yloop_write copies the bytes into a uv_buf_t, fires
 * uv_write, then yields. The write callback resumes the coro and
 * delivers the result via the coro's status word. */

#include <yaafc/yloop/yloop.h>
#include <yaafc/yco/coro.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>

#include <uv.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define RING_INIT_CAP 4096

/* Coroutines whose entry function has returned can't free their own
 * stack while running on it. They queue themselves here; the reaper
 * uv_check runs once per loop iteration on the loop stack and destroys
 * every queued coro that has actually finished. */
struct coro_zombie {
    struct yaafc_coro *coro;
    struct coro_zombie *next;
};

struct yloop {
    uv_loop_t loop;
    uv_check_t reaper;
    struct coro_zombie *zombies;
};

struct yloop_listener {
    struct yloop *owner;
    uv_tcp_t tcp;
    yloop_serve_fn serve;
    void *ud;
};

struct yloop_stream {
    struct yloop *owner;
    uv_tcp_t tcp;
    /* The coroutine this stream's I/O completions resume. For an
     * accepted (serve) stream this is the coro spawned to run the
     * handler, and the stream OWNS it (owns_coro = 1) — it destroys it
     * on close. An outbound stream opened via yloop_connect_tcp from
     * inside a serve coroutine merely BORROWS that coro to resume on
     * connect/read/write (owns_coro = 0): destroying it here would
     * double-free the coro the serve stream already owns. */
    struct yaafc_coro *coro;
    int owns_coro;

    /* receive ring */
    uint8_t *rbuf;
    size_t rcap;
    size_t rlen;
    int eof;

    /* read-wait state. Only the read path uses `coro`; writes track
     * their own waiter per-request (see struct write_req), so multiple
     * coroutines may write the same stream concurrently while one coro
     * is parked in yloop_read. */
    size_t want;        /* bytes the suspended coro wants */
    int read_blocked;   /* coro suspended in yloop_read? */

    int closing;
};

static void on_reaper_check(uv_check_t *h)
{
    struct yloop *l = h->data;
    struct coro_zombie *z = l->zombies;
    l->zombies = NULL;
    while (z) {
        struct coro_zombie *next = z->next;
        if (yaafc_coro_is_finished(z->coro)) {
            yaafc_coro_destroy(z->coro);
            free(z);
        } else {
            /* Not finished yet — keep it for the next iteration. */
            z->next = l->zombies;
            l->zombies = z;
        }
        z = next;
    }
}

void yloop_reap_coro(struct yloop *l, struct yaafc_coro *coro)
{
    if (!l || !coro) return;
    struct coro_zombie *z = calloc(1, sizeof(*z));
    if (!z) {
        ywarn("yloop_reap_coro: calloc failed — coro id=%u leaked",
              yaafc_coro_id(coro));
        return;
    }
    z->coro = coro;
    z->next = l->zombies;
    l->zombies = z;
}

static void on_alloc(uv_handle_t *h, size_t suggested, uv_buf_t *out)
{
    (void)h;
    out->base = malloc(suggested);
    out->len  = out->base ? suggested : 0;
}

static void ring_append(struct yloop_stream *s, const uint8_t *data, size_t n)
{
    if (!s->rbuf) {
        s->rcap = RING_INIT_CAP;
        s->rbuf = malloc(s->rcap);
    }
    if (s->rlen + n > s->rcap) {
        size_t ncap = s->rcap ? s->rcap : RING_INIT_CAP;
        while (s->rlen + n > ncap) ncap *= 2;
        uint8_t *nb = realloc(s->rbuf, ncap);
        if (!nb) {
            ywarn("yloop: ring realloc failed");
            return;
        }
        s->rbuf = nb;
        s->rcap = ncap;
    }
    memcpy(s->rbuf + s->rlen, data, n);
    s->rlen += n;
}

static void on_read(uv_stream_t *st, ssize_t nread, const uv_buf_t *buf)
{
    struct yloop_stream *s = st->data;
    if (nread > 0) {
        ring_append(s, (const uint8_t *)buf->base, (size_t)nread);
    } else if (nread < 0) {
        /* UV_EOF or any other read error → terminal. */
        s->eof = 1;
    }
    free(buf->base);
    if (s->read_blocked && (s->rlen >= s->want || s->eof)) {
        s->read_blocked = 0;
        yaafc_coro_resume(s->coro);
    }
}

size_t yloop_read_some(struct yloop_stream *s, void *buf, size_t cap)
{
    if (!s || !buf || cap == 0) return 0;
    /* Resume whoever is calling now — a stream may be driven by
     * different coroutines over its lifetime (e.g. a pooled/cached
     * outbound RPC connection). Callers must serialise access to one
     * stream; this just makes the wakeup target correct. */
    s->coro = yaafc_coro_current();
    while (s->rlen == 0 && !s->eof) {
        s->want = 1;
        s->read_blocked = 1;
        yaafc_coro_yield();
    }
    if (s->rlen == 0) return 0; /* EOF */
    size_t take = s->rlen < cap ? s->rlen : cap;
    memcpy(buf, s->rbuf, take);
    if (take < s->rlen) {
        memmove(s->rbuf, s->rbuf + take, s->rlen - take);
    }
    s->rlen -= take;
    return take;
}

size_t yloop_read(struct yloop_stream *s, void *buf, size_t n)
{
    if (!s || !buf || n == 0) return 0;
    s->coro = yaafc_coro_current(); /* resume the current caller (see yloop_read_some) */
    while (s->rlen < n && !s->eof) {
        s->want = n;
        s->read_blocked = 1;
        yaafc_coro_yield();
    }
    size_t take = s->rlen < n ? s->rlen : n;
    memcpy(buf, s->rbuf, take);
    if (take < s->rlen) {
        memmove(s->rbuf, s->rbuf + take, s->rlen - take);
    }
    s->rlen -= take;
    return take;
}

/* Each write carries its own waiter and result slot rather than parking
 * on the shared stream, so concurrent writers (many handler coros
 * responding on one multiplexed connection) don't clobber each other.
 * libuv flushes queued uv_write requests FIFO and never interleaves the
 * buffers of distinct requests — so a caller that emits a whole frame in
 * ONE yloop_write is guaranteed contiguous on the wire. */
struct write_req {
    uv_write_t req;
    struct yaafc_coro *coro; /* the writer to resume on completion */
    int *status_out;         /* points at the caller's stack slot */
    char *data;              /* owned copy */
};

static void on_write(uv_write_t *req, int status)
{
    struct write_req *wr = (struct write_req *)req;
    struct yaafc_coro *coro = wr->coro;
    if (wr->status_out) *wr->status_out = status;
    free(wr->data);
    free(wr);
    yaafc_coro_resume(coro);
}

size_t yloop_write(struct yloop_stream *s, const void *buf, size_t n)
{
    if (!s || !buf || n == 0) return 0;
    struct write_req *wr = calloc(1, sizeof(*wr));
    if (!wr) return 0;
    int status = -1; /* lives on this coro's stack until on_write resumes us */
    wr->coro = yaafc_coro_current();
    wr->status_out = &status;
    wr->data = malloc(n);
    if (!wr->data) { free(wr); return 0; }
    memcpy(wr->data, buf, n);
    uv_buf_t b = uv_buf_init(wr->data, (unsigned)n);
    int rc = uv_write(&wr->req, (uv_stream_t *)&s->tcp, &b, 1, on_write);
    if (rc < 0) {
        free(wr->data);
        free(wr);
        return 0;
    }
    yaafc_coro_yield();
    return status == 0 ? n : 0;
}

static void on_handle_close(uv_handle_t *h)
{
    /* uv_close completion: the handle is detached but we still own
     * the surrounding struct. Free the stream wrapper here. */
    struct yloop_stream *s = h->data;
    if (!s) return;
    /* Only the stream that OWNS the coroutine destroys it. A borrowed
     * coro (outbound yloop_connect_tcp stream) must not — the owning
     * serve stream will, and a second destroy here is a double free. */
    if (s->owns_coro && s->coro && yaafc_coro_is_finished(s->coro)) {
        yaafc_coro_destroy(s->coro);
    }
    free(s->rbuf);
    free(s);
}

void yloop_close(struct yloop_stream *s)
{
    if (!s || s->closing) return;
    s->closing = 1;
    uv_read_stop((uv_stream_t *)&s->tcp);
    uv_close((uv_handle_t *)&s->tcp, on_handle_close);
}

/* The serve function and ud aren't available inside a static entry —
 * we pack them into a tiny closure laid out immediately after the
 * stream so the trampoline can reach it with a fixed offset. */
struct serve_closure {
    yloop_serve_fn fn;
    void *ud;
};

static void serve_trampoline(void *arg)
{
    struct yloop_stream *s = arg;
    struct serve_closure *cl = (struct serve_closure *)((char *)s + sizeof(*s));
    cl->fn(s->owner, s, cl->ud);
    /* If the handler forgot to close, close now so libuv can clean up. */
    yloop_close(s);
}

static void on_connection(uv_stream_t *server, int status)
{
    struct yloop_listener *L = server->data;
    if (status < 0) {
        ywarn("yloop: accept failed: %s", uv_strerror(status));
        return;
    }

    /* Stream + closure laid out back-to-back so the trampoline can find
     * the closure with a single offset. */
    struct yloop_stream *s = calloc(1, sizeof(*s) + sizeof(struct serve_closure));
    if (!s) return;
    s->owner = L->owner;
    struct serve_closure *cl = (struct serve_closure *)((char *)s + sizeof(*s));
    cl->fn = L->serve;
    cl->ud = L->ud;

    int rc = uv_tcp_init(&L->owner->loop, &s->tcp);
    if (rc < 0) {
        ywarn("yloop: uv_tcp_init failed: %s", uv_strerror(rc));
        free(s);
        return;
    }
    s->tcp.data = s;

    rc = uv_accept(server, (uv_stream_t *)&s->tcp);
    if (rc < 0) {
        ywarn("yloop: uv_accept failed: %s", uv_strerror(rc));
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return;
    }
    /* Kill Nagle on every accepted peer. Without this, request/response
     * traffic on loopback eats a ~40 ms delayed-ACK timer per RTT. */
    uv_tcp_nodelay(&s->tcp, 1);

    rc = uv_read_start((uv_stream_t *)&s->tcp, on_alloc, on_read);
    if (rc < 0) {
        ywarn("yloop: uv_read_start failed: %s", uv_strerror(rc));
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return;
    }

    struct yaafc_coro_ptr_result sr =
        yaafc_coro_spawn(serve_trampoline, s, 0, "yloop-serve");
    if (YAAFC_IS_ERR(sr)) {
        ywarn("yloop: coro spawn failed");
        yaafc_error_destroy(sr.error);
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return;
    }
    s->coro = sr.value;
    s->owns_coro = 1; /* the accepted stream owns the serve coro */
    yaafc_coro_resume(s->coro);
}

struct yloop_ptr_result yloop_create(void)
{
    struct yloop *l = calloc(1, sizeof(*l));
    if (!l) return YAAFC_ERR(yloop_ptr, "yloop_create: calloc failed");
    int rc = uv_loop_init(&l->loop);
    if (rc < 0) {
        free(l);
        return YAAFC_ERR(yloop_ptr, "yloop_create: uv_loop_init failed");
    }
    /* Zombie-coro reaper: runs at the tail of every loop iteration but
     * is unref'd so it never by itself keeps uv_run alive. */
    uv_check_init(&l->loop, &l->reaper);
    l->reaper.data = l;
    uv_check_start(&l->reaper, on_reaper_check);
    uv_unref((uv_handle_t *)&l->reaper);
    return YAAFC_OK(yloop_ptr, l);
}

void yloop_destroy(struct yloop *l)
{
    if (!l) return;
    /* Final sweep: destroy any coro still queued for reaping. */
    on_reaper_check(&l->reaper);
    struct coro_zombie *z = l->zombies;
    while (z) {
        struct coro_zombie *next = z->next;
        free(z);
        z = next;
    }
    uv_check_stop(&l->reaper);
    uv_loop_close(&l->loop);
    free(l);
}

struct yaafc_void_result yloop_run(struct yloop *l)
{
    if (!l) return YAAFC_ERR(yaafc_void, "yloop_run: NULL loop");
    uv_run(&l->loop, UV_RUN_DEFAULT);
    return YAAFC_OK_VOID();
}

void yloop_stop(struct yloop *l)
{
    if (!l) return;
    uv_stop(&l->loop);
}

struct sleep_state {
    uv_timer_t timer;
    struct yaafc_coro *coro;
};

static void on_sleep_timer(uv_timer_t *handle)
{
    struct sleep_state *ss = handle->data;
    uv_timer_stop(handle);
    uv_close((uv_handle_t *)handle, NULL);
    /* The timer handle is embedded in ss; closing it doesn't free ss
     * — yloop_sleep_ms owns that on the coroutine's stack and reads
     * it after resume returns. We resume the coro and the rest
     * unwinds naturally. */
    yaafc_coro_resume(ss->coro);
}

void yloop_sleep_ms(struct yloop *l, unsigned int ms)
{
    if (!l) return;
    struct yaafc_coro *self = yaafc_coro_current();
    if (!self) {
        ywarn("yloop_sleep_ms: not in a coroutine — refusing to block");
        return;
    }
    struct sleep_state ss = {0};
    ss.coro = self;
    uv_timer_init(&l->loop, &ss.timer);
    ss.timer.data = &ss;
    uv_timer_start(&ss.timer, on_sleep_timer, ms, 0);
    yaafc_coro_yield();
}

/* ---- subprocess (uv_spawn) ----------------------------------- */

struct yloop_process {
    uv_process_t proc;
    yloop_process_exit_cb cb;
    void *ud;
    int pid;
};

static void on_proc_exit(uv_process_t *p, int64_t exit_status, int term_signal)
{
    struct yloop_process *self = p->data;
    if (self->cb) {
        self->cb(self, exit_status, term_signal, self->ud);
    }
    uv_close((uv_handle_t *)p, NULL);
    /* Defer free via the loop tick: uv_close completion can run
     * after the close cb, so we can't free self immediately without
     * potentially racing with libuv's internal state. The runtime
     * outlives the handle; leaking a few bytes per spawn is fine for
     * now. A close-cb-driven free is the right cleanup. */
}

int yloop_spawn(struct yloop *l, const char *file, char *const argv[],
                yloop_process_exit_cb on_exit, void *ud)
{
    if (!l || !file || !argv) return 0;

    struct yloop_process *self = calloc(1, sizeof(*self));
    if (!self) return 0;
    self->cb = on_exit;
    self->ud = ud;
    self->proc.data = self;

    /* stdio inheritance — parent stdin/out/err pass straight through. */
    uv_stdio_container_t io[3] = {
        {.flags = UV_INHERIT_FD, .data.fd = 0},
        {.flags = UV_INHERIT_FD, .data.fd = 1},
        {.flags = UV_INHERIT_FD, .data.fd = 2},
    };

    uv_process_options_t opts = {0};
    opts.file = file;
    opts.args = argv;
    opts.exit_cb = on_proc_exit;
    opts.stdio_count = 3;
    opts.stdio = io;
    /* Inherit env (NULL → libuv defaults to parent env). */

    int rc = uv_spawn(&l->loop, &self->proc, &opts);
    if (rc < 0) {
        ywarn("yloop_spawn: uv_spawn failed: %s", uv_strerror(rc));
        free(self);
        return 0;
    }
    self->pid = self->proc.pid;
    yinfo("yloop_spawn: pid=%d cmd=%s", self->pid, file);
    return self->pid;
}

int yloop_kill(struct yloop *l, int pid, int signum)
{
    if (!l || pid <= 0) return -1;
    return uv_kill(pid, signum);
}

/* ---- outgoing TCP (yloop_connect_tcp) ------------------------------- */

struct connect_state {
    uv_connect_t req;
    struct yaafc_coro *coro;
    int status;
};

static void on_connect(uv_connect_t *req, int status)
{
    struct connect_state *cs = req->data;
    cs->status = status;
    yaafc_coro_resume(cs->coro);
}

struct resolve_state {
    uv_getaddrinfo_t req;
    struct yaafc_coro *coro;
    int status;
    struct sockaddr_in addr;
    int got_addr;
};

static void on_resolve(uv_getaddrinfo_t *req, int status, struct addrinfo *res)
{
    struct resolve_state *rs = req->data;
    rs->status = status;
    if (status == 0 && res) {
        /* Take the first IPv4 result; ignore everything else. */
        for (struct addrinfo *p = res; p; p = p->ai_next) {
            if (p->ai_family == AF_INET && p->ai_addrlen >= sizeof(rs->addr)) {
                memcpy(&rs->addr, p->ai_addr, sizeof(rs->addr));
                rs->got_addr = 1;
                break;
            }
        }
        uv_freeaddrinfo(res);
    }
    yaafc_coro_resume(rs->coro);
}

struct yloop_stream_ptr_result yloop_connect_tcp(struct yloop *l,
                                                 const char *host, int port)
{
    if (!l || !host || port <= 0)
        return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: bad args");
    struct yaafc_coro *self = yaafc_coro_current();
    if (!self)
        return YAAFC_ERR(yloop_stream_ptr,
                         "yloop_connect_tcp: must be called from a coroutine");

    struct yloop_stream *s = calloc(1, sizeof(*s));
    if (!s) return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: calloc failed");
    s->owner = l;
    s->coro  = self;

    int rc = uv_tcp_init(&l->loop, &s->tcp);
    if (rc < 0) {
        free(s);
        return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: uv_tcp_init failed");
    }
    s->tcp.data = s;

    struct sockaddr_in addr;
    rc = uv_ip4_addr(host, port, &addr);
    if (rc < 0) {
        /* uv_ip4_addr fails on hostnames; resolve asynchronously via
         * uv_getaddrinfo so the libuv loop keeps servicing other
         * sockets while DNS is in flight. A synchronous getaddrinfo
         * here would freeze every other connection on the loop while
         * the resolver waits — even a misconfigured local DNS turns
         * one slow lookup into a full event-loop stall. */
        struct resolve_state rs = {0};
        rs.coro = self;
        rs.req.data = &rs;
        struct addrinfo hints = {0};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char portbuf[16];
        snprintf(portbuf, sizeof(portbuf), "%d", port);
        int dr = uv_getaddrinfo(&l->loop, &rs.req, on_resolve,
                                host, portbuf, &hints);
        if (dr < 0) {
            uv_close((uv_handle_t *)&s->tcp, on_handle_close);
            return YAAFC_ERR(yloop_stream_ptr,
                             "yloop_connect_tcp: uv_getaddrinfo dispatch failed");
        }
        yaafc_coro_yield();
        if (rs.status < 0 || !rs.got_addr) {
            uv_close((uv_handle_t *)&s->tcp, on_handle_close);
            return YAAFC_ERR(yloop_stream_ptr,
                             "yloop_connect_tcp: getaddrinfo failed");
        }
        addr = rs.addr;
    }

    struct connect_state cs = {0};
    cs.coro = self;
    cs.req.data = &cs;
    rc = uv_tcp_connect(&cs.req, &s->tcp, (const struct sockaddr *)&addr, on_connect);
    if (rc < 0) {
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: uv_tcp_connect dispatch failed");
    }
    yaafc_coro_yield();
    if (cs.status < 0) {
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: connect failed");
    }
    uv_tcp_nodelay(&s->tcp, 1);

    rc = uv_read_start((uv_stream_t *)&s->tcp, on_alloc, on_read);
    if (rc < 0) {
        uv_close((uv_handle_t *)&s->tcp, on_handle_close);
        return YAAFC_ERR(yloop_stream_ptr, "yloop_connect_tcp: uv_read_start failed");
    }
    return YAAFC_OK(yloop_stream_ptr, s);
}

/* ---- blocking-work executor (libuv thread pool) --------------------
 *
 * Run a blocking function on libuv's worker pool and suspend the
 * calling coroutine until it finishes — the loop thread stays free to
 * service other coroutines meanwhile. This is asyncio's
 * `run_in_executor` shape (offload → await → resume), the mechanism for
 * keeping blocking work (sqlite/mdbx queries, libgit2, filesystem) off
 * the event-loop hot path.
 *
 * `work` runs on a POOL thread: it must touch only its own `arg` and no
 * loop-thread state. The completion callback resumes the coro on the
 * loop thread. Called outside a coroutine (bootstrap / tests), it runs
 * inline. */
struct blocking_work {
    uv_work_t req;
    struct yaafc_coro *coro;
    void (*work)(void *);
    void *arg;
};

static void on_blocking_work(uv_work_t *req)
{
    struct blocking_work *bw = req->data;
    bw->work(bw->arg);
}

static void on_blocking_done(uv_work_t *req, int status)
{
    (void)status;
    struct blocking_work *bw = req->data;
    yaafc_coro_resume(bw->coro);
}

struct yaafc_void_result yloop_run_blocking(struct yloop *l, void (*work)(void *), void *arg)
{
    if (!l || !work)
        return YAAFC_ERR(yaafc_void, "yloop_run_blocking: bad args");

    struct yaafc_coro *self = yaafc_coro_current();
    if (!self) {
        /* Nothing to suspend (mesh parent, unit tests): run it here. */
        work(arg);
        return YAAFC_OK_VOID();
    }

    struct blocking_work bw = {.coro = self, .work = work, .arg = arg};
    bw.req.data = &bw; /* lives on the suspended coro's stack until resume */
    int rc = uv_queue_work(&l->loop, &bw.req, on_blocking_work, on_blocking_done);
    if (rc < 0) {
        /* Pool refused the job — fall back to inline rather than fail. */
        work(arg);
        return YAAFC_OK_VOID();
    }
    yaafc_coro_yield();
    return YAAFC_OK_VOID();
}

struct yaafc_void_result yloop_listen_tcp(struct yloop *l, const char *host, int port,
                                          yloop_serve_fn serve, void *ud)
{
    if (!l || !host || !serve) {
        return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: bad args");
    }
    struct yloop_listener *L = calloc(1, sizeof(*L));
    if (!L) return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: calloc failed");
    L->owner = l;
    L->serve = serve;
    L->ud = ud;

    int rc = uv_tcp_init(&l->loop, &L->tcp);
    if (rc < 0) {
        free(L);
        return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: uv_tcp_init failed");
    }
    L->tcp.data = L;

    struct sockaddr_in addr;
    rc = uv_ip4_addr(host, port, &addr);
    if (rc < 0) {
        return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: uv_ip4_addr failed");
    }
    rc = uv_tcp_bind(&L->tcp, (const struct sockaddr *)&addr, 0);
    if (rc < 0) {
        return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: uv_tcp_bind failed");
    }
    rc = uv_listen((uv_stream_t *)&L->tcp, 128, on_connection);
    if (rc < 0) {
        return YAAFC_ERR(yaafc_void, "yloop_listen_tcp: uv_listen failed");
    }
    yinfo("yloop: listening on %s:%d", host, port);
    return YAAFC_OK_VOID();
}
