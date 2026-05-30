/* GENERATED — do not edit. */
#include <yaafc/yclass/rpc.h>
#include <yaafc/yclass/jinvoke.h>
#include <yaafc/yclass/yheaders.h>
#include <yaafc/yjson/yjson.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/ycore/yspan.h>
#include <yaafc/yclass/class.h>
#include "github_authn.internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t github_authn_store_set_credentials_skel(const void *_body, size_t _body_len,
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
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int_result _r = github_authn_store_set_credentials(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.github_authn_store_set_credentials dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.github_authn_store_set_credentials", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] github_authn_store_set_credentials", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            yaafc_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        yaafc_error_destroy(_r.error);
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

static size_t github_authn_store_register_code_skel(const void *_body, size_t _body_len,
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
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int_result _r = github_authn_store_register_code(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.github_authn_store_register_code dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.github_authn_store_register_code", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] github_authn_store_register_code", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            yaafc_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        yaafc_error_destroy(_r.error);
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

static size_t github_authn_store_resolve_skel(const void *_body, size_t _body_len,
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
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_uint32_result _r = github_authn_store_resolve(&_local, _obj, _hdrs, _v1);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.github_authn_store_resolve dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.github_authn_store_resolve", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] github_authn_store_resolve", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            yaafc_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        yaafc_error_destroy(_r.error);
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

static size_t github_authn_store_count_codes_skel(const void *_body, size_t _body_len,
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
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_size_result _r = github_authn_store_count_codes(&_local, _obj, _hdrs);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.github_authn_store_count_codes dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.github_authn_store_count_codes", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] github_authn_store_count_codes", _r.error);
        const char *_msg = _r.error.msg ? _r.error.msg : "(no msg)";
        uint32_t _ml = (uint32_t)strlen(_msg);
        if (_ml > 256) _ml = 256;
        if (_resp_max < 1 + 4 + _ml) {
            yaafc_error_destroy(_r.error);
            ((uint8_t *)_resp)[0] = 1;
            return _resp_max >= 1 ? 1 : 0;
        }
        ((uint8_t *)_resp)[0] = 1;
        memcpy((uint8_t *)_resp + 1, &_ml, 4);
        memcpy((uint8_t *)_resp + 5, _msg, _ml);
        yaafc_error_destroy(_r.error);
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

static int github_authn_store_set_credentials_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    uint32_t arg1 = (uint32_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int_result call_result = github_authn_store_set_credentials(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "github_authn_store_set_credentials",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int github_authn_store_register_code_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    uint32_t arg1 = (uint32_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int_result call_result = github_authn_store_register_code(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "github_authn_store_register_code",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int github_authn_store_resolve_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    uint32_t arg0 = (uint32_t)yjson_as_int(yjson_array_at(args, 0), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_uint32_result call_result = github_authn_store_resolve(call_ctx, obj, hdrs, arg0);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "github_authn_store_resolve",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int github_authn_store_count_codes_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_size_result call_result = github_authn_store_count_codes(call_ctx, obj, hdrs);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "github_authn_store_count_codes",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

struct object_ptr_result github_authn_store_create(struct ctx *ctx)
{
    ydebug("class=github_authn_store");
    struct class_ptr_result _kr = github_authn_store_class_get();
    if (YAAFC_IS_ERR(_kr))
        return YAAFC_ERR(object_ptr, "github_authn_store_create: class accessor failed", _kr);
    /* A service dependency is acquired once and cached for the connection
     * (remote) / process (in-process) lifetime — no per-call create. */
    return rpc_object_acquire(ctx, _kr.value, "github_authn_store");
}


/* ---- github_authn: jinvoke table ------------------------------------ */

struct github_authn_jinvoke_row { const char *name; jinvoke_fn fn; };

static const struct github_authn_jinvoke_row github_authn_jinvoke_rows[] = {
    {"github_authn_store_set_credentials", github_authn_store_set_credentials_jinvoke},
    {"github_authn_store_register_code", github_authn_store_register_code_jinvoke},
    {"github_authn_store_resolve", github_authn_store_resolve_jinvoke},
    {"github_authn_store_count_codes", github_authn_store_count_codes_jinvoke}
};

static jinvoke_fn github_authn_jinvoke_lookup(const char *qname)
{
    for (size_t i = 0;
         i < sizeof(github_authn_jinvoke_rows) / sizeof(github_authn_jinvoke_rows[0]); ++i)
        if (strcmp(github_authn_jinvoke_rows[i].name, qname) == 0)
            return github_authn_jinvoke_rows[i].fn;
    return NULL;
}
/* ---- github_authn: class name → accessor (lazy) ---------------------- */

static struct class_ptr_result github_authn_accessor_lookup(const char *name)
{
    if (strcmp(name, "github_authn_store") == 0) return github_authn_store_class_get();
    return YAAFC_OK(class_ptr, NULL);
}

/* ---- github_authn: slot → skel, name-keyed static data --------------- */

struct github_authn_skel_row { const char *name; rpc_skel_fn fn; };

static const struct github_authn_skel_row github_authn_skel_rows[] = {
    {"github_authn_store_set_credentials", github_authn_store_set_credentials_skel},
    {"github_authn_store_register_code", github_authn_store_register_code_skel},
    {"github_authn_store_resolve", github_authn_store_resolve_skel},
    {"github_authn_store_count_codes", github_authn_store_count_codes_skel}
};

static rpc_skel_fn github_authn_skel_lookup(method_slot slot)
{
    struct const_char_ptr_result nr = method_slot_name(slot);
    if (YAAFC_IS_ERR(nr)) { yaafc_error_destroy(nr.error); return NULL; }
    const char *name = nr.value;
    for (size_t i = 0; i < sizeof(github_authn_skel_rows) / sizeof(github_authn_skel_rows[0]); ++i)
        if (strcmp(github_authn_skel_rows[i].name, name) == 0)
            return github_authn_skel_rows[i].fn;
    return NULL;
}

/* ---- github_authn: install hooks before main ------------------------- */

__attribute__((constructor))
static void github_authn_install_hooks(void)
{
    struct yaafc_void_result _ar = class_add_accessor_lookup(github_authn_accessor_lookup);
    if (YAAFC_IS_ERR(_ar)) {
        yaafc_error_print(stderr, "github_authn_install_hooks", _ar.error);
        yaafc_error_destroy(_ar.error);
        abort();
    }
    rpc_add_skel_lookup(github_authn_skel_lookup);
    jinvoke_add_lookup(github_authn_jinvoke_lookup);
}
