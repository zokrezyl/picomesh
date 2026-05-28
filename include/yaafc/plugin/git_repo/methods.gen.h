/* GENERATED — do not edit. */
#ifndef YAAFC_GIT_REPO_METHODS_GEN_H
#define YAAFC_GIT_REPO_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result git_repo_store_make(struct ctx * ctx, struct object * obj, uint32_t owner_id);
typedef struct yaafc_uint32_result (*git_repo_store_make_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result git_repo_store_delete(struct ctx * ctx, struct object * obj, uint32_t repo_id);
typedef struct yaafc_int_result (*git_repo_store_delete_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_uint32_result git_repo_store_owner_of(struct ctx * ctx, struct object * obj, uint32_t repo_id);
typedef struct yaafc_uint32_result (*git_repo_store_owner_of_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result git_repo_store_count_for_owner(struct ctx * ctx, struct object * obj, uint32_t owner_id);
typedef struct yaafc_size_result (*git_repo_store_count_for_owner_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result git_repo_store_count_total(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*git_repo_store_count_total_fn)(struct ctx *, struct object *);

#endif
