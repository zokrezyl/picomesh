/* GENERATED — do not edit. */
/* Internal codegen-only header for plugin `git_repo`.
 * NEVER include this from outside src/picomesh/plugins/git_repo/. */
#ifndef PICOMESH_GIT_REPO_INTERNAL_H
#define PICOMESH_GIT_REPO_INTERNAL_H

#include <picomesh/plugin/git_repo/git_repo.h>

typedef struct picomesh_uint32_result (*git_repo_store_make_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, const char *, const char *);
typedef struct picomesh_int_result (*git_repo_store_delete_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_uint32_result (*git_repo_store_owner_of_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*git_repo_store_count_for_owner_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_size_result (*git_repo_store_count_total_fn)(struct ctx *, struct object *, struct yheaders *);
typedef struct picomesh_string_result (*git_repo_store_list_for_owner_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_string_result (*git_repo_store_read_tree_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, const char *, const char *);
typedef struct picomesh_string_result (*git_repo_store_read_file_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, const char *, const char *);
typedef struct picomesh_string_result (*git_repo_store_put_file_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, const char *, const char *, const char *, const char *, const char *);
typedef struct picomesh_int_result (*git_repo_store_is_public_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t);
typedef struct picomesh_int_result (*git_repo_store_set_public_fn)(struct ctx *, struct object *, struct yheaders *, uint32_t, int);

#endif
