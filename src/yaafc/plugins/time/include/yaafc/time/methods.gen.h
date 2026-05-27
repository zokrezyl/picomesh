/* GENERATED — do not edit. */
#ifndef YAAFC_TIME_METHODS_GEN_H
#define YAAFC_TIME_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int64_result;

struct yaafc_int64_result time_clock_now_ms(struct ctx * ctx, struct object * obj);
typedef struct yaafc_int64_result (*time_clock_now_ms_fn)(struct ctx *, struct object *);
struct yaafc_int64_result time_clock_sleep_ms(struct ctx * ctx, struct object * obj, uint32_t ms);
typedef struct yaafc_int64_result (*time_clock_sleep_ms_fn)(struct ctx *, struct object *, uint32_t);

#endif
