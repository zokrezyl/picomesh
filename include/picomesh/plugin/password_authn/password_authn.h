/* GENERATED — do not edit. */
/* Public interface for plugin `password_authn` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/password_authn/. */
#ifndef PICOMESH_PLUGIN_PASSWORD_AUTHN_H
#define PICOMESH_PLUGIN_PASSWORD_AUTHN_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result password_authn_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result password_authn_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_int_result password_authn_store_register(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash);
struct picomesh_int_result password_authn_store_authenticate(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash);
struct picomesh_int_result password_authn_store_change_password(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash);
struct picomesh_size_result password_authn_store_count_registered(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_password_authn_register(void);

#endif
