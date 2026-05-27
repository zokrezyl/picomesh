/* yrpc — binary RPC frontend.
 *
 * The frontend itself is stateless: the listen call is fire-and-forget
 * on the engine's yloop, and the serve coroutine drives
 * `rpc_server_run_io` against the stream until the peer closes. The
 * returned `struct yrpc_frontend` is a tiny handle so callers can stop
 * the listener cleanly. */

#include <yaafc/frontends/yrpc/yrpc.h>
#include <yaafc/yengine/engine.h>
#include <yaafc/yloop/yloop.h>
#include <yaafc/yclass/rpc.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>

#include <stdlib.h>

struct yrpc_frontend {
    struct yaafc_engine *engine;
};

static size_t stream_read_cb(void *ud, void *buf, size_t n)
{
    return yloop_read((struct yloop_stream *)ud, buf, n);
}

static size_t stream_write_cb(void *ud, const void *buf, size_t n)
{
    return yloop_write((struct yloop_stream *)ud, buf, n);
}

static void serve_one(struct yloop *l, struct yloop_stream *s, void *ud)
{
    (void)l; (void)ud;
    yinfo("yrpc: peer connected");
    rpc_server_run_io(s, stream_read_cb, stream_write_cb);
    yinfo("yrpc: peer disconnected");
}

struct yrpc_frontend_ptr_result yrpc_start(struct yaafc_engine *e,
                                           const struct yrpc_config *cfg)
{
    if (!e) return YAAFC_ERR(yrpc_frontend_ptr, "yrpc_start: NULL engine");
    const char *host = (cfg && cfg->host) ? cfg->host : "127.0.0.1";
    int port = (cfg && cfg->port > 0) ? cfg->port : 7777;

    struct yloop *l = yaafc_engine_loop(e);
    if (!l) return YAAFC_ERR(yrpc_frontend_ptr, "yrpc_start: engine has no loop");

    struct yaafc_void_result lr = yloop_listen_tcp(l, host, port, serve_one, NULL);
    if (YAAFC_IS_ERR(lr)) {
        return YAAFC_ERR(yrpc_frontend_ptr, "yrpc_start: yloop_listen_tcp failed", lr);
    }

    struct yrpc_frontend *f = calloc(1, sizeof(*f));
    if (!f) return YAAFC_ERR(yrpc_frontend_ptr, "yrpc_start: calloc failed");
    f->engine = e;
    yinfo("yrpc: listening on %s:%d", host, port);
    return YAAFC_OK(yrpc_frontend_ptr, f);
}

void yrpc_stop(struct yrpc_frontend *f)
{
    /* The listener is owned by the yloop and will be torn down when
     * the loop closes. The frontend handle itself just goes away. */
    if (!f) return;
    free(f);
}
