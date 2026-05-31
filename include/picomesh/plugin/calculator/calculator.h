/* GENERATED — do not edit. */
/* Public interface for plugin `calculator` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/calculator/. */
#ifndef PICOMESH_PLUGIN_CALCULATOR_H
#define PICOMESH_PLUGIN_CALCULATOR_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int64_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result calculator_calc_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result calculator_calc_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_int64_result calculator_calc_add(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t x, int64_t y);
struct picomesh_int64_result calculator_calc_sub(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t x, int64_t y);
struct picomesh_int64_result calculator_calc_mul(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t x, int64_t y);
struct picomesh_int64_result calculator_calc_div(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t x, int64_t y);

/* ---- activation ---- */
void picomesh_plugin_calculator_register(void);

#endif
