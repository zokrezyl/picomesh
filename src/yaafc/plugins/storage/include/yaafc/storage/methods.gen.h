/* GENERATED — do not edit. */
#ifndef YAAFC_STORAGE_METHODS_GEN_H
#define YAAFC_STORAGE_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int64_result;
struct yaafc_int_result;
struct yaafc_size_result;

struct yaafc_int_result storage_kv_set(struct ctx * ctx, struct object * obj, uint32_t key_id, int32_t value);
typedef struct yaafc_int_result (*storage_kv_set_fn)(struct ctx *, struct object *, uint32_t, int32_t);
struct yaafc_int_result storage_kv_get(struct ctx * ctx, struct object * obj, uint32_t key_id);
typedef struct yaafc_int_result (*storage_kv_get_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result storage_kv_count(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*storage_kv_count_fn)(struct ctx *, struct object *);
struct yaafc_int_result storage_sql_set(struct ctx * ctx, struct object * obj, const char * key, int64_t value);
typedef struct yaafc_int_result (*storage_sql_set_fn)(struct ctx *, struct object *, const char *, int64_t);
struct yaafc_int64_result storage_sql_get(struct ctx * ctx, struct object * obj, const char * key);
typedef struct yaafc_int64_result (*storage_sql_get_fn)(struct ctx *, struct object *, const char *);
struct yaafc_int_result storage_sql_exists(struct ctx * ctx, struct object * obj, const char * key);
typedef struct yaafc_int_result (*storage_sql_exists_fn)(struct ctx *, struct object *, const char *);
struct yaafc_int_result storage_sql_del(struct ctx * ctx, struct object * obj, const char * key);
typedef struct yaafc_int_result (*storage_sql_del_fn)(struct ctx *, struct object *, const char *);
struct yaafc_size_result storage_sql_count(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*storage_sql_count_fn)(struct ctx *, struct object *);

#endif
