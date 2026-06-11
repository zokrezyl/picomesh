/* random/posix.c — POSIX CSPRNG backend.
 *
 * Linux uses getrandom(2) (glibc wrapper over the syscall). macOS and the
 * BSDs don't expose getrandom but do provide getentropy(3), which fills up
 * to 256 bytes per call from the same kernel CSPRNG — chunk for larger
 * requests. Both block only until the pool is initialised, then never
 * again, which is what callers minting session ids / tokens want. */

/* getrandom() needs _GNU_SOURCE / _DEFAULT_SOURCE; the build defines
 * _GNU_SOURCE tree-wide, so no local define is needed here. */

#include <picomesh/platform/random.h>

#include <errno.h>
#include <sys/random.h>

int picomesh_platform_random_bytes(void *buf, size_t len) {
  unsigned char *out = (unsigned char *)buf;
  size_t off = 0;
  while (off < len) {
#if defined(__linux__)
    ssize_t got = getrandom(out + off, len - off, 0);
    if (got < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    off += (size_t)got;
#else
    /* getentropy fills at most 256 bytes and is all-or-nothing per call. */
    size_t chunk = len - off;
    if (chunk > 256)
      chunk = 256;
    if (getentropy(out + off, chunk) != 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    off += chunk;
#endif
  }
  return 0;
}
