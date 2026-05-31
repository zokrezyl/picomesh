/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `calculator`.
 * NEVER include this from outside src/picomesh/plugins/calculator/. */
#ifndef PICOMESH_CALCULATOR_INTERNAL_H
#define PICOMESH_CALCULATOR_INTERNAL_H

#include <picomesh/plugin/calculator/calculator.h>

typedef struct picomesh_int64_result (*calculator_calc_add_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_int64_result (*calculator_calc_sub_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_int64_result (*calculator_calc_mul_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);
typedef struct picomesh_int64_result (*calculator_calc_div_fn)(struct ctx *, struct object *, struct yheaders *, int64_t, int64_t);

#endif
