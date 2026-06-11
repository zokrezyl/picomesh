/* Windows compat shim for <strings.h> (POSIX).
 *
 * MSVC has no <strings.h>; the case-insensitive comparisons it declares live
 * in <string.h> under the _str*i* spelling. This shim is only on the include
 * path for WIN32 builds (see the root CMakeLists), so it never shadows the
 * real header on POSIX. */

#ifndef PICOMESH_WIN_COMPAT_STRINGS_H
#define PICOMESH_WIN_COMPAT_STRINGS_H

#include <string.h>

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

#endif /* PICOMESH_WIN_COMPAT_STRINGS_H */
