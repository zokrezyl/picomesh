/* Windows compat shim for <pthread.h> (POSIX threads) — the subset picomesh
 * uses, mapped onto the Win32 primitives. Only on the include path for WIN32
 * builds (see the root CMakeLists), so it never shadows the real pthread.h on
 * POSIX.
 *
 * Mapping:
 *   pthread_mutex_t  -> SRWLOCK              (non-recursive, matches default)
 *   pthread_cond_t   -> CONDITION_VARIABLE
 *   pthread_key_t    -> FLS index           (FlsAlloc runs the destructor)
 *   pthread_t        -> thread HANDLE
 *
 * PTHREAD_MUTEX_INITIALIZER maps to SRWLOCK_INIT ({0}), which is a valid
 * static initializer usable without an explicit init call — picomesh relies
 * on that for its file-scope/static mutexes. */

#ifndef PICOMESH_WIN_COMPAT_PTHREAD_H
#define PICOMESH_WIN_COMPAT_PTHREAD_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <process.h> /* _beginthreadex */
#include <stdlib.h>  /* malloc/free */

typedef SRWLOCK pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef DWORD pthread_key_t;
typedef HANDLE pthread_t;

/* Attribute types are only ever passed as NULL / left unused. */
typedef int pthread_attr_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

/* ---- mutex ---- */
static inline int pthread_mutex_init(pthread_mutex_t *mutex,
                                     const pthread_mutexattr_t *attr) {
  (void)attr;
  InitializeSRWLock(mutex);
  return 0;
}
static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
  (void)mutex; /* SRWLOCK needs no teardown */
  return 0;
}
static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
  AcquireSRWLockExclusive(mutex);
  return 0;
}
static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  ReleaseSRWLockExclusive(mutex);
  return 0;
}

/* ---- condition variable ---- */
static inline int pthread_cond_init(pthread_cond_t *cond,
                                    const pthread_condattr_t *attr) {
  (void)attr;
  InitializeConditionVariable(cond);
  return 0;
}
static inline int pthread_cond_destroy(pthread_cond_t *cond) {
  (void)cond;
  return 0;
}
static inline int pthread_cond_wait(pthread_cond_t *cond,
                                    pthread_mutex_t *mutex) {
  return SleepConditionVariableSRW(cond, mutex, INFINITE, 0) ? 0 : -1;
}
static inline int pthread_cond_signal(pthread_cond_t *cond) {
  WakeConditionVariable(cond);
  return 0;
}
static inline int pthread_cond_broadcast(pthread_cond_t *cond) {
  WakeAllConditionVariable(cond);
  return 0;
}

/* ---- thread-specific data (keys) ---- */
static inline int pthread_key_create(pthread_key_t *key,
                                     void (*destructor)(void *)) {
  /* FlsAlloc runs the destructor on thread/fiber exit, matching the pthread
   * key contract (TlsAlloc would not). x64 has a single calling convention,
   * so the cdecl destructor is ABI-compatible with PFLS_CALLBACK_FUNCTION. */
  DWORD index = FlsAlloc((PFLS_CALLBACK_FUNCTION)destructor);
  if (index == FLS_OUT_OF_INDEXES)
    return -1;
  *key = index;
  return 0;
}
static inline int pthread_key_delete(pthread_key_t key) {
  return FlsFree(key) ? 0 : -1;
}
static inline int pthread_setspecific(pthread_key_t key, const void *value) {
  return FlsSetValue(key, (PVOID)value) ? 0 : -1;
}
static inline void *pthread_getspecific(pthread_key_t key) {
  return FlsGetValue(key);
}

/* ---- thread lifecycle ---- */
struct picomesh_pthread_thunk {
  void *(*start_routine)(void *);
  void *arg;
};
static unsigned __stdcall picomesh_pthread_trampoline(void *raw) {
  struct picomesh_pthread_thunk thunk = *(struct picomesh_pthread_thunk *)raw;
  free(raw);
  thunk.start_routine(thunk.arg);
  return 0;
}
static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                                 void *(*start_routine)(void *), void *arg) {
  (void)attr;
  struct picomesh_pthread_thunk *thunk =
      (struct picomesh_pthread_thunk *)malloc(sizeof(*thunk));
  if (!thunk)
    return -1;
  thunk->start_routine = start_routine;
  thunk->arg = arg;
  uintptr_t handle =
      _beginthreadex(NULL, 0, picomesh_pthread_trampoline, thunk, 0, NULL);
  if (handle == 0) {
    free(thunk);
    return -1;
  }
  *thread = (HANDLE)handle;
  return 0;
}
static inline int pthread_join(pthread_t thread, void **retval) {
  WaitForSingleObject(thread, INFINITE);
  if (retval)
    *retval = NULL;
  CloseHandle(thread);
  return 0;
}
static inline pthread_t pthread_self(void) {
  return GetCurrentThread();
}

#endif /* PICOMESH_WIN_COMPAT_PTHREAD_H */
