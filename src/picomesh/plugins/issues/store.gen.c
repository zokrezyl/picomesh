/* GENERATED — do not edit. */
#include "issues.internal.h"

__attribute__((unused)) static issues_issues_open_fn
    _issues_issues_issues_issues_open_check = issues_issues_open_impl;
__attribute__((unused)) static issues_issues_close_fn
    _issues_issues_issues_issues_close_check = issues_issues_close_impl;
__attribute__((unused)) static issues_issues_status_fn
    _issues_issues_issues_issues_status_check = issues_issues_status_impl;
__attribute__((unused)) static issues_issues_count_open_in_repo_fn
    _issues_issues_issues_issues_count_open_in_repo_check =
        issues_issues_count_open_in_repo_impl;
__attribute__((unused)) static issues_issues_count_total_fn
    _issues_issues_issues_issues_count_total_check =
        issues_issues_count_total_impl;
__attribute__((unused)) static issues_issues_list_fn
    _issues_issues_issues_issues_list_check = issues_issues_list_impl;
__attribute__((unused)) static issues_issues_list_all_fn
    _issues_issues_issues_issues_list_all_check = issues_issues_list_all_impl;
__attribute__((unused)) static issues_issues_repo_of_fn
    _issues_issues_issues_issues_repo_of_check = issues_issues_repo_of_impl;

struct class_ptr_result issues_issues_class_get(void) {
  static const struct class *cls = NULL;
  if (cls)
    return PICOMESH_OK(class_ptr, cls);
  ydebug("registering class=issues_issues");

  static const struct class_descriptor desc = {
      .name = "issues_issues",
      .type = CLASS_TYPE_REGULAR,
      .data_size = sizeof(struct issues_issues_data),
  };
  static const struct op ops[] = {
      {"issues", "issues_open", (method_id_t)issues_issues_open,
       (impl_t)issues_issues_open_impl},
      {"issues", "issues_close", (method_id_t)issues_issues_close,
       (impl_t)issues_issues_close_impl},
      {"issues", "issues_status", (method_id_t)issues_issues_status,
       (impl_t)issues_issues_status_impl},
      {"issues", "issues_count_open_in_repo",
       (method_id_t)issues_issues_count_open_in_repo,
       (impl_t)issues_issues_count_open_in_repo_impl},
      {"issues", "issues_count_total", (method_id_t)issues_issues_count_total,
       (impl_t)issues_issues_count_total_impl},
      {"issues", "issues_list", (method_id_t)issues_issues_list,
       (impl_t)issues_issues_list_impl},
      {"issues", "issues_list_all", (method_id_t)issues_issues_list_all,
       (impl_t)issues_issues_list_all_impl},
      {"issues", "issues_repo_of", (method_id_t)issues_issues_repo_of,
       (impl_t)issues_issues_repo_of_impl},
  };
  struct class_ptr_result _r =
      class_register(&desc, ops, sizeof(ops) / sizeof(ops[0]), NULL, NULL, 0);
  if (PICOMESH_IS_ERR(_r))
    return PICOMESH_ERR(class_ptr,
                        "issues_issues_class_get: class_register failed", _r);
  cls = _r.value;
  return _r;
}
