/* yjson — C ABI shim over simdjson + a small writer.
 *
 * Compiled as C++17. The C ABI is opaque: callers pass around
 * `struct yjson_doc *` / `struct yjson_value *` borrowed pointers.
 *
 * Internally we use simdjson's DOM parser (one parser + parsed doc
 * per yjson_doc). Borrowed values point into the doc; freeing the
 * doc invalidates all borrowed pointers.
 *
 * The writer is a tiny manual JSON serializer — simdjson doesn't
 * ship a stable builder API, and our outbound shapes are simple
 * enough that hand-rolling beats the dependency lift. */

#include <picomesh/yjson/yjson.h>

#include <simdjson.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

namespace sj = simdjson;

/* ----------------------------------------------------------------- */
/* yjson_doc / yjson_value internals                                  */
/* ----------------------------------------------------------------- */

/* yjson_value is opaque to C. Under the hood it's a simdjson DOM
 * element — but simdjson DOM elements are values (not pointers), so
 * we store them in a deque-like container and hand out pointers to
 * stable slots.
 *
 * Strings come from simdjson as `std::string_view`. The DOM element
 * keeps them alive for the lifetime of the parsed buffer, so we
 * lazily NUL-terminate into our own arena when callers ask for a
 * C-string. */

struct yjson_doc;

struct yjson_value {
    sj::dom::element elem;
    /* Owning doc — child handles (object_get / array_at) are allocated
     * into doc->values so they're freed with the doc. */
    yjson_doc *doc = nullptr;
    /* lazy NUL-terminated cache for as_string */
    mutable std::string str_cache;
    mutable bool str_cached = false;
};

struct yjson_doc {
    sj::dom::parser parser;
    sj::padded_string buf;
    sj::dom::element root;

    /* Stable storage for yjson_value handles we've handed out. Using
     * deque would invalidate-never, but std::list is overkill — we
     * use a chunked vector. Pointer stability comes from never
     * resizing existing chunks. */
    std::vector<std::unique_ptr<yjson_value>> values;
};

/* Per-thread last-error string for the parse failure path. */
static thread_local std::string g_last_err;

static yjson_value *make_value(yjson_doc *doc, sj::dom::element e)
{
    auto v = std::make_unique<yjson_value>();
    v->elem = e;
    v->doc = doc;
    yjson_value *raw = v.get();
    doc->values.push_back(std::move(v));
    return raw;
}

/* ----------------------------------------------------------------- */
/* Parse                                                              */
/* ----------------------------------------------------------------- */

extern "C" struct yjson_doc *yjson_parse(const char *data, size_t len)
{
    auto doc = std::make_unique<yjson_doc>();
    /* simdjson needs a SIMDJSON_PADDING tail to vectorize the tail
     * load safely. padded_string copies + pads. */
    doc->buf = sj::padded_string(data, len);
    auto res = doc->parser.parse(doc->buf);
    if (res.error()) {
        g_last_err = sj::error_message(res.error());
        return nullptr;
    }
    doc->root = res.value();
    return doc.release();
}

extern "C" void yjson_doc_free(struct yjson_doc *doc)
{
    delete doc;
}

extern "C" const struct yjson_value *yjson_doc_root(const struct yjson_doc *doc)
{
    if (!doc) return nullptr;
    /* The root needs a stable yjson_value*; make_value mutates the
     * doc's vector, so cast away const for this one allocation. */
    auto *mut = const_cast<yjson_doc *>(doc);
    return make_value(mut, doc->root);
}

extern "C" const char *yjson_last_error(void)
{
    if (g_last_err.empty()) return nullptr;
    return g_last_err.c_str();
}

/* ----------------------------------------------------------------- */
/* Type queries / scalar getters                                      */
/* ----------------------------------------------------------------- */

extern "C" enum yjson_kind yjson_kind(const struct yjson_value *v)
{
    if (!v) return YJSON_NULL;
    switch (v->elem.type()) {
    case sj::dom::element_type::ARRAY:    return YJSON_ARRAY;
    case sj::dom::element_type::OBJECT:   return YJSON_OBJECT;
    case sj::dom::element_type::STRING:   return YJSON_STRING;
    case sj::dom::element_type::INT64:
    case sj::dom::element_type::UINT64:   return YJSON_INT;
    case sj::dom::element_type::DOUBLE:   return YJSON_FLOAT;
    case sj::dom::element_type::BOOL:     return YJSON_BOOL;
    case sj::dom::element_type::NULL_VALUE:
    default:                              return YJSON_NULL;
    }
}

extern "C" int yjson_is_null  (const struct yjson_value *v) { return yjson_kind(v) == YJSON_NULL; }
extern "C" int yjson_is_bool  (const struct yjson_value *v) { return yjson_kind(v) == YJSON_BOOL; }
extern "C" int yjson_is_int   (const struct yjson_value *v) { return yjson_kind(v) == YJSON_INT; }
extern "C" int yjson_is_float (const struct yjson_value *v) { return yjson_kind(v) == YJSON_FLOAT; }
extern "C" int yjson_is_string(const struct yjson_value *v) { return yjson_kind(v) == YJSON_STRING; }
extern "C" int yjson_is_array (const struct yjson_value *v) { return yjson_kind(v) == YJSON_ARRAY; }
extern "C" int yjson_is_object(const struct yjson_value *v) { return yjson_kind(v) == YJSON_OBJECT; }

extern "C" int yjson_as_bool(const struct yjson_value *v, int fallback)
{
    if (!v) return fallback;
    bool b;
    if (v->elem.get(b) == sj::SUCCESS) return b ? 1 : 0;
    int64_t i;
    if (v->elem.get(i) == sj::SUCCESS) return i != 0;
    return fallback;
}

extern "C" int64_t yjson_as_int(const struct yjson_value *v, int64_t fallback)
{
    if (!v) return fallback;
    int64_t i;
    if (v->elem.get(i) == sj::SUCCESS) return i;
    uint64_t u;
    if (v->elem.get(u) == sj::SUCCESS) return (int64_t)u;
    double d;
    if (v->elem.get(d) == sj::SUCCESS) return (int64_t)d;
    bool b;
    if (v->elem.get(b) == sj::SUCCESS) return b ? 1 : 0;
    return fallback;
}

extern "C" double yjson_as_float(const struct yjson_value *v, double fallback)
{
    if (!v) return fallback;
    double d;
    if (v->elem.get(d) == sj::SUCCESS) return d;
    int64_t i;
    if (v->elem.get(i) == sj::SUCCESS) return (double)i;
    return fallback;
}

extern "C" const char *yjson_as_string(const struct yjson_value *v, const char *fallback)
{
    if (!v) return fallback;
    std::string_view sv;
    if (v->elem.get(sv) != sj::SUCCESS) return fallback;
    if (!v->str_cached) {
        v->str_cache.assign(sv.data(), sv.size());
        v->str_cached = true;
    }
    return v->str_cache.c_str();
}

/* ----------------------------------------------------------------- */
/* Container access                                                   */
/* ----------------------------------------------------------------- */

extern "C" size_t yjson_array_size(const struct yjson_value *v)
{
    if (!v) return 0;
    sj::dom::array arr;
    if (v->elem.get(arr) != sj::SUCCESS) return 0;
    return arr.size();
}

extern "C" const struct yjson_value *yjson_array_at(const struct yjson_value *v, size_t idx)
{
    if (!v) return nullptr;
    sj::dom::array arr;
    if (v->elem.get(arr) != sj::SUCCESS) return nullptr;
    if (idx >= arr.size()) return nullptr;
    auto e = arr.at(idx);
    if (e.error()) return nullptr;
    /* Child handle is owned by the same doc we descend from, so it is
     * freed with yjson_doc_free — no thread-local arena (which used to
     * grow without bound, one entry per call, and leaked). */
    if (!v->doc) return nullptr;
    return make_value(v->doc, e.value());
}

extern "C" const struct yjson_value *yjson_object_get(const struct yjson_value *v, const char *key)
{
    if (!v || !key) return nullptr;
    sj::dom::object obj;
    if (v->elem.get(obj) != sj::SUCCESS) return nullptr;
    auto e = obj[key];
    if (e.error()) return nullptr;
    /* Owned by the descending doc → freed with yjson_doc_free. (Was a
     * never-cleared thread-local arena that leaked one handle per call.) */
    if (!v->doc) return nullptr;
    return make_value(v->doc, e.value());
}

/* ----------------------------------------------------------------- */
/* Writer                                                              */
/* ----------------------------------------------------------------- */
/*
 * Tracks containers on a small stack so we can emit commas correctly.
 * Each "slot" position remembers whether something has been written
 * inside the current container (controls leading-comma insertion).
 *
 * Container kinds:
 *   'o' = object, expect alternating key / value
 *   'a' = array, expect values
 *   ' ' = top level (one value only)
 */

struct yjson_writer {
    std::string out;
    struct frame { char kind; int has_item; int expects_value; };
    std::vector<frame> stack;
};

static void w_open(yjson_writer *w)
{
    if (w->stack.empty()) {
        w->stack.push_back({' ', 0, 1});
        return;
    }
    auto &top = w->stack.back();
    if (top.kind == 'a') {
        if (top.has_item) w->out.push_back(',');
        top.has_item = 1;
    } else if (top.kind == 'o') {
        if (!top.expects_value) {
            /* We're at a key position — caller should have called _key
             * first. Treat as an error: write an empty key. */
            w->out.append("\"\":");
            top.expects_value = 1;
        } else {
            top.expects_value = 0;
            top.has_item = 1;
        }
    }
}

static void w_close_value(yjson_writer *w)
{
    if (w->stack.empty()) return;
    auto &top = w->stack.back();
    if (top.kind == 'o') {
        /* Just wrote a value; next is key again. */
        top.expects_value = 0;
    }
}

static void escape_string(std::string &out, const char *s)
{
    out.push_back('"');
    if (!s) { out.push_back('"'); return; }
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
        case '"':  out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b");  break;
        case '\f': out.append("\\f");  break;
        case '\n': out.append("\\n");  break;
        case '\r': out.append("\\r");  break;
        case '\t': out.append("\\t");  break;
        default:
            if (c < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                out.append(buf);
            } else {
                out.push_back((char)c);
            }
        }
    }
    out.push_back('"');
}

extern "C" struct yjson_writer *yjson_writer_new(void)
{
    return new (std::nothrow) yjson_writer();
}

extern "C" void yjson_writer_free(struct yjson_writer *w)
{
    delete w;
}

extern "C" void yjson_writer_begin_object(struct yjson_writer *w)
{
    if (!w) return;
    w_open(w);
    w->out.push_back('{');
    w->stack.push_back({'o', 0, 0});
}

extern "C" void yjson_writer_end_object(struct yjson_writer *w)
{
    if (!w || w->stack.empty()) return;
    w->out.push_back('}');
    w->stack.pop_back();
    w_close_value(w);
}

extern "C" void yjson_writer_begin_array(struct yjson_writer *w)
{
    if (!w) return;
    w_open(w);
    w->out.push_back('[');
    w->stack.push_back({'a', 0, 1});
}

extern "C" void yjson_writer_end_array(struct yjson_writer *w)
{
    if (!w || w->stack.empty()) return;
    w->out.push_back(']');
    w->stack.pop_back();
    w_close_value(w);
}

extern "C" void yjson_writer_key(struct yjson_writer *w, const char *key)
{
    if (!w || w->stack.empty()) return;
    auto &top = w->stack.back();
    if (top.kind != 'o') return;
    if (top.has_item) w->out.push_back(',');
    escape_string(w->out, key);
    w->out.push_back(':');
    top.expects_value = 1;
}

extern "C" void yjson_writer_null(struct yjson_writer *w)
{
    if (!w) return; w_open(w); w->out.append("null"); w_close_value(w);
}

extern "C" void yjson_writer_bool(struct yjson_writer *w, int v)
{
    if (!w) return; w_open(w);
    w->out.append(v ? "true" : "false");
    w_close_value(w);
}

extern "C" void yjson_writer_int(struct yjson_writer *w, int64_t v)
{
    if (!w) return; w_open(w);
    char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)v);
    w->out.append(buf);
    w_close_value(w);
}

extern "C" void yjson_writer_float(struct yjson_writer *w, double v)
{
    if (!w) return; w_open(w);
    char buf[64]; snprintf(buf, sizeof(buf), "%g", v);
    w->out.append(buf);
    w_close_value(w);
}

extern "C" void yjson_writer_string(struct yjson_writer *w, const char *s)
{
    if (!w) return; w_open(w);
    escape_string(w->out, s ? s : "");
    w_close_value(w);
}

extern "C" void yjson_writer_raw(struct yjson_writer *w, const char *json)
{
    if (!w) return; w_open(w);
    w->out.append(json && *json ? json : "null");
    w_close_value(w);
}

extern "C" const char *yjson_writer_data(struct yjson_writer *w, size_t *len_out)
{
    if (!w) { if (len_out) *len_out = 0; return ""; }
    if (len_out) *len_out = w->out.size();
    return w->out.c_str();
}
