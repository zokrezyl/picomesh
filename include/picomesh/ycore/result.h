#ifndef PICOMESH_YCORE_RESULT_H
#define PICOMESH_YCORE_RESULT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Error info — head of a heap-linked chain. The top-of-chain error lives by
 * value inside a Result; deeper levels (the `cause` chain) are heap-allocated.
 *
 * Location: `file`, `line`, `func` are captured at the PICOMESH_ERR call site
 * via __FILE__ / __LINE__ / __func__ — string literals from the binary's
 * read-only segment, never freed.
 *
 * Ownership: when a callee returns a Result with an error, the immediate
 * caller owns that error's `cause` chain. Two ways to discharge ownership:
 *   1) Wrap and forward: PICOMESH_ERR(type, "outer", inner_res) transfers the
 *      cause chain into the new error.
 *   2) Drop: picomesh_error_destroy(inner_res.error) frees the chain. */
struct picomesh_error {
    const char *msg;
    const char *file;
    const char *func;
    int line;
    struct picomesh_error *cause;
};

#define PICOMESH_RESULT_DECLARE(type, value_type)                                                     \
    struct type##_result {                                                                         \
        int ok;                                                                                    \
        union {                                                                                    \
            value_type value;                                                                      \
            struct picomesh_error error;                                                              \
        };                                                                                         \
    }

PICOMESH_RESULT_DECLARE(picomesh_void, int);
/* Owned opaque pointer (caller-defined ownership). Used by factory functions
 * that build a heap object and want to carry an error message on failure. */
PICOMESH_RESULT_DECLARE(picomesh_void_ptr, void *);
PICOMESH_RESULT_DECLARE(picomesh_int, int);
PICOMESH_RESULT_DECLARE(picomesh_size, size_t);
PICOMESH_RESULT_DECLARE(picomesh_int64, int64_t);
PICOMESH_RESULT_DECLARE(picomesh_uint32, uint32_t);
/* Owned, heap-allocated, NUL-terminated string (or opaque bytes with a
 * trailing NUL). The OK consumer takes ownership of `.value` and must
 * free() it. Used by the storage `get` methods, whose values are
 * strings/bytes rather than fixed-width integers. */
PICOMESH_RESULT_DECLARE(picomesh_string, char *);
/* Owned, heap-allocated, NUL-terminated string that already holds a
 * serialized JSON value (array/object/etc.). Identical to picomesh_string
 * on the binary wire (packed as a string the caller frees), but the JSON
 * frontend emits `.value` verbatim instead of quoting it — so a method
 * that returns a list returns a real JSON array, not a delimited blob. */
PICOMESH_RESULT_DECLARE(picomesh_json, char *);

struct picomesh_error *picomesh_error_chain(struct picomesh_error prev);
void picomesh_error_destroy(struct picomesh_error err);
void picomesh_error_print(FILE *out, const char *headline, struct picomesh_error err);
size_t picomesh_error_snprint(char *buf, size_t bufsize, struct picomesh_error err);

#define PICOMESH_OK_VOID() ((struct picomesh_void_result){.ok = 1, .value = 0})
#define PICOMESH_OK(type, val) ((struct type##_result){.ok = 1, .value = (val)})

#define PICOMESH_ERR(...) PICOMESH_ERR_DISPATCH(__VA_ARGS__, PICOMESH_ERR_3, PICOMESH_ERR_2)(__VA_ARGS__)
#define PICOMESH_ERR_DISPATCH(_1, _2, _3, NAME, ...) NAME

#define PICOMESH_ERR_2(type, err_msg)                                                                 \
    ((struct type##_result){.ok = 0,                                                               \
                            .error = {.msg = (err_msg),                                            \
                                      .file = __FILE__,                                            \
                                      .func = __func__,                                            \
                                      .line = __LINE__,                                            \
                                      .cause = NULL}})

#define PICOMESH_ERR_3(type, err_msg, prev_res)                                                       \
    ((struct type##_result){.ok = 0,                                                               \
                            .error = {.msg = (err_msg),                                            \
                                      .file = __FILE__,                                            \
                                      .func = __func__,                                            \
                                      .line = __LINE__,                                            \
                                      .cause = picomesh_error_chain((prev_res).error)}})

#define PICOMESH_IS_OK(res) ((res).ok)
#define PICOMESH_IS_ERR(res) (!(res).ok)

#if defined(__clang__) || defined(__GNUC__)
#define PICOMESH_EXTERNAL_CALLBACK __attribute__((annotate("picomesh_external_callback")))
#else
#define PICOMESH_EXTERNAL_CALLBACK
#endif

#define PICOMESH_RETURN_IF_ERR(type, res, msg)                                                        \
    do {                                                                                           \
        if (PICOMESH_IS_ERR(res)) {                                                                   \
            return PICOMESH_ERR(type, msg, (res));                                                    \
        }                                                                                          \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YCORE_RESULT_H */
