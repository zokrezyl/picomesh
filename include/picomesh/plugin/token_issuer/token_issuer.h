/* GENERATED — do not edit. */
/* Public interface for plugin `token_issuer` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/token_issuer/. */
#ifndef PICOMESH_PLUGIN_TOKEN_ISSUER_H
#define PICOMESH_PLUGIN_TOKEN_ISSUER_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_json_result;
struct picomesh_size_result;
struct picomesh_uint32_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result token_issuer_token_issuer_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result token_issuer_token_issuer_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_uint32_result token_issuer_token_issuer_login(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, uint32_t provider_id);
struct picomesh_uint32_result token_issuer_token_issuer_validate(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t token_id);
struct picomesh_uint32_result token_issuer_token_issuer_refresh(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t token_id);
struct picomesh_int_result token_issuer_token_issuer_revoke(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t token_id);
struct picomesh_size_result token_issuer_token_issuer_count_active(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_json_result token_issuer_token_issuer_list(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t offset, int64_t limit);
struct picomesh_json_result token_issuer_token_issuer_list_all(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_token_issuer_register(void);

#endif
