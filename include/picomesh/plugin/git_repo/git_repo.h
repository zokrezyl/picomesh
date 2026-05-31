/* GENERATED — do not edit. */
/* Public interface for plugin `git_repo` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/git_repo/. */
#ifndef PICOMESH_PLUGIN_GIT_REPO_H
#define PICOMESH_PLUGIN_GIT_REPO_H

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
struct class_ptr_result git_repo_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result git_repo_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_uint32_result git_repo_store_make(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id, const char * owner_name, const char * repo_name);
struct picomesh_int_result git_repo_store_delete(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id);
struct picomesh_uint32_result git_repo_store_owner_of(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id);
struct picomesh_size_result git_repo_store_count_for_owner(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id);
struct picomesh_size_result git_repo_store_count_total(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_string_result git_repo_store_list_for_owner(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id);
struct picomesh_string_result git_repo_store_read_tree(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * ref, const char * path);
struct picomesh_string_result git_repo_store_read_file(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * ref, const char * path);
struct picomesh_string_result git_repo_store_put_file(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * path, const char * content, const char * message, const char * author_name, const char * author_email);
struct picomesh_int_result git_repo_store_is_public(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id);
struct picomesh_int_result git_repo_store_set_public(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, int is_public);

/* ---- activation ---- */
void picomesh_plugin_git_repo_register(void);

#endif
