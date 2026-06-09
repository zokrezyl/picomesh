/* GENERATED — do not edit. */
#include "git_repo.internal.h"

__attribute__((unused)) static git_repo_git_repo_make_fn
    _git_repo_git_repo_git_repo_git_repo_make_check =
        git_repo_git_repo_make_impl;
__attribute__((unused)) static git_repo_git_repo_delete_fn
    _git_repo_git_repo_git_repo_git_repo_delete_check =
        git_repo_git_repo_delete_impl;
__attribute__((unused)) static git_repo_git_repo_owner_of_fn
    _git_repo_git_repo_git_repo_git_repo_owner_of_check =
        git_repo_git_repo_owner_of_impl;
__attribute__((unused)) static git_repo_git_repo_namespace_of_fn
    _git_repo_git_repo_git_repo_git_repo_namespace_of_check =
        git_repo_git_repo_namespace_of_impl;
__attribute__((unused)) static git_repo_git_repo_count_for_owner_fn
    _git_repo_git_repo_git_repo_git_repo_count_for_owner_check =
        git_repo_git_repo_count_for_owner_impl;
__attribute__((unused)) static git_repo_git_repo_count_total_fn
    _git_repo_git_repo_git_repo_git_repo_count_total_check =
        git_repo_git_repo_count_total_impl;
__attribute__((unused)) static git_repo_git_repo_list_for_owner_fn
    _git_repo_git_repo_git_repo_git_repo_list_for_owner_check =
        git_repo_git_repo_list_for_owner_impl;
__attribute__((unused)) static git_repo_git_repo_list_for_namespace_fn
    _git_repo_git_repo_git_repo_git_repo_list_for_namespace_check =
        git_repo_git_repo_list_for_namespace_impl;
__attribute__((unused)) static git_repo_git_repo_count_for_namespace_fn
    _git_repo_git_repo_git_repo_git_repo_count_for_namespace_check =
        git_repo_git_repo_count_for_namespace_impl;
__attribute__((unused)) static git_repo_git_repo_read_tree_fn
    _git_repo_git_repo_git_repo_git_repo_read_tree_check =
        git_repo_git_repo_read_tree_impl;
__attribute__((unused)) static git_repo_git_repo_read_file_fn
    _git_repo_git_repo_git_repo_git_repo_read_file_check =
        git_repo_git_repo_read_file_impl;
__attribute__((unused)) static git_repo_git_repo_put_file_fn
    _git_repo_git_repo_git_repo_git_repo_put_file_check =
        git_repo_git_repo_put_file_impl;
__attribute__((unused)) static git_repo_git_repo_is_public_fn
    _git_repo_git_repo_git_repo_git_repo_is_public_check =
        git_repo_git_repo_is_public_impl;
__attribute__((unused)) static git_repo_git_repo_set_public_fn
    _git_repo_git_repo_git_repo_git_repo_set_public_check =
        git_repo_git_repo_set_public_impl;
__attribute__((unused)) static git_repo_git_repo_list_fn
    _git_repo_git_repo_git_repo_git_repo_list_check =
        git_repo_git_repo_list_impl;
__attribute__((unused)) static git_repo_git_repo_list_all_fn
    _git_repo_git_repo_git_repo_git_repo_list_all_check =
        git_repo_git_repo_list_all_impl;

struct class_ptr_result git_repo_git_repo_class_get(void) {
  static const struct class *cls = NULL;
  if (cls)
    return PICOMESH_OK(class_ptr, cls);
  ydebug("registering class=git_repo_git_repo");

  static const struct class_descriptor desc = {
      .name = "git_repo_git_repo",
      .type = CLASS_TYPE_REGULAR,
      .data_size = sizeof(struct git_repo_git_repo_data),
  };
  static const struct op ops[] = {
      {"git_repo", "git_repo_make", (method_id_t)git_repo_git_repo_make,
       (impl_t)git_repo_git_repo_make_impl},
      {"git_repo", "git_repo_delete", (method_id_t)git_repo_git_repo_delete,
       (impl_t)git_repo_git_repo_delete_impl},
      {"git_repo", "git_repo_owner_of", (method_id_t)git_repo_git_repo_owner_of,
       (impl_t)git_repo_git_repo_owner_of_impl},
      {"git_repo", "git_repo_namespace_of",
       (method_id_t)git_repo_git_repo_namespace_of,
       (impl_t)git_repo_git_repo_namespace_of_impl},
      {"git_repo", "git_repo_count_for_owner",
       (method_id_t)git_repo_git_repo_count_for_owner,
       (impl_t)git_repo_git_repo_count_for_owner_impl},
      {"git_repo", "git_repo_count_total",
       (method_id_t)git_repo_git_repo_count_total,
       (impl_t)git_repo_git_repo_count_total_impl},
      {"git_repo", "git_repo_list_for_owner",
       (method_id_t)git_repo_git_repo_list_for_owner,
       (impl_t)git_repo_git_repo_list_for_owner_impl},
      {"git_repo", "git_repo_list_for_namespace",
       (method_id_t)git_repo_git_repo_list_for_namespace,
       (impl_t)git_repo_git_repo_list_for_namespace_impl},
      {"git_repo", "git_repo_count_for_namespace",
       (method_id_t)git_repo_git_repo_count_for_namespace,
       (impl_t)git_repo_git_repo_count_for_namespace_impl},
      {"git_repo", "git_repo_read_tree",
       (method_id_t)git_repo_git_repo_read_tree,
       (impl_t)git_repo_git_repo_read_tree_impl},
      {"git_repo", "git_repo_read_file",
       (method_id_t)git_repo_git_repo_read_file,
       (impl_t)git_repo_git_repo_read_file_impl},
      {"git_repo", "git_repo_put_file", (method_id_t)git_repo_git_repo_put_file,
       (impl_t)git_repo_git_repo_put_file_impl},
      {"git_repo", "git_repo_is_public",
       (method_id_t)git_repo_git_repo_is_public,
       (impl_t)git_repo_git_repo_is_public_impl},
      {"git_repo", "git_repo_set_public",
       (method_id_t)git_repo_git_repo_set_public,
       (impl_t)git_repo_git_repo_set_public_impl},
      {"git_repo", "git_repo_list", (method_id_t)git_repo_git_repo_list,
       (impl_t)git_repo_git_repo_list_impl},
      {"git_repo", "git_repo_list_all", (method_id_t)git_repo_git_repo_list_all,
       (impl_t)git_repo_git_repo_list_all_impl},
  };
  struct class_ptr_result _r =
      class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]), NULL, NULL, 0);
  if (PICOMESH_IS_ERR(_r))
    return PICOMESH_ERR(
        class_ptr, "git_repo_git_repo_class_get: class_register failed", _r);
  cls = _r.value;
  return _r;
}
