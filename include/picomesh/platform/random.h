/* platform/random.h — cross-platform cryptographically-secure randomness.
 *
 * One call, no platform headers in callers. Backends live under
 * src/picomesh/platform/random/<platform>.c — CMake selects one based
 * on the target:
 *   - posix.c   : Linux getrandom(2); macOS/BSD getentropy(3).
 *   - windows.c : the CRT CSPRNG (rand_s / RtlGenRandom).
 *
 * Replaces direct <sys/random.h> getrandom() use, which is Linux-only
 * (macOS has <sys/random.h> but no getrandom symbol; Windows has
 * neither). */

#ifndef PICOMESH_PLATFORM_RANDOM_H
#define PICOMESH_PLATFORM_RANDOM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fill `buf` with `len` cryptographically-secure random bytes, retrying
 * partial reads / EINTR internally. Returns 0 on success, -1 on failure
 * (in which case the contents of `buf` are unspecified). All-or-nothing:
 * a 0 return guarantees every byte was written. */
int picomesh_platform_random_bytes(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_PLATFORM_RANDOM_H */
