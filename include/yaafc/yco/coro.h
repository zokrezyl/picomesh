/* yaafc/yco/coro.h - Coroutine primitive.
 *
 * Real stackful coroutines via libco. spawn allocates a stack; resume
 * switches into it; yield switches back to whoever resumed.
 *
 * Resume must be called on the loop thread. Cross-thread wakeups (e.g.
 * a worker thread reporting I/O readiness) post a request via the
 * event loop and the loop thread invokes resume.
 *
 * Cancellation is not modelled — if an owner is destroyed while a
 * coroutine is suspended in an `_await` wrapper, the resume callback
 * touches freed memory. Match the lifetime of the coroutine to the
 * thing it operates on.
 *
 * Every fallible entry returns a Result (see <yaafc/ycore/result.h>). */

#ifndef YAAFC_YCO_CORO_H
#define YAAFC_YCO_CORO_H

#include <yaafc/ycore/result.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yaafc_coro;

YAAFC_RESULT_DECLARE(yaafc_coro_ptr, struct yaafc_coro *);

typedef void (*yaafc_coro_entry)(void *arg);

/* Spawn a coroutine. Does not start it; call yaafc_coro_resume to run.
 * stack_hint of 0 → libco default. name is copied; may be NULL. */
struct yaafc_coro_ptr_result yaafc_coro_spawn(yaafc_coro_entry entry, void *arg,
                                              size_t stack_hint, const char *name);

void yaafc_coro_yield(void);
void yaafc_coro_resume(struct yaafc_coro *coro);
void yaafc_coro_destroy(struct yaafc_coro *coro);

struct yaafc_coro *yaafc_coro_current(void);
int yaafc_coro_is_finished(const struct yaafc_coro *coro);

unsigned int yaafc_coro_id(const struct yaafc_coro *coro);
const char *yaafc_coro_name(const struct yaafc_coro *coro);

void yaafc_coro_set_status(struct yaafc_coro *coro, int status);
int yaafc_coro_get_status(const struct yaafc_coro *coro);

#ifdef __cplusplus
}
#endif

#endif /* YAAFC_YCO_CORO_H */
