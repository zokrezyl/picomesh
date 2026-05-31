/* GENERATED — do not edit. */
/* Public interface for plugin `issues` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/issues/. */
#ifndef PICOMESH_PLUGIN_ISSUES_H
#define PICOMESH_PLUGIN_ISSUES_H

#include <picomesh/yclass/class.h>
#include <picomesh/yclass/rpc.h>

struct picomesh_int_result;
struct picomesh_size_result;
struct picomesh_uint32_result;
struct yheaders;
struct object_ptr_result;
struct class_ptr_result;

/* ---- class accessors ---- */
struct class_ptr_result issues_store_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result issues_store_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_uint32_result issues_store_open(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, uint32_t author_id);
struct picomesh_int_result issues_store_close(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t issue_id);
struct picomesh_int_result issues_store_status(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t issue_id);
struct picomesh_size_result issues_store_count_open_in_repo(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id);
struct picomesh_size_result issues_store_count_total(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_issues_register(void);

#endif
