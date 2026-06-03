/* GENERATED — do not edit. */
/* Public interface for plugin `git_pipeline` — GENERATED.
 * Edit the annotated sources under src/picomesh/plugins/git_pipeline/. */
#ifndef PICOMESH_PLUGIN_GIT_PIPELINE_H
#define PICOMESH_PLUGIN_GIT_PIPELINE_H

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
struct class_ptr_result git_pipeline_git_pipeline_class_get(void);

/* ---- constructors ---- */
struct object_ptr_result git_pipeline_git_pipeline_create(struct ctx *ctx);

/* ---- methods ---- */
struct picomesh_uint32_result git_pipeline_git_pipeline_enqueue(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id);
struct picomesh_uint32_result git_pipeline_git_pipeline_lease(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t runner_id);
struct picomesh_int_result git_pipeline_git_pipeline_complete(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t job_id, int32_t status);
struct picomesh_size_result git_pipeline_git_pipeline_count_pending(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_size_result git_pipeline_git_pipeline_count_running(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_size_result git_pipeline_git_pipeline_count_done(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);
struct picomesh_json_result git_pipeline_git_pipeline_list(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int64_t offset, int64_t limit);
struct picomesh_json_result git_pipeline_git_pipeline_list_all(struct ctx * ctx, struct object * obj, struct yheaders * hdrs);

/* ---- activation ---- */
void picomesh_plugin_git_pipeline_register(void);

#endif
