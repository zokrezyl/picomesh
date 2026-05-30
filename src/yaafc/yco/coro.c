/* yaafc/yco/coro.c — libco-backed coroutine primitive.
 *
 * libco does the stack-switch and tracks the "active" thread; we layer
 * id/name/status/finished bookkeeping on top so callers can introspect
 * a coroutine the way the design doc describes. */

#include <yaafc/yco/coro.h>
#include <yaafc/ycore/ytrace.h>

#include <libco.h>

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_STACK_SIZE (256 * 1024)

struct yaafc_coro {
    cothread_t thread;
    void *arg;
    yaafc_coro_entry entry;
    cothread_t resumer; /* who to switch back to on yield */
    unsigned int id;
    char *name;
    int finished;
    int status;
};

/* The currently-running coroutine handle. The libco "active" thread is
 * the main stack when this is NULL, otherwise this object's thread.
 *
 * THREAD-LOCAL: each worker thread runs its own libuv loop and its own
 * libco scheduler, so each needs an independent "current coroutine".
 * libco itself tracks its active context per-thread (co_active_handle is
 * thread_local in every backend), so this companion bookkeeping must be
 * thread-local too — otherwise two worker threads switching coroutines
 * concurrently would clobber each other's notion of who is running. */
static struct yaafc_coro **current_slot(void)
{
    static _Thread_local struct yaafc_coro *cur = NULL;
    return &cur;
}

/* Coroutine ids are purely for tracing/introspection. They're handed out
 * across all worker threads, so the counter is atomic to stay race-free
 * (and globally unique) without a lock. */
static unsigned int next_id(void)
{
    static atomic_uint n = 0;
    return atomic_fetch_add_explicit(&n, 1, memory_order_relaxed) + 1;
}

static void coro_trampoline(void)
{
    /* libco entry trampoline. The currently-active coroutine is `cur`
     * — we set it just before co_switch'ing in. After entry returns
     * we mark finished, restore `cur` to NULL, and switch back to
     * whoever resumed us. Subsequent resumes are no-ops via the
     * is_finished check in yaafc_coro_resume. */
    struct yaafc_coro *self = *current_slot();
    if (!self) return;
    self->entry(self->arg);
    self->finished = 1;
    cothread_t back = self->resumer;
    *current_slot() = NULL;
    co_switch(back);
}

struct yaafc_coro_ptr_result yaafc_coro_spawn(yaafc_coro_entry entry, void *arg,
                                              size_t stack_hint, const char *name)
{
    if (!entry) return YAAFC_ERR(yaafc_coro_ptr, "yaafc_coro_spawn: NULL entry");
    struct yaafc_coro *c = calloc(1, sizeof(*c));
    if (!c) return YAAFC_ERR(yaafc_coro_ptr, "yaafc_coro_spawn: calloc failed");
    c->entry = entry;
    c->arg = arg;
    c->id = next_id();
    c->name = name ? strdup(name) : NULL;
    size_t stack = stack_hint ? stack_hint : DEFAULT_STACK_SIZE;
    c->thread = co_create((unsigned int)stack, coro_trampoline);
    if (!c->thread) {
        free(c->name);
        free(c);
        return YAAFC_ERR(yaafc_coro_ptr, "yaafc_coro_spawn: co_create failed");
    }
    ydebug("spawn id=%u name=%s stack=%zu thread=%p", c->id,
           c->name ? c->name : "(anon)", stack, c->thread);
    return YAAFC_OK(yaafc_coro_ptr, c);
}

void yaafc_coro_yield(void)
{
    struct yaafc_coro *self = *current_slot();
    if (!self) {
        ywarn("yaafc_coro_yield: called from main stack — no-op");
        return;
    }
    cothread_t back = self->resumer;
    *current_slot() = NULL;
    co_switch(back);
    /* When we come back, this coroutine is active again. */
    *current_slot() = self;
}

void yaafc_coro_resume(struct yaafc_coro *coro)
{
    if (!coro || coro->finished) return;
    /* Save and restore the caller's "current" around the switch so that
     * resume works when called from WITHIN another coroutine (nested
     * resume) — e.g. a cooperative lock handing off to the next waiter.
     * The resumed coro clears current_slot to NULL when it yields/
     * finishes; without restoring here, the nesting caller would keep
     * running with current_slot == NULL and its next yloop_read/write
     * would latch the wrong (NULL) coro, losing the wakeup. When resume
     * is called from the loop's main stack `prev` is NULL, so existing
     * callers are unaffected. */
    struct yaafc_coro *prev = *current_slot();
    coro->resumer = co_active();
    *current_slot() = coro;
    co_switch(coro->thread);
    *current_slot() = prev;
}

void yaafc_coro_destroy(struct yaafc_coro *coro)
{
    if (!coro) return;
    if (!coro->finished) {
        ywarn("yaafc_coro_destroy: destroying unfinished coro id=%u", coro->id);
    }
    if (coro->thread) co_delete(coro->thread);
    free(coro->name);
    free(coro);
}

struct yaafc_coro *yaafc_coro_current(void)
{
    return *current_slot();
}

int yaafc_coro_is_finished(const struct yaafc_coro *coro)
{
    return coro ? coro->finished : 1;
}

unsigned int yaafc_coro_id(const struct yaafc_coro *coro)
{
    return coro ? coro->id : 0;
}

const char *yaafc_coro_name(const struct yaafc_coro *coro)
{
    return coro && coro->name ? coro->name : NULL;
}

void yaafc_coro_set_status(struct yaafc_coro *coro, int status)
{
    if (coro) coro->status = status;
}

int yaafc_coro_get_status(const struct yaafc_coro *coro)
{
    return coro ? coro->status : 0;
}
