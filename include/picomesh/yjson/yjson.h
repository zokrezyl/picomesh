/* yjson — thin C ABI over simdjson + a small JSON writer.
 *
 * Parsing goes through simdjson; we expose a borrowed-handle API so
 * C callers don't see any C++ details. Building JSON for outbound
 * responses goes through a hand-rolled writer (simdjson is parse-
 * only) — sufficient for the limited shapes yttp / cli emit.
 *
 * Lifetime:
 *
 *   struct yjson_doc *doc = yjson_parse(buf, len);
 *   const struct yjson_value *root = yjson_doc_root(doc);
 *   ... navigate / read ...
 *   yjson_doc_free(doc);
 *
 * Every borrowed `yjson_value *` is valid until `yjson_doc_free`. */

#ifndef PICOMESH_YJSON_YJSON_H
#define PICOMESH_YJSON_YJSON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct yjson_doc;
struct yjson_value;

enum yjson_kind {
    YJSON_NULL,
    YJSON_BOOL,
    YJSON_INT,
    YJSON_FLOAT,
    YJSON_STRING,
    YJSON_ARRAY,
    YJSON_OBJECT,
};

/* --- parse ----------------------------------------------------------- */

/* Parse `data[0..len)`. Returns NULL on parse error (caller can check
 * yjson_last_error if non-NULL needed). The doc owns every value
 * borrowed below. */
struct yjson_doc *yjson_parse(const char *data, size_t len);
void yjson_doc_free(struct yjson_doc *doc);
const struct yjson_value *yjson_doc_root(const struct yjson_doc *doc);

/* Last per-thread parse error message, or NULL. Heap-owned by the
 * library; do not free. */
const char *yjson_last_error(void);

/* --- type queries ---------------------------------------------------- */

enum yjson_kind yjson_kind(const struct yjson_value *v);
int yjson_is_null  (const struct yjson_value *v);
int yjson_is_bool  (const struct yjson_value *v);
int yjson_is_int   (const struct yjson_value *v);
int yjson_is_float (const struct yjson_value *v);
int yjson_is_string(const struct yjson_value *v);
int yjson_is_array (const struct yjson_value *v);
int yjson_is_object(const struct yjson_value *v);

/* --- scalar getters -------------------------------------------------- */

int     yjson_as_bool  (const struct yjson_value *v, int fallback);
int64_t yjson_as_int   (const struct yjson_value *v, int64_t fallback);
double  yjson_as_float (const struct yjson_value *v, double fallback);
/* Returned pointer is valid until yjson_doc_free. */
const char *yjson_as_string(const struct yjson_value *v, const char *fallback);

/* --- container access ----------------------------------------------- */

size_t yjson_array_size(const struct yjson_value *v);
const struct yjson_value *yjson_array_at(const struct yjson_value *v, size_t idx);

const struct yjson_value *yjson_object_get(const struct yjson_value *v, const char *key);

/* --- writer (hand-rolled; no simdjson involved) --------------------- */

struct yjson_writer;

struct yjson_writer *yjson_writer_new(void);
void yjson_writer_free(struct yjson_writer *w);

void yjson_writer_begin_object(struct yjson_writer *w);
void yjson_writer_end_object  (struct yjson_writer *w);
void yjson_writer_begin_array (struct yjson_writer *w);
void yjson_writer_end_array   (struct yjson_writer *w);

void yjson_writer_key   (struct yjson_writer *w, const char *key);
void yjson_writer_null  (struct yjson_writer *w);
void yjson_writer_bool  (struct yjson_writer *w, int v);
void yjson_writer_int   (struct yjson_writer *w, int64_t v);
void yjson_writer_float (struct yjson_writer *w, double v);
void yjson_writer_string(struct yjson_writer *w, const char *s);

/* Emit an already-serialized JSON fragment as a value, verbatim (no
 * quoting, no escaping). The caller guarantees `json` is well-formed JSON;
 * a NULL or empty fragment is written as `null`. Used to splice a value a
 * method already produced as JSON text (e.g. a list) into the response. */
void yjson_writer_raw   (struct yjson_writer *w, const char *json);

/* Borrow the assembled buffer (no copy). NUL-terminated. Valid until
 * the writer is freed or another write happens. `*len_out` (if non-
 * NULL) receives the length excluding NUL. */
const char *yjson_writer_data(struct yjson_writer *w, size_t *len_out);

#ifdef __cplusplus
}
#endif

#endif /* PICOMESH_YJSON_YJSON_H */
