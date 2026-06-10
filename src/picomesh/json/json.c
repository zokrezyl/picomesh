/* json — C ABI over yyjson (parser) + a small hand-rolled writer.
 *
 * Pure C: yyjson is a single-file C library, so the whole JSON layer
 * carries no C++ dependency (it replaced simdjson, which forced a C++17
 * toolchain on the tree for this one translation unit).
 *
 * yyjson hands out borrowed `yyjson_val *` pointers that stay stable for
 * the lifetime of the parsed `yyjson_doc`. That maps directly onto our
 * opaque `struct json_value *` / `struct json_doc *` handles — we just
 * cast between the two, with no wrapper objects or arena. Every borrowed
 * value (root, array element, object member, and the strings they expose)
 * is valid until json_doc_free.
 *
 * The writer is a tiny manual JSON serializer — our outbound shapes are
 * simple enough that hand-rolling beats pulling in a builder API. */

#include <picomesh/json/json.h>

#include <yyjson.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* json_doc / json_value are opaque to callers; under the hood they ARE the
 * yyjson handles, reached through these casts. We never dereference the
 * picomesh-side types — always cast back to the yyjson type first. */
static inline yyjson_doc *as_doc(const struct json_doc *doc) {
  return (yyjson_doc *)(void *)doc;
}
static inline yyjson_val *as_val(const struct json_value *value) {
  return (yyjson_val *)(void *)value;
}

/* Per-thread last-error string. yyjson error messages are constant string
 * literals, so we can hold the pointer directly — nothing to copy or free. */
static _Thread_local const char *json_thread_last_error;

/* ----------------------------------------------------------------- */
/* Parse                                                              */
/* ----------------------------------------------------------------- */

struct json_doc *json_parse(const char *data, size_t len) {
  json_thread_last_error = NULL;
  yyjson_read_err err = {0};
  /* No YYJSON_READ_INSITU: yyjson copies `data` into the doc, so the
   * caller's buffer need not outlive this call (and `data` is not
   * modified, hence the safe const cast). */
  yyjson_doc *doc =
      yyjson_read_opts((char *)data, len, 0 /* flags */, NULL /* alc */, &err);
  if (!doc) {
    json_thread_last_error = err.msg ? err.msg : "json parse failed";
    return NULL;
  }
  return (struct json_doc *)doc;
}

void json_doc_free(struct json_doc *doc) {
  if (doc)
    yyjson_doc_free(as_doc(doc));
}

const struct json_value *json_doc_root(const struct json_doc *doc) {
  if (!doc)
    return NULL;
  return (const struct json_value *)yyjson_doc_get_root(as_doc(doc));
}

const char *json_last_error(void) { return json_thread_last_error; }

/* ----------------------------------------------------------------- */
/* Type queries / scalar getters                                      */
/* ----------------------------------------------------------------- */

enum json_kind json_kind(const struct json_value *v) {
  yyjson_val *val = as_val(v);
  if (!val)
    return JSON_NULL;
  if (yyjson_is_arr(val))
    return JSON_ARRAY;
  if (yyjson_is_obj(val))
    return JSON_OBJECT;
  if (yyjson_is_str(val))
    return JSON_STRING;
  if (yyjson_is_bool(val))
    return JSON_BOOL;
  if (yyjson_is_real(val))
    return JSON_FLOAT;
  if (yyjson_is_int(val)) /* signed or unsigned integer */
    return JSON_INT;
  return JSON_NULL;
}

int json_is_null(const struct json_value *v) {
  return json_kind(v) == JSON_NULL;
}
int json_is_bool(const struct json_value *v) {
  return json_kind(v) == JSON_BOOL;
}
int json_is_int(const struct json_value *v) { return json_kind(v) == JSON_INT; }
int json_is_float(const struct json_value *v) {
  return json_kind(v) == JSON_FLOAT;
}
int json_is_string(const struct json_value *v) {
  return json_kind(v) == JSON_STRING;
}
int json_is_array(const struct json_value *v) {
  return json_kind(v) == JSON_ARRAY;
}
int json_is_object(const struct json_value *v) {
  return json_kind(v) == JSON_OBJECT;
}

int json_as_bool(const struct json_value *v, int fallback) {
  yyjson_val *val = as_val(v);
  if (!val)
    return fallback;
  if (yyjson_is_bool(val))
    return yyjson_get_bool(val) ? 1 : 0;
  if (yyjson_is_sint(val))
    return yyjson_get_sint(val) != 0;
  if (yyjson_is_uint(val))
    return yyjson_get_uint(val) != 0;
  return fallback;
}

int64_t json_as_int(const struct json_value *v, int64_t fallback) {
  yyjson_val *val = as_val(v);
  if (!val)
    return fallback;
  if (yyjson_is_sint(val))
    return yyjson_get_sint(val);
  if (yyjson_is_uint(val))
    return (int64_t)yyjson_get_uint(val);
  if (yyjson_is_real(val))
    return (int64_t)yyjson_get_real(val);
  if (yyjson_is_bool(val))
    return yyjson_get_bool(val) ? 1 : 0;
  return fallback;
}

double json_as_float(const struct json_value *v, double fallback) {
  yyjson_val *val = as_val(v);
  if (!val)
    return fallback;
  /* yyjson_get_num yields a double for int / uint / real alike. */
  if (yyjson_is_num(val))
    return yyjson_get_num(val);
  return fallback;
}

const char *json_as_string(const struct json_value *v, const char *fallback) {
  yyjson_val *val = as_val(v);
  if (!val)
    return fallback;
  /* yyjson strings are NUL-terminated and owned by the doc — valid until
   * json_doc_free, so no copy/cache is needed. */
  const char *s = yyjson_get_str(val);
  return s ? s : fallback;
}

/* ----------------------------------------------------------------- */
/* Container access                                                   */
/* ----------------------------------------------------------------- */

size_t json_array_size(const struct json_value *v) {
  yyjson_val *val = as_val(v);
  if (!val)
    return 0;
  return yyjson_arr_size(val); /* 0 if not an array */
}

const struct json_value *json_array_at(const struct json_value *v, size_t idx) {
  yyjson_val *val = as_val(v);
  if (!val)
    return NULL;
  /* NULL if `val` is not an array or `idx` is out of range. */
  return (const struct json_value *)yyjson_arr_get(val, idx);
}

const struct json_value *json_object_get(const struct json_value *v,
                                         const char *key) {
  yyjson_val *val = as_val(v);
  if (!val || !key)
    return NULL;
  /* NULL if `val` is not an object or `key` is absent. */
  return (const struct json_value *)yyjson_obj_get(val, key);
}

/* ----------------------------------------------------------------- */
/* Writer                                                              */
/* ----------------------------------------------------------------- */
/*
 * Tracks containers on a small stack so we can emit commas correctly.
 * Each frame remembers whether something has been written inside the
 * current container (controls leading-comma insertion).
 *
 * Container kinds:
 *   'o' = object, expect alternating key / value
 *   'a' = array, expect values
 *   ' ' = top level (one value only)
 *
 * The output buffer and the frame stack grow independently; on an
 * allocation failure the writer goes into a sticky `oom` state and all
 * further writes become no-ops, so json_writer_data still returns the
 * (truncated, NUL-terminated) prefix written so far.
 */

struct jw_frame {
  char kind;
  int has_item;
  int expects_value;
};

struct json_writer {
  char *out;
  size_t len;
  size_t cap;
  struct jw_frame *stack;
  size_t depth;
  size_t stack_cap;
  int oom;
};

/* Ensure room for `extra` more bytes plus the terminating NUL. */
static int jw_reserve(struct json_writer *w, size_t extra) {
  if (w->oom)
    return 0;
  size_t need = w->len + extra + 1;
  if (need <= w->cap)
    return 1;
  size_t cap = w->cap ? w->cap : 64;
  while (cap < need)
    cap *= 2;
  char *grown = realloc(w->out, cap);
  if (!grown) {
    w->oom = 1;
    return 0;
  }
  w->out = grown;
  w->cap = cap;
  return 1;
}

static void jw_putc(struct json_writer *w, char c) {
  if (!jw_reserve(w, 1))
    return;
  w->out[w->len++] = c;
  w->out[w->len] = '\0';
}

static void jw_append(struct json_writer *w, const char *s, size_t n) {
  if (!jw_reserve(w, n))
    return;
  memcpy(w->out + w->len, s, n);
  w->len += n;
  w->out[w->len] = '\0';
}

static void jw_appends(struct json_writer *w, const char *s) {
  jw_append(w, s, strlen(s));
}

static struct jw_frame *jw_top(struct json_writer *w) {
  return w->depth ? &w->stack[w->depth - 1] : NULL;
}

static void jw_push(struct json_writer *w, char kind, int has_item,
                    int expects_value) {
  if (w->oom)
    return;
  if (w->depth == w->stack_cap) {
    size_t cap = w->stack_cap ? w->stack_cap * 2 : 8;
    struct jw_frame *grown = realloc(w->stack, cap * sizeof(*grown));
    if (!grown) {
      w->oom = 1;
      return;
    }
    w->stack = grown;
    w->stack_cap = cap;
  }
  w->stack[w->depth].kind = kind;
  w->stack[w->depth].has_item = has_item;
  w->stack[w->depth].expects_value = expects_value;
  w->depth++;
}

static void jw_pop(struct json_writer *w) {
  if (w->depth)
    w->depth--;
}

/* Note: jw_putc / jw_append grow only the output buffer, never the frame
 * stack, so a `struct jw_frame *` from jw_top stays valid across them. */
static void w_open(struct json_writer *w) {
  struct jw_frame *top = jw_top(w);
  if (!top) {
    jw_push(w, ' ', 0, 1);
    return;
  }
  if (top->kind == 'a') {
    if (top->has_item)
      jw_putc(w, ',');
    top->has_item = 1;
  } else if (top->kind == 'o') {
    if (!top->expects_value) {
      /* At a key position — caller should have called _key first. Treat
       * as an error: write an empty key. */
      jw_appends(w, "\"\":");
      top->expects_value = 1;
    } else {
      top->expects_value = 0;
      top->has_item = 1;
    }
  }
}

static void w_close_value(struct json_writer *w) {
  struct jw_frame *top = jw_top(w);
  if (!top)
    return;
  if (top->kind == 'o') /* just wrote a value; next is a key again */
    top->expects_value = 0;
}

static void escape_string(struct json_writer *w, const char *s) {
  jw_putc(w, '"');
  if (!s) {
    jw_putc(w, '"');
    return;
  }
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    unsigned char c = *p;
    switch (c) {
    case '"':
      jw_appends(w, "\\\"");
      break;
    case '\\':
      jw_appends(w, "\\\\");
      break;
    case '\b':
      jw_appends(w, "\\b");
      break;
    case '\f':
      jw_appends(w, "\\f");
      break;
    case '\n':
      jw_appends(w, "\\n");
      break;
    case '\r':
      jw_appends(w, "\\r");
      break;
    case '\t':
      jw_appends(w, "\\t");
      break;
    default:
      if (c < 0x20) {
        char buf[8];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        jw_appends(w, buf);
      } else {
        jw_putc(w, (char)c);
      }
    }
  }
  jw_putc(w, '"');
}

struct json_writer *json_writer_new(void) {
  return calloc(1, sizeof(struct json_writer));
}

void json_writer_free(struct json_writer *w) {
  if (!w)
    return;
  free(w->out);
  free(w->stack);
  free(w);
}

void json_writer_begin_object(struct json_writer *w) {
  if (!w)
    return;
  w_open(w);
  jw_putc(w, '{');
  jw_push(w, 'o', 0, 0);
}

void json_writer_end_object(struct json_writer *w) {
  if (!w || w->depth == 0)
    return;
  jw_putc(w, '}');
  jw_pop(w);
  w_close_value(w);
}

void json_writer_begin_array(struct json_writer *w) {
  if (!w)
    return;
  w_open(w);
  jw_putc(w, '[');
  jw_push(w, 'a', 0, 1);
}

void json_writer_end_array(struct json_writer *w) {
  if (!w || w->depth == 0)
    return;
  jw_putc(w, ']');
  jw_pop(w);
  w_close_value(w);
}

void json_writer_key(struct json_writer *w, const char *key) {
  if (!w || w->depth == 0)
    return;
  struct jw_frame *top = jw_top(w);
  if (top->kind != 'o')
    return;
  if (top->has_item)
    jw_putc(w, ',');
  escape_string(w, key);
  jw_putc(w, ':');
  top->expects_value = 1;
}

void json_writer_null(struct json_writer *w) {
  if (!w)
    return;
  w_open(w);
  jw_appends(w, "null");
  w_close_value(w);
}

void json_writer_bool(struct json_writer *w, int v) {
  if (!w)
    return;
  w_open(w);
  jw_appends(w, v ? "true" : "false");
  w_close_value(w);
}

void json_writer_int(struct json_writer *w, int64_t v) {
  if (!w)
    return;
  w_open(w);
  char buf[32];
  snprintf(buf, sizeof(buf), "%lld", (long long)v);
  jw_appends(w, buf);
  w_close_value(w);
}

void json_writer_float(struct json_writer *w, double v) {
  if (!w)
    return;
  w_open(w);
  char buf[64];
  snprintf(buf, sizeof(buf), "%g", v);
  jw_appends(w, buf);
  w_close_value(w);
}

void json_writer_string(struct json_writer *w, const char *s) {
  if (!w)
    return;
  w_open(w);
  escape_string(w, s ? s : "");
  w_close_value(w);
}

void json_writer_raw(struct json_writer *w, const char *json) {
  if (!w)
    return;
  w_open(w);
  jw_appends(w, json && *json ? json : "null");
  w_close_value(w);
}

const char *json_writer_data(struct json_writer *w, size_t *len_out) {
  if (!w || !w->out) {
    if (len_out)
      *len_out = 0;
    return "";
  }
  if (len_out)
    *len_out = w->len;
  return w->out;
}
