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
#include "accounts.internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t accounts_store_register_skel(const void *_body, size_t _body_len,
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
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_register");
    struct picomesh_int_result _r = accounts_store_register(&_local, _obj, _hdrs, _v1);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_register", _r.error);
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

static size_t accounts_store_exists_skel(const void *_body, size_t _body_len,
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
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_exists");
    struct picomesh_int_result _r = accounts_store_exists(&_local, _obj, _hdrs, _v1);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_exists", _r.error);
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

static size_t accounts_store_set_balance_skel(const void *_body, size_t _body_len,
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
    int64_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_set_balance");
    struct picomesh_int_result _r = accounts_store_set_balance(&_local, _obj, _hdrs, _v1, _v2);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_set_balance", _r.error);
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

static size_t accounts_store_balance_skel(const void *_body, size_t _body_len,
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
    struct ytelemetry_span _tsp;
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_balance");
    struct picomesh_int64_result _r = accounts_store_balance(&_local, _obj, _hdrs, _v1);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_balance", _r.error);
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

static size_t accounts_store_count_skel(const void *_body, size_t _body_len,
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
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_count");
    struct picomesh_size_result _r = accounts_store_count(&_local, _obj, _hdrs);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_count", _r.error);
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

static size_t accounts_store_list_skel(const void *_body, size_t _body_len,
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
    ytelemetry_server_span_begin(&_tsp, _hdrs, "skel.accounts_store_list");
    struct picomesh_string_result _r = accounts_store_list(&_local, _obj, _hdrs);
    ytelemetry_span_end(&_tsp, !PICOMESH_IS_ERR(_r), PICOMESH_IS_ERR(_r) ? _r.error.msg : NULL);
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (PICOMESH_IS_ERR(_r)) {
        picomesh_error_print(stderr, "[skel] accounts_store_list", _r.error);
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

static int accounts_store_register_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_int_result call_result = accounts_store_register(call_ctx, obj, hdrs, arg0);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_register",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int accounts_store_exists_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_int_result call_result = accounts_store_exists(call_ctx, obj, hdrs, arg0);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_exists",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int accounts_store_set_balance_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    int64_t arg1 = (int64_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_int_result call_result = accounts_store_set_balance(call_ctx, obj, hdrs, arg0, arg1);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_set_balance",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int accounts_store_balance_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_int64_result call_result = accounts_store_balance(call_ctx, obj, hdrs, arg0);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_balance",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int accounts_store_count_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_size_result call_result = accounts_store_count(call_ctx, obj, hdrs);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_count",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int accounts_store_list_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct picomesh_string_result call_result = accounts_store_list(call_ctx, obj, hdrs);
    if (PICOMESH_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "accounts_store_list",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        picomesh_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_string(result, call_result.value ? call_result.value : "");
    free(call_result.value);
    return 0;
}

struct object_ptr_result accounts_store_create(struct ctx *ctx)
{
    ydebug("class=accounts_store");
    struct class_ptr_result _kr = accounts_store_class_get();
    if (PICOMESH_IS_ERR(_kr))
        return PICOMESH_ERR(object_ptr, "accounts_store_create: class accessor failed", _kr);
    /* A service dependency is acquired once and cached for the connection
     * (remote) / process (in-process) lifetime — no per-call create. */
    return rpc_object_acquire(ctx, _kr.value, "accounts_store");
}


/* ---- accounts: jinvoke table ------------------------------------ */

struct accounts_jinvoke_row { const char *name; jinvoke_fn fn; };

static const struct accounts_jinvoke_row accounts_jinvoke_rows[] = {
    {"accounts_store_register", accounts_store_register_jinvoke},
    {"accounts_store_exists", accounts_store_exists_jinvoke},
    {"accounts_store_set_balance", accounts_store_set_balance_jinvoke},
    {"accounts_store_balance", accounts_store_balance_jinvoke},
    {"accounts_store_count", accounts_store_count_jinvoke},
    {"accounts_store_list", accounts_store_list_jinvoke}
};

static jinvoke_fn accounts_jinvoke_lookup(const char *qname)
{
    for (size_t i = 0;
         i < sizeof(accounts_jinvoke_rows) / sizeof(accounts_jinvoke_rows[0]); ++i)
        if (strcmp(accounts_jinvoke_rows[i].name, qname) == 0)
            return accounts_jinvoke_rows[i].fn;
    return NULL;
}
/* ---- accounts: class name → accessor (lazy) ---------------------- */

static struct class_ptr_result accounts_accessor_lookup(const char *name)
{
    if (strcmp(name, "accounts_store") == 0) return accounts_store_class_get();
    return PICOMESH_OK(class_ptr, NULL);
}

/* ---- accounts: slot → skel, name-keyed static data --------------- */

struct accounts_skel_row { const char *name; rpc_skel_fn fn; };

static const struct accounts_skel_row accounts_skel_rows[] = {
    {"accounts_store_register", accounts_store_register_skel},
    {"accounts_store_exists", accounts_store_exists_skel},
    {"accounts_store_set_balance", accounts_store_set_balance_skel},
    {"accounts_store_balance", accounts_store_balance_skel},
    {"accounts_store_count", accounts_store_count_skel},
    {"accounts_store_list", accounts_store_list_skel}
};

static rpc_skel_fn accounts_skel_lookup(method_slot slot)
{
    struct const_char_ptr_result nr = method_slot_name(slot);
    if (PICOMESH_IS_ERR(nr)) { picomesh_error_destroy(nr.error); return NULL; }
    const char *name = nr.value;
    for (size_t i = 0; i < sizeof(accounts_skel_rows) / sizeof(accounts_skel_rows[0]); ++i)
        if (strcmp(accounts_skel_rows[i].name, name) == 0)
            return accounts_skel_rows[i].fn;
    return NULL;
}

/* ---- accounts: registration entry point (called from the driver for
 *      config-ACTIVATED plugins only — registration is activation) ---- */

void picomesh_plugin_accounts_register(void)
{
    struct picomesh_void_result _ar = class_add_accessor_lookup(accounts_accessor_lookup);
    if (PICOMESH_IS_ERR(_ar)) {
        picomesh_error_print(stderr, "picomesh_plugin_accounts_register", _ar.error);
        picomesh_error_destroy(_ar.error);
        abort();
    }
    rpc_add_skel_lookup(accounts_skel_lookup);
    jinvoke_add_lookup(accounts_jinvoke_lookup);
    { struct class_ptr_result reg = accounts_store_class_get();
      if (PICOMESH_IS_ERR(reg)) picomesh_error_destroy(reg.error); }
}
