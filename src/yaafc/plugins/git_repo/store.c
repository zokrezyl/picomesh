/* git_repo — repository metadata.
 *
 *   create(owner_id)             → repo_id
 *   delete(repo_id)              → 1 / 0
 *   owner_of(repo_id)            → owner_uid (0 if missing)
 *   count_for_owner(owner_id)    → number of repos owned
 *   count_total                  → grand total
 *
 * Storage of the actual git tree (objects, packs, refs) is out of
 * scope — those go on disk under repos_dir which the scenario config
 * supplies. Once we have a git protocol speaker we can wire that
 * through `remotes: → storage`. */

#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/class.h>

#include <stdint.h>

#define REPOS_MAX 256

struct repo_entry {
    uint32_t repo_id;
    uint32_t owner_id;
    int used;
};

struct [[clang::annotate("class@git_repo:store")]] git_repo_store_data {
    struct repo_entry entries[REPOS_MAX];
    size_t count;
    uint32_t next_id;
};

static struct git_repo_store_data *gr(struct object *obj)
{
    return (struct git_repo_store_data *)((char *)obj + sizeof(struct object));
}

[[clang::annotate("override@git_repo:store:store_make")]]
struct yaafc_uint32_result git_repo_store_make_impl(struct ctx *ctx, struct object *obj,
                                                      uint32_t owner_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    if (d->next_id == 0) d->next_id = 1;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (!d->entries[i].used) {
            d->entries[i].repo_id = d->next_id++;
            d->entries[i].owner_id = owner_id;
            d->entries[i].used = 1;
            d->count++;
            yinfo("git_repo: created repo=%u owner=%u", d->entries[i].repo_id, owner_id);
            return YAAFC_OK(yaafc_uint32, d->entries[i].repo_id);
        }
    }
    return YAAFC_ERR(yaafc_uint32, "git_repo_create: table full");
}

[[clang::annotate("override@git_repo:store:store_delete")]]
struct yaafc_int_result git_repo_store_delete_impl(struct ctx *ctx, struct object *obj,
                                                   uint32_t repo_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].repo_id == repo_id) {
            d->entries[i].used = 0;
            d->count--;
            return YAAFC_OK(yaafc_int, 1);
        }
    }
    return YAAFC_OK(yaafc_int, 0);
}

[[clang::annotate("override@git_repo:store:store_owner_of")]]
struct yaafc_uint32_result git_repo_store_owner_of_impl(struct ctx *ctx, struct object *obj,
                                                        uint32_t repo_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].repo_id == repo_id) {
            return YAAFC_OK(yaafc_uint32, d->entries[i].owner_id);
        }
    }
    return YAAFC_OK(yaafc_uint32, 0);
}

[[clang::annotate("override@git_repo:store:store_count_for_owner")]]
struct yaafc_size_result git_repo_store_count_for_owner_impl(struct ctx *ctx, struct object *obj,
                                                             uint32_t owner_id)
{
    (void)ctx;
    struct git_repo_store_data *d = gr(obj);
    size_t n = 0;
    for (size_t i = 0; i < REPOS_MAX; ++i) {
        if (d->entries[i].used && d->entries[i].owner_id == owner_id) n++;
    }
    return YAAFC_OK(yaafc_size, n);
}

[[clang::annotate("override@git_repo:store:store_count_total")]]
struct yaafc_size_result git_repo_store_count_total_impl(struct ctx *ctx, struct object *obj)
{
    (void)ctx;
    return YAAFC_OK(yaafc_size, gr(obj)->count);
}

#include "store.gen.c"
