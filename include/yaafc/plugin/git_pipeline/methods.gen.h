/* GENERATED — do not edit. */
#ifndef YAAFC_GIT_PIPELINE_METHODS_GEN_H
#define YAAFC_GIT_PIPELINE_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result git_pipeline_store_enqueue(struct ctx * ctx, struct object * obj, uint32_t repo_id);
typedef struct yaafc_uint32_result (*git_pipeline_store_enqueue_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_uint32_result git_pipeline_store_lease(struct ctx * ctx, struct object * obj, uint32_t runner_id);
typedef struct yaafc_uint32_result (*git_pipeline_store_lease_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result git_pipeline_store_complete(struct ctx * ctx, struct object * obj, uint32_t job_id, int32_t status);
typedef struct yaafc_int_result (*git_pipeline_store_complete_fn)(struct ctx *, struct object *, uint32_t, int32_t);
struct yaafc_size_result git_pipeline_store_count_pending(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*git_pipeline_store_count_pending_fn)(struct ctx *, struct object *);
struct yaafc_size_result git_pipeline_store_count_running(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*git_pipeline_store_count_running_fn)(struct ctx *, struct object *);
struct yaafc_size_result git_pipeline_store_count_done(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*git_pipeline_store_count_done_fn)(struct ctx *, struct object *);

#endif
