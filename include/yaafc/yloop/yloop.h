/* yloop — libuv-backed event loop + coroutine-yielding streams.
 *
 * The handler you register (yloop_listen_tcp's `serve`) is invoked once
 * per accepted connection, running inside its own coroutine. From that
 * coroutine you call yloop_read / yloop_write as if they were blocking;
 * they yield the coro on EAGAIN and resume when libuv signals readiness
 * / write completion. */

#ifndef YAAFC_YLOOP_YLOOP_H
#define YAAFC_YLOOP_YLOOP_H

#include <yaafc/ycore/result.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yloop;
struct yloop_stream;
struct yaafc_coro;

YAAFC_RESULT_DECLARE(yloop_ptr, struct yloop *);

/* Queue a coroutine whose entry has returned for deferred destruction.
 * A coro can't free its own stack while running on it, so a handler
 * coro's last act is to hand itself here; the loop destroys it on the
 * next iteration once it observes the coro as finished. Safe to call
 * from inside the coro being reaped. */
void yloop_reap_coro(struct yloop *l, struct yaafc_coro *coro);

typedef void (*yloop_serve_fn)(struct yloop *l, struct yloop_stream *s, void *ud);

/* Owner of the libuv loop. Single-threaded; all callbacks fire on the
 * loop thread. */
struct yloop_ptr_result yloop_create(void);
void yloop_destroy(struct yloop *l);

/* Run the loop until yloop_stop() is called or there's nothing left. */
struct yaafc_void_result yloop_run(struct yloop *l);

/* Run `work(arg)` on libuv's worker thread pool, suspending the calling
 * coroutine until it completes (asyncio `run_in_executor` shape). Keeps
 * blocking work — DB queries, libgit2, filesystem — off the loop thread
 * so other coroutines keep running. `work` runs on another thread: it
 * must touch only `arg`. Outside a coroutine it runs inline. */
struct yaafc_void_result yloop_run_blocking(struct yloop *l, void (*work)(void *), void *arg);

/* Drain pending work, then exit yloop_run. Safe to call from a serve coro. */
void yloop_stop(struct yloop *l);

/* Bind + listen on host:port. For each accepted connection, spawn a
 * coroutine running `serve(yloop, stream, ud)`. The serve function owns
 * the stream and must call yloop_close before returning (or yloop will
 * leak it). */
struct yaafc_void_result yloop_listen_tcp(struct yloop *l, const char *host, int port,
                                          yloop_serve_fn serve, void *ud);

/* Outgoing TCP. Connects to host:port; the calling coroutine yields
 * until the handshake completes. On success returns the stream (caller
 * owns; must yloop_close); on failure returns an error result. Must be
 * called from inside a coroutine running on `l`. */
YAAFC_RESULT_DECLARE(yloop_stream_ptr, struct yloop_stream *);
struct yloop_stream_ptr_result yloop_connect_tcp(struct yloop *l,
                                                 const char *host, int port);

/* Read exactly n bytes into buf. Yields the calling coro until satisfied.
 * Returns the number of bytes actually read — 0 on EOF, < n only at EOF. */
size_t yloop_read(struct yloop_stream *s, void *buf, size_t n);

/* Read up to `cap` bytes into buf — returns whatever is available
 * once at least one byte arrives (or 0 on EOF). For stream parsers
 * (HTTP, line protocols) where you don't know the exact frame length
 * ahead of time. */
size_t yloop_read_some(struct yloop_stream *s, void *buf, size_t cap);

/* Write all n bytes. Yields the calling coro until the write completes.
 * Returns the number of bytes actually written (n on success, 0 on error). */
size_t yloop_write(struct yloop_stream *s, const void *buf, size_t n);

/* Tear the stream down. Idempotent. */
void yloop_close(struct yloop_stream *s);

/* Suspend the current coroutine for `ms` milliseconds. Backed by
 * uv_timer_t — the loop thread keeps servicing other I/O while this
 * coro waits. Must be called from inside a coroutine spawned on the
 * loop (e.g. a serve coro from yloop_listen_tcp). */
void yloop_sleep_ms(struct yloop *l, unsigned int ms);

/* ---- subprocess spawn (libuv uv_spawn wrapper) ----------------- */

struct yloop_process;

/* Called when the child exits or is killed. `exit_status` is the
 * child's exit code; `term_signal` is non-zero if killed by signal.
 * The yloop_process is freed AFTER the callback returns. */
typedef void (*yloop_process_exit_cb)(struct yloop_process *p,
                                      int64_t exit_status,
                                      int term_signal,
                                      void *ud);

/* Spawn `file` with `argv` (NULL-terminated). Returns the child PID
 * on success, 0 on failure. The yloop owns the uv_process_t until
 * the exit callback fires.
 *
 * stdin/stdout/stderr are inherited from the parent — the child can
 * print to the parent's terminal. */
int yloop_spawn(struct yloop *l, const char *file, char *const argv[],
                yloop_process_exit_cb on_exit, void *ud);

/* Send `signum` to the named pid (which the loop's spawn returned).
 * Returns 0 on success, non-zero on error. */
int yloop_kill(struct yloop *l, int pid, int signum);

#ifdef __cplusplus
}
#endif

#endif /* YAAFC_YLOOP_YLOOP_H */
