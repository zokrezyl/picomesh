/* GENERATED — do not edit. */
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/jinvoke.h>
#include <picomesh/yclass/yheaders.h>
#include <picomesh/yjson/yjson.h>
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/class.h>
#include "session.internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t session_store_start_skel(const void *_body, size_t _body_len,
                          void *_resp, size_t _resp_max)
{
    size_t _off = 0;
    struct ctx _local = {0};
    /* The framework header section is first on every CALL body — parse
     * it back into the `hdrs` argument before the packed business args. */
    struct yheaders *_hdrs = NULL;
    {
        size_t _hconsumed = 0;
        _hdrs = yheaders_parse(_body, _body_len, &_hconsumed);
        if (!_hdrs) goto _short_body;
        _off = _hconsumed;
    }
    struct object *_obj = NULL;
    {
        if (_off + 8 > _body_len) goto _short_body;
        uint64_t _h;
        memcpy(&_h, (const uint8_t *)_body + _off, 8); _off += 8;
        _obj = (struct object *)rpc_handle_resolve(_h);
    }
    uint32_t _v1 = 0;
    if (_off + sizeof(_v1) > _body_len) goto _short_body;
    memcpy(&_v1, (const uint8_t *)_body + _off, sizeof(_v1));
    _off += sizeof(_v1);
    uint32_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.session_store_start");
    struct picomesh_string_result _r = session_store_start(&_local, _obj, _hdrs, _v1, _v2);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] session_store_start", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            picomesh_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        picomesh_error_destroy(_r.error);
        return 1 + 4 + _ml;
    }
    {
        const char *_sv = _r.value ? _r.value : "";
        uint32_t _svlen = (uint32_t)strlen(_sv);
        if (_resp_max < 1 + 4 + (size_t)_svlen) { free(_r.value); return 0; }
        ((uint8_t *)_resp)[0] = 0;
        memcpy((uint8_t *)_resp + 1, &_svlen, 4);
        if (_svlen) memcpy((uint8_t *)_resp + 5, _sv, _svlen);
        free(_r.value);
        return 1 + 4 + (size_t)_svlen;
    }
_short_body:
    yheaders_free(_hdrs);
    if (_resp_max >= 1) ((uint8_t *)_resp)[0] = 1;
    return _resp_max >= 1 ? 1 : 0;
}

static size_t session_store_lookup_skel(const void *_body, size_t _body_len,
                          void *_resp, size_t _resp_max)
{
    size_t _off = 0;
    struct ctx _local = {0};
    /* The framework header section is first on every CALL body — parse
     * it back into the `hdrs` argument before the packed business args. */
    struct yheaders *_hdrs = NULL;
    {
        size_t _hconsumed = 0;
        _hdrs = yheaders_parse(_body, _body_len, &_hconsumed);
        if (!_hdrs) goto _short_body;
        _off = _hconsumed;
    }
    struct object *_obj = NULL;
    {
        if (_off + 8 > _body_len) goto _short_body;
        uint64_t _h;
        memcpy(&_h, (const uint8_t *)_body + _off, 8); _off += 8;
        _obj = (struct object *)rpc_handle_resolve(_h);
    }
    char _s1[4096];
    {
        if (_off + 4 > _body_len) goto _short_body;
        uint32_t _slen;
        memcpy(&_slen, (const uint8_t *)_body + _off, 4); _off += 4;
        if (_off + _slen > _body_len) goto _short_body;
        if (_slen >= sizeof(_s1)) goto _short_body;
        if (_slen) memcpy(_s1, (const uint8_t *)_body + _off, _slen);
        _s1[_slen] = 0; _off += _slen;
    }
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.session_store_lookup");
    struct picomesh_uint32_result _r = session_store_lookup(&_local, _obj, _hdrs, _s1);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] session_store_lookup", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            picomesh_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        picomesh_error_destroy(_r.error);
        return 1 + 4 + _ml;
    }
    if (_resp_max < 1 + sizeof(_r.value)) return 0;
    ((uint8_t *)_resp)[0] = 0;
    memcpy((uint8_t *)_resp + 1, &_r.value, sizeof(_r.value));
    return 1 + sizeof(_r.value);
_short_body:
    yheaders_free(_hdrs);
    if (_resp_max >= 1) ((uint8_t *)_resp)[0] = 1;
    return _resp_max >= 1 ? 1 : 0;
}

static size_t session_store_destroy_skel(const void *_body, size_t _body_len,
                          void *_resp, size_t _resp_max)
{
    size_t _off = 0;
    struct ctx _local = {0};
    /* The framework header section is first on every CALL body — parse
     * it back into the `hdrs` argument before the packed business args. */
    struct yheaders *_hdrs = NULL;
    {
        size_t _hconsumed = 0;
        _hdrs = yheaders_parse(_body, _body_len, &_hconsumed);
        if (!_hdrs) goto _short_body;
        _off = _hconsumed;
    }
    struct object *_obj = NULL;
    {
        if (_off + 8 > _body_len) goto _short_body;
        uint64_t _h;
        memcpy(&_h, (const uint8_t *)_body + _off, 8); _off += 8;
        _obj = (struct object *)rpc_handle_resolve(_h);
    }
    char _s1[4096];
    {
        if (_off + 4 > _body_len) goto _short_body;
        uint32_t _slen;
        memcpy(&_slen, (const uint8_t *)_body + _off, 4); _off += 4;
        if (_off + _slen > _body_len) goto _short_body;
        if (_slen >= sizeof(_s1)) goto _short_body;
        if (_slen) memcpy(_s1, (const uint8_t *)_body + _off, _slen);
        _s1[_slen] = 0; _off += _slen;
    }
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.session_store_destroy");
    struct picomesh_int_result _r = session_store_destroy(&_local, _obj, _hdrs, _s1);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] session_store_destroy", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            picomesh_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        picomesh_error_destroy(_r.error);
        return 1 + 4 + _ml;
    }
    if (_resp_max < 1 + sizeof(_r.value)) return 0;
    ((uint8_t *)_resp)[0] = 0;
    memcpy((uint8_t *)_resp + 1, &_r.value, sizeof(_r.value));
    return 1 + sizeof(_r.value);
_short_body:
    yheaders_free(_hdrs);
    if (_resp_max >= 1) ((uint8_t *)_resp)[0] = 1;
    return _resp_max >= 1 ? 1 : 0;
}

static size_t session_store_count_active_skel(const void *_body, size_t _body_len,
                          void *_resp, size_t _resp_max)
{
    size_t _off = 0;
    struct ctx _local = {0};
    /* The framework header section is first on every CALL body — parse
     * it back into the `hdrs` argument before the packed business args. */
    struct yheaders *_hdrs = NULL;
    {
        size_t _hconsumed = 0;
        _hdrs = yheaders_parse(_body, _body_len, &_hconsumed);
        if (!_hdrs) goto _short_body;
        _off = _hconsumed;
    }
    struct object *_obj = NULL;
    {
        if (_off + 8 > _body_len) goto _short_body;
        uint64_t _h;
        memcpy(&_h, (const uint8_t *)_body + _off, 8); _off += 8;
        _obj = (struct object *)rpc_handle_resolve(_h);
    }
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.session_store_count_active");
    struct picomesh_size_result _r = session_store_count_active(&_local, _obj, _hdrs);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] session_store_count_active", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            picomesh_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        picomesh_error_destroy(_r.error);
        return 1 + 4 + _ml;
    }
    if (_resp_max < 1 + sizeof(_r.value)) return 0;
    ((uint8_t *)_resp)[0] = 0;
    memcpy((uint8_t *)_resp + 1, &_r.value, sizeof(_r.value));
    return 1 + sizeof(_r.value);
_short_body:
    yheaders_free(_hdrs);
    if (_resp_max >= 1) ((uint8_t *)_resp)[0] = 1;
    return _resp_max >= 1 ? 1 : 0;
}

static int session_store_start_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    uint32_t arg1 = (uint32_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_string_result call_result = session_store_start(call_ctx, obj, hdrs, arg0, arg1);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "session_store_start",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_string(result, call_result.value ? call_result.value : "");
    free(call_result.value);
    return 0;
}

static int session_store_lookup_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    const char *arg0 = yjson_as_string(yjson_array_at(args, 0), "");
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_uint32_result call_result = session_store_lookup(call_ctx, obj, hdrs, arg0);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "session_store_lookup",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int session_store_destroy_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    const char *arg0 = yjson_as_string(yjson_array_at(args, 0), "");
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_int_result call_result = session_store_destroy(call_ctx, obj, hdrs, arg0);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "session_store_destroy",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int session_store_count_active_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_size_result call_result = session_store_count_active(call_ctx, obj, hdrs);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "session_store_count_active",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

struct object_ptr_result session_store_create(struct ctx *ctx)
{
    ydebug("class=session_store");
    struct class_ptr_result _kr = session_store_class_get();
    if (PICOMESH_IS_ERR(_kr))
        return PICOMESH_ERR(object_ptr, "session_store_create: class accessor failed", _kr);
    /* A service dependency is acquired once and cached for the connection
     * (remote) / process (in-process) lifetime — no per-call create. */
    return rpc_object_acquire(ctx, _kr.value, "session_store");
}


/* ---- session: jinvoke table ------------------------------------ */

struct session_jinvoke_row { const char *name; jinvoke_fn fn; };

static const struct session_jinvoke_row session_jinvoke_rows[] = {
    {"session_store_start", session_store_start_jinvoke},
    {"session_store_lookup", session_store_lookup_jinvoke},
    {"session_store_destroy", session_store_destroy_jinvoke},
    {"session_store_count_active", session_store_count_active_jinvoke}
};

static jinvoke_fn session_jinvoke_lookup(const char *qname)
{
    for (size_t i = 0;
         i < sizeof(session_jinvoke_rows) / sizeof(session_jinvoke_rows[0]); ++i)
        if (strcmp(session_jinvoke_rows[i].name, qname) == 0)
            return session_jinvoke_rows[i].fn;
    return NULL;
}
/* ---- session: class name → accessor (lazy) ---------------------- */

static struct class_ptr_result session_accessor_lookup(const char *name)
{
    if (strcmp(name, "session_store") == 0) return session_store_class_get();
    return PICOMESH_OK(class_ptr, NULL);
}

/* ---- session: slot → skel, name-keyed static data --------------- */

struct session_skel_row { const char *name; rpc_skel_fn fn; };

static const struct session_skel_row session_skel_rows[] = {
    {"session_store_start", session_store_start_skel},
    {"session_store_lookup", session_store_lookup_skel},
    {"session_store_destroy", session_store_destroy_skel},
    {"session_store_count_active", session_store_count_active_skel}
};

static rpc_skel_fn session_skel_lookup(method_slot slot)
{
    struct const_char_ptr_result nr = method_slot_name(slot);
    if (PICOMESH_IS_ERR(nr)) { picomesh_error_destroy(nr.error); return NULL; }
    const char *name = nr.value;
    for (size_t i = 0; i < sizeof(session_skel_rows) / sizeof(session_skel_rows[0]); ++i)
        if (strcmp(session_skel_rows[i].name, name) == 0)
            return session_skel_rows[i].fn;
    return NULL;
}

/* ---- session: registration entry point (called from the driver for
 *      config-ACTIVATED plugins only — registration is activation) ---- */

void picomesh_plugin_session_register(void)
{
    struct picomesh_void_result _ar = class_add_accessor_lookup(session_accessor_lookup);
    if (PICOMESH_IS_ERR(_ar)) {
        picomesh_error_print(stderr, "picomesh_plugin_session_register", _ar.error);
        picomesh_error_destroy(_ar.error);
        abort();
    }
    rpc_add_skel_lookup(session_skel_lookup);
    jinvoke_add_lookup(session_jinvoke_lookup);
    { struct class_ptr_result reg = session_store_class_get();
      if (PICOMESH_IS_ERR(reg)) picomesh_error_destroy(reg.error); }
}
