/* Internal: the built-in authorizer types' ops getters. */

#ifndef PICOMESH_AUTHORIZERS_TYPES_H
#define PICOMESH_AUTHORIZERS_TYPES_H

#include <picomesh/authorizers/base.h>

const struct picomesh_authorizer_ops *picomesh_authorizer_policy_ops(void);
const struct picomesh_authorizer_ops *picomesh_authorizer_none_ops(void);

#endif /* PICOMESH_AUTHORIZERS_TYPES_H */
