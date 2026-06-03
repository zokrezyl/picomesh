/* GENERATED — do not edit. */
#include "github_authn.internal.h"

__attribute__((unused))
static github_authn_github_authn_set_credentials_fn _github_authn_github_authn_github_authn_github_authn_set_credentials_check = github_authn_github_authn_set_credentials_impl;
__attribute__((unused))
static github_authn_github_authn_register_code_fn _github_authn_github_authn_github_authn_github_authn_register_code_check = github_authn_github_authn_register_code_impl;
__attribute__((unused))
static github_authn_github_authn_resolve_fn _github_authn_github_authn_github_authn_github_authn_resolve_check = github_authn_github_authn_resolve_impl;
__attribute__((unused))
static github_authn_github_authn_count_codes_fn _github_authn_github_authn_github_authn_github_authn_count_codes_check = github_authn_github_authn_count_codes_impl;
__attribute__((unused))
static github_authn_github_authn_list_fn _github_authn_github_authn_github_authn_github_authn_list_check = github_authn_github_authn_list_impl;
__attribute__((unused))
static github_authn_github_authn_list_all_fn _github_authn_github_authn_github_authn_github_authn_list_all_check = github_authn_github_authn_list_all_impl;

struct class_ptr_result github_authn_github_authn_class_get(void)
{
    static const struct class *cls = NULL;
    if (cls) return PICOMESH_OK(class_ptr, cls);
    ydebug("registering class=github_authn_github_authn");

    static const struct class_descriptor desc = {
        .name = "github_authn_github_authn",
        .type = CLASS_TYPE_REGULAR,
        .data_size = sizeof(struct github_authn_github_authn_data),
    };
    static const struct op ops[] = {
        {"github_authn", "github_authn_set_credentials", (method_id_t)github_authn_github_authn_set_credentials, (impl_t)github_authn_github_authn_set_credentials_impl},
        {"github_authn", "github_authn_register_code", (method_id_t)github_authn_github_authn_register_code, (impl_t)github_authn_github_authn_register_code_impl},
        {"github_authn", "github_authn_resolve", (method_id_t)github_authn_github_authn_resolve, (impl_t)github_authn_github_authn_resolve_impl},
        {"github_authn", "github_authn_count_codes", (method_id_t)github_authn_github_authn_count_codes, (impl_t)github_authn_github_authn_count_codes_impl},
        {"github_authn", "github_authn_list", (method_id_t)github_authn_github_authn_list, (impl_t)github_authn_github_authn_list_impl},
        {"github_authn", "github_authn_list_all", (method_id_t)github_authn_github_authn_list_all, (impl_t)github_authn_github_authn_list_all_impl},
    };
    struct class_ptr_result _r =
        class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]),
                       NULL, NULL, 0);
    if (PICOMESH_IS_ERR(_r))
        return PICOMESH_ERR(class_ptr, "github_authn_github_authn_class_get: class_register failed", _r);
    cls = _r.value;
    return _r;
}
