/* GENERATED — do not edit. */
/* Public interface for plugin `session` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/session/. */
#ifndef PICOMESH_PLUGIN_SESSION_H
#define PICOMESH_PLUGIN_SESSION_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct picomesh_string_result;
struct picomesh_uint32_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result session_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result session_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_string_result session_store_start(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, uint32_t provider_id);
struct picomesh_uint32_result session_store_lookup(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * token);
struct picomesh_int_result session_store_destroy(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * token);
struct picomesh_size_result session_store_count_active(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_session_register(void);

#endif
