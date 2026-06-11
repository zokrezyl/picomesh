/* random/windows.c — Windows CSPRNG backend.
 *
 * Uses rand_s(), the CRT wrapper over RtlGenRandom (the system CSPRNG).
 * It needs _CRT_RAND_S defined before <stdlib.h> and pulls no extra
 * import library (unlike BCryptGenRandom, which would need bcrypt.lib),
 * keeping the picomesh.exe link self-contained. */

#define _CRT_RAND_S

#include <picomesh/platform/random.h>

#include <stdlib.h>

int picomesh_platform_random_bytes(void *buf, size_t len) {
  unsigned char *out = (unsigned char *)buf;
  size_t off = 0;
  while (off < len) {
    unsigned int value;
    if (rand_s(&value) != 0)
      return -1;
    size_t chunk = len - off;
    if (chunk > sizeof(value))
      chunk = sizeof(value);
    for (size_t i = 0; i < chunk; ++i)
      out[off + i] = (unsigned char)(value >> (8 * i));
    off += chunk;
  }
  return 0;
}
