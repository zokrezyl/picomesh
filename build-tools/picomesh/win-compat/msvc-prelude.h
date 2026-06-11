/* MSVC prelude — force-included ahead of every translation unit on WIN32
 * (see the root CMakeLists /FI). Maps GCC/clang keywords and the POSIX clock
 * the sources use directly, so they are available in EVERY TU regardless of
 * which headers it includes (e.g. files that pull <time.h> but not the
 * unistd shim for CLOCK_REALTIME). Deliberately free of <windows.h> so it
 * can't disturb <winsock2.h>-before-<windows.h> ordering — the few Win32
 * calls are forward-declared. */

#ifndef PICOMESH_MSVC_PRELUDE_H
#define PICOMESH_MSVC_PRELUDE_H

#include <time.h> /* struct timespec (C11) */

/* GCC/clang thread-local storage keyword -> MSVC. */
#define __thread __declspec(thread)

/* ---- POSIX monotonic/realtime clock ---- */
typedef int clockid_t;
#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#endif

__declspec(dllimport) int __stdcall QueryPerformanceCounter(long long *count);
__declspec(dllimport) int __stdcall QueryPerformanceFrequency(long long *freq);
__declspec(dllimport) void __stdcall GetSystemTimePreciseAsFileTime(
    unsigned long long *filetime);

static __inline int clock_gettime(clockid_t clock_id, struct timespec *ts) {
  if (clock_id == CLOCK_MONOTONIC) {
    long long freq = 0, count = 0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    if (freq == 0)
      return -1;
    ts->tv_sec = (time_t)(count / freq);
    ts->tv_nsec = (long)(((count % freq) * 1000000000LL) / freq);
  } else {
    /* FILETIME: 100 ns ticks since 1601-01-01; offset to the Unix epoch. */
    unsigned long long filetime = 0;
    GetSystemTimePreciseAsFileTime(&filetime);
    unsigned long long unix100ns = filetime - 116444736000000000ULL;
    ts->tv_sec = (time_t)(unix100ns / 10000000ULL);
    ts->tv_nsec = (long)((unix100ns % 10000000ULL) * 100ULL);
  }
  return 0;
}

#endif /* PICOMESH_MSVC_PRELUDE_H */
