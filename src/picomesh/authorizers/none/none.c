/* `none` authorizer — allow every call.
 *
 * For internal frontends that sit behind a gateway which already authorizes
 * (e.g. a transport bridge), or for development. Explicit, so the choice to run
 * with no policy is a deliberate `type: none`, not the silent absence of an
 * `authorizer:` block. */

#include <picomesh/authorizers/base.h>

#include <stddef.h>
#include <stdio.h>

static struct picomesh_void_ptr_result none_create(struct picomesh_engine *engine,
                                                   const struct yconfig_node *config)
{
    (void)engine; (void)config;
    /* No state; return a non-NULL sentinel so the registry treats it as built. */
    static int sentinel;
    return PICOMESH_OK(picomesh_void_ptr, &sentinel);
}

static struct picomesh_authz_decision none_authorize(void *state, const char *endpoint,
                                                     const struct yjson_value *args, const char *jwt)
{
    (void)state; (void)endpoint; (void)args; (void)jwt;
    struct picomesh_authz_decision allow = {.allowed = 1, .status = 0};
    snprintf(allow.reason, sizeof(allow.reason), "no authorizer configured");
    return allow;
}

static void none_destroy(void *state) { (void)state; }

const struct picomesh_authorizer_ops *picomesh_authorizer_none_ops(void)
{
    static const struct picomesh_authorizer_ops ops = {
        .type_name = "none",
        .create = none_create,
        .authorize = none_authorize,
        .destroy = none_destroy,
    };
    return &ops;
}
