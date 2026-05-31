/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `password_authn`.
 * NEVER include this from outside src/picomesh/plugins/password_authn/. */
#ifndef PICOMESH_PASSWORD_AUTHN_INTERNAL_H
#define PICOMESH_PASSWORD_AUTHN_INTERNAL_H

#include <picomesh/plugin/password_authn/password_authn.h>

typedef struct picomesh_int_result (*password_authn_store_register_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int64_t);
typedef struct picomesh_int_result (*password_authn_store_authenticate_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int64_t);
typedef struct picomesh_int_result (*password_authn_store_change_password_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int64_t);
typedef struct picomesh_size_result (*password_authn_store_count_registered_fn)(struct ctx *, struct object *, struct yheaders *);

#endif
