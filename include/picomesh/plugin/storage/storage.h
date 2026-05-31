/* GENERATED — do not edit. */
/* Public interface for plugin `storage` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/storage/. */
#ifndef PICOMESH_PLUGIN_STORAGE_H
#define PICOMESH_PLUGIN_STORAGE_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct picomesh_string_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result storage_kv_class_get(void);
struct class_ptr_result storage_db_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result storage_kv_create(struct ctx *ctx);
struct object_ptr_result storage_db_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_int_result storage_kv_set(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * key, const char * value);
struct picomesh_string_result storage_kv_get(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * key);
struct picomesh_size_result storage_kv_count(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_int_result storage_set(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key, const char * value);
struct picomesh_string_result storage_get(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key);
struct picomesh_int_result storage_exists(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key);
struct picomesh_int_result storage_del(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key);
struct picomesh_size_result storage_count(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context);

/* ---- activation ---- */
void picomesh_plugin_storage_register(void);

#endif
