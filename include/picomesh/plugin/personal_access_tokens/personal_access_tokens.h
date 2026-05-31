/* GENERATED — do not edit. */
/* Public interface for plugin `personal_access_tokens` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/personal_access_tokens/. */
#ifndef PICOMESH_PLUGIN_PERSONAL_ACCESS_TOKENS_H
#define PICOMESH_PLUGIN_PERSONAL_ACCESS_TOKENS_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct picomesh_uint32_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result personal_access_tokens_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result personal_access_tokens_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_uint32_result personal_access_tokens_store_mint(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id);
struct picomesh_uint32_result personal_access_tokens_store_lookup(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t pat_id);
struct picomesh_int_result personal_access_tokens_store_revoke(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t pat_id);
struct picomesh_size_result personal_access_tokens_store_list_for_user(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id);
struct picomesh_size_result personal_access_tokens_store_count_active(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_personal_access_tokens_register(void);

#endif
