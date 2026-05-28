/* GENERATED — do not edit. */
#ifndef YAAFC_CALCULATOR_METHODS_GEN_H
#define YAAFC_CALCULATOR_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int64_result;

struct yaafc_int64_result calculator_calc_add(struct ctx * ctx, struct object * obj, int64_t x, int64_t y);
typedef struct yaafc_int64_result (*calculator_calc_add_fn)(struct ctx *, struct object *, int64_t, int64_t);
struct yaafc_int64_result calculator_calc_sub(struct ctx * ctx, struct object * obj, int64_t x, int64_t y);
typedef struct yaafc_int64_result (*calculator_calc_sub_fn)(struct ctx *, struct object *, int64_t, int64_t);
struct yaafc_int64_result calculator_calc_mul(struct ctx * ctx, struct object * obj, int64_t x, int64_t y);
typedef struct yaafc_int64_result (*calculator_calc_mul_fn)(struct ctx *, struct object *, int64_t, int64_t);
struct yaafc_int64_result calculator_calc_div(struct ctx * ctx, struct object * obj, int64_t x, int64_t y);
typedef struct yaafc_int64_result (*calculator_calc_div_fn)(struct ctx *, struct object *, int64_t, int64_t);

#endif
