/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `git_pipeline`.
 * NEVER include this from outside src/picomesh/plugins/git_pipeline/. */
#ifndef PICOMESH_GIT_PIPELINE_INTERNAL_H
#define PICOMESH_GIT_PIPELINE_INTERNAL_H

#include <picomesh/plugin/git_pipeline/git_pipeline.h>

typedef struct picomesh_uint32_result (*git_pipeline_store_enqueue_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_uint32_result (*git_pipeline_store_lease_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*git_pipeline_store_complete_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int32_t);
typedef struct picomesh_size_result (*git_pipeline_store_count_pending_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_size_result (*git_pipeline_store_count_running_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_size_result (*git_pipeline_store_count_done_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
