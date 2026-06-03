/* Authorizer registry — build the one authorizer from config, run it.
 *
 * The frontend reads `authorizer:` (a single typed dict) and asks this module
 * to materialise it; `type:` selects a registered authorizer (`policy`,
 * `none`, …). Adding a new authorizer type = one new module + one line. */

#ifndef PICOMESH_AUTHORIZERS_REGISTRY_H
#define PICOMESH_AUTHORIZERS_REGISTRY_H

#include <picomesh/authorizers/base.h>
#include <picomesh/ycore/result.h>

#ifdef __cplusplus
extern "C" {
#endif

struct picomesh_authorizer;

/* Build the authorizer from the `authorizer:` config node (a map with a
 * `type:` key). Fails loud on a missing/unknown type — security config is
 * never silently dropped. */
struct picomesh_void_ptr_result
picomesh_authorizer_build(struct picomesh_engine *engine, const struct yconfig_node *config);

/* Decide allow/deny for one call. */
struct picomesh_authz_decision
picomesh_authorizer_decide(struct picomesh_authorizer *authorizer, const char *endpoint,
                           const struct yjson_value *args, const char *jwt);

void picomesh_authorizer_free(struct picomesh_authorizer *authorizer);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_AUTHORIZERS_REGISTRY_H */
