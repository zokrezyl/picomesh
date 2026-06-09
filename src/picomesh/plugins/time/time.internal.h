/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `time`.
 * NEVER include this from outside src/picomesh/plugins/time/. */
#ifndef PICOMESH_TIME_INTERNAL_H
#define PICOMESH_TIME_INTERNAL_H

#include <picomesh/plugin/time/time.h>

typedef struct picomesh_int64_result (*time_clock_now_ms_fn)(struct ctx *,
                                                             struct object *,
                                                             struct yheaders *);
typedef struct picomesh_int64_result (*time_clock_sleep_ms_fn)(
    struct ctx *, struct object *, struct yheaders *, uint32_t);

#endif
