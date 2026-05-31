/* GENERATED — do not edit. */
/* Public interface for plugin `github_authn` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/github_authn/. */
#ifndef PICOMESH_PLUGIN_GITHUB_AUTHN_H
#define PICOMESH_PLUGIN_GITHUB_AUTHN_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct picomesh_uint32_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result github_authn_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result github_authn_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_int_result github_authn_store_set_credentials(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t client_id, uint32_t secret_id);
struct picomesh_int_result github_authn_store_register_code(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t code, uint32_t user_id);
struct picomesh_uint32_result github_authn_store_resolve(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t code);
struct picomesh_size_result github_authn_store_count_codes(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_github_authn_register(void);

#endif
