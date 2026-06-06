/* picomesh_allocator — per-thread, size-classed pool allocator.
 *
 * Purpose: keep large, short-lived scratch buffers (the yrpc wire arg/response
 * buffers) OFF the stack and OFF the process allocator's hot path. The yrpc
 * stubs used to declare `uint8_t _wbuf[65536]` / `_a[16384]` as locals, so each
 * in-process service hop reserved ~90 KiB of stack frame; a few nested hops in
 * the collocated/all-in-one deployment overflowed the fixed coroutine stack.
 * Routing those buffers through this allocator drops each hop's frame to a
 * pointer and recycles the storage instead of malloc/free-ing per call.
 *
 * Model: one allocator per THREAD (get it with picomesh_allocator_thread()).
 * It pulls big CHUNKS from the process allocator once, carves blocks out of
 * them, and keeps a free-list per size class. Every request is rounded UP to a
 * multiple of PICOMESH_ALLOC_BASE; that multiple is the size class. Blocks
 * larger than the largest class fall through to a direct malloc (tracked so
 * free still works).
 *
 * THREADING: the allocator is NOT thread-safe and needs no lock BECAUSE it is
 * thread-local — a block MUST be alloc'd and freed on the same thread. Never
 * hand a block from one thread to another (e.g. a loop thread to a worker):
 * freeing it on the other thread would push onto the wrong free-list. Within a
 * thread, many coroutines may each hold their own block at once (a coroutine
 * keeps its block across a yield); each alloc returns a distinct block, so that
 * is fine. */

#ifndef PICOMESH_ALLOCATOR_ALLOCATOR_H
#define PICOMESH_ALLOCATOR_ALLOCATOR_H

#include <stddef.h>

/* Size-class granularity. Every allocation is rounded up to a multiple of this.
 * 4096 == the page size: the yrpc buffers (4K/8K/16K/64K) are exact multiples
 * (zero rounding waste on the hot path), worst-case waste is one base, and it
 * is friendly to the page-granular chunks the process allocator hands back. */
#define PICOMESH_ALLOC_BASE 4096u

/* Largest pooled size class: PICOMESH_ALLOC_MAX_CLASS * PICOMESH_ALLOC_BASE
 * bytes (16 * 4096 == 64 KiB, the yrpc frame max). Requests above this are
 * served by a direct malloc and freed back on release. */
#define PICOMESH_ALLOC_MAX_CLASS 16u

struct picomesh_allocator;

/* Create / destroy an allocator explicitly. Most callers want the thread-local
 * one (picomesh_allocator_thread) instead. destroy releases every chunk. */
struct picomesh_allocator *picomesh_allocator_create(void);
void picomesh_allocator_destroy(struct picomesh_allocator *pool);

/* The calling thread's allocator, created lazily on first use. Lives for the
 * thread's lifetime. */
struct picomesh_allocator *picomesh_allocator_thread(void);

/* Allocate `size` bytes (rounded up to a multiple of PICOMESH_ALLOC_BASE).
 * Returns NULL only on out-of-memory. Free with picomesh_allocator_free on the
 * SAME thread. The returned storage is uninitialised. */
void *picomesh_allocator_alloc(struct picomesh_allocator *pool, size_t size);

/* Return a block obtained from picomesh_allocator_alloc to its pool. `ptr` may
 * be NULL (no-op). Must run on the thread that allocated it. */
void picomesh_allocator_free(struct picomesh_allocator *pool, void *ptr);

#endif /* PICOMESH_ALLOCATOR_ALLOCATOR_H */
