/* Internal: the built-in authenticator types' ops getters. The registry lists
 * these; adding a type means adding its module + one line here and in the
 * registry table. Each getter returns a pointer to a function-local static
 * const ops table (no file-scope data symbol). */

#ifndef PICOMESH_AUTHENTICATORS_TYPES_H
#define PICOMESH_AUTHENTICATORS_TYPES_H

#include <picomesh/authenticators/base.h>

const struct picomesh_authenticator_ops *
picomesh_authenticator_session_cookie_ops(void);
const struct picomesh_authenticator_ops *
picomesh_authenticator_bearer_jwt_token_ops(void);
const struct picomesh_authenticator_ops *
picomesh_authenticator_bearer_opaque_token_ops(void);

#endif /* PICOMESH_AUTHENTICATORS_TYPES_H */
