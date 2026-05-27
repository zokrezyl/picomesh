/* GENERATED — do not edit. */
#ifndef YAAFC_ISSUES_METHODS_GEN_H
#define YAAFC_ISSUES_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_uint32_result issues_store_open(struct ctx * ctx, struct object * obj, uint32_t repo_id, uint32_t author_id);
typedef struct yaafc_uint32_result (*issues_store_open_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_int_result issues_store_close(struct ctx * ctx, struct object * obj, uint32_t issue_id);
typedef struct yaafc_int_result (*issues_store_close_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result issues_store_status(struct ctx * ctx, struct object * obj, uint32_t issue_id);
typedef struct yaafc_int_result (*issues_store_status_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result issues_store_count_open_in_repo(struct ctx * ctx, struct object * obj, uint32_t repo_id);
typedef struct yaafc_size_result (*issues_store_count_open_in_repo_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result issues_store_count_total(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*issues_store_count_total_fn)(struct ctx *, struct object *);

#endif
