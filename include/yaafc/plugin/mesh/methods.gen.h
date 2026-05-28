/* GENERATED — do not edit. */
#ifndef YAAFC_MESH_METHODS_GEN_H
#define YAAFC_MESH_METHODS_GEN_H

#include <yaafc/yclass/class.h>

struct yaafc_int_result;
struct yaafc_size_result;
struct yaafc_uint32_result;

struct yaafc_int_result mesh_store_register_service(struct ctx * ctx, struct object * obj, uint32_t service_id, uint32_t port);
typedef struct yaafc_int_result (*mesh_store_register_service_fn)(struct ctx *, struct object *, uint32_t, uint32_t);
struct yaafc_uint32_result mesh_store_resolve(struct ctx * ctx, struct object * obj, uint32_t service_id);
typedef struct yaafc_uint32_result (*mesh_store_resolve_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result mesh_store_forget(struct ctx * ctx, struct object * obj, uint32_t service_id);
typedef struct yaafc_int_result (*mesh_store_forget_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_size_result mesh_store_count_services(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*mesh_store_count_services_fn)(struct ctx *, struct object *);
struct yaafc_int_result mesh_store_spawn_yaafc(struct ctx * ctx, struct object * obj, uint32_t port);
typedef struct yaafc_int_result (*mesh_store_spawn_yaafc_fn)(struct ctx *, struct object *, uint32_t);
struct yaafc_int_result mesh_store_kill_pid(struct ctx * ctx, struct object * obj, int32_t pid);
typedef struct yaafc_int_result (*mesh_store_kill_pid_fn)(struct ctx *, struct object *, int32_t);
struct yaafc_size_result mesh_store_count_children(struct ctx * ctx, struct object * obj);
typedef struct yaafc_size_result (*mesh_store_count_children_fn)(struct ctx *, struct object *);
struct yaafc_int_result mesh_store_reconcile_from_config(struct ctx * ctx, struct object * obj);
typedef struct yaafc_int_result (*mesh_store_reconcile_from_config_fn)(struct ctx *, struct object *);
struct yaafc_int_result mesh_store_reconcile(struct ctx * ctx, struct object * obj);
typedef struct yaafc_int_result (*mesh_store_reconcile_fn)(struct ctx *, struct object *);

#endif
