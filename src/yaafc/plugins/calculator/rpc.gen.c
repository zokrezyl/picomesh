/* GENERATED — do not edit. */
#include <yaafc/yclass/rpc.h>
#include <yaafc/yclass/jinvoke.h>
#include <yaafc/yclass/yheaders.h>
#include <yaafc/yjson/yjson.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/ycore/yspan.h>
#include <yaafc/yclass/class.h>
#include "calculator.internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calculator_calc_add_skel(const void *_body, size_t _body_len,
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
    int64_t _v1 = 0;
    if (_off + sizeof(_v1) > _body_len) goto _short_body;
    memcpy(&_v1, (const uint8_t *)_body + _off, sizeof(_v1));
    _off += sizeof(_v1);
    int64_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int64_result _r = calculator_calc_add(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.calculator_calc_add dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.calculator_calc_add", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] calculator_calc_add", _r.error);
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

static size_t calculator_calc_sub_skel(const void *_body, size_t _body_len,
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
    int64_t _v1 = 0;
    if (_off + sizeof(_v1) > _body_len) goto _short_body;
    memcpy(&_v1, (const uint8_t *)_body + _off, sizeof(_v1));
    _off += sizeof(_v1);
    int64_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int64_result _r = calculator_calc_sub(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.calculator_calc_sub dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.calculator_calc_sub", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] calculator_calc_sub", _r.error);
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

static size_t calculator_calc_mul_skel(const void *_body, size_t _body_len,
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
    int64_t _v1 = 0;
    if (_off + sizeof(_v1) > _body_len) goto _short_body;
    memcpy(&_v1, (const uint8_t *)_body + _off, sizeof(_v1));
    _off += sizeof(_v1);
    int64_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int64_result _r = calculator_calc_mul(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.calculator_calc_mul dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.calculator_calc_mul", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] calculator_calc_mul", _r.error);
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

static size_t calculator_calc_div_skel(const void *_body, size_t _body_len,
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
    int64_t _v1 = 0;
    if (_off + sizeof(_v1) > _body_len) goto _short_body;
    memcpy(&_v1, (const uint8_t *)_body + _off, sizeof(_v1));
    _off += sizeof(_v1);
    int64_t _v2 = 0;
    if (_off + sizeof(_v2) > _body_len) goto _short_body;
    memcpy(&_v2, (const uint8_t *)_body + _off, sizeof(_v2));
    _off += sizeof(_v2);
    double span_start = yaafc_ytime_monotonic_sec();
    struct yaafc_int64_result _r = calculator_calc_div(&_local, _obj, _hdrs, _v1, _v2);
    {
        double span_us = (yaafc_ytime_monotonic_sec() - span_start) * 1e6;
        const char *span_trace = _hdrs ? yheaders_get(_hdrs, "trace_id") : "-";
        ydebug("span trace=%s op=skel.calculator_calc_div dur_us=%.0f", span_trace ? span_trace : "-", span_us);
        yspan_record("skel.calculator_calc_div", span_us);
    }
    yheaders_free(_hdrs); _hdrs = NULL;
    if (_resp_max < 1) return 0;
    if (YAAFC_IS_ERR(_r)) {
        yaafc_error_print(stderr, "[skel] calculator_calc_div", _r.error);
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

static int calculator_calc_add_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    int64_t arg0 = (int64_t)yjson_as_int(yjson_array_at(args, 0), 0);
    int64_t arg1 = (int64_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int64_result call_result = calculator_calc_add(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "calculator_calc_add",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int calculator_calc_sub_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    int64_t arg0 = (int64_t)yjson_as_int(yjson_array_at(args, 0), 0);
    int64_t arg1 = (int64_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int64_result call_result = calculator_calc_sub(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "calculator_calc_sub",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int calculator_calc_mul_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    int64_t arg0 = (int64_t)yjson_as_int(yjson_array_at(args, 0), 0);
    int64_t arg1 = (int64_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int64_result call_result = calculator_calc_mul(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "calculator_calc_mul",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

static int calculator_calc_div_jinvoke(struct ctx *ctx, struct object *obj, struct yheaders *hdrs,
                          const struct yjson_value *args,
                          struct yjson_writer *result, char *err, size_t err_cap)
{
    int64_t arg0 = (int64_t)yjson_as_int(yjson_array_at(args, 0), 0);
    int64_t arg1 = (int64_t)yjson_as_int(yjson_array_at(args, 1), 0);
    struct ctx local_ctx = {0};
    struct ctx *call_ctx = ctx ? ctx : &local_ctx;
    struct yaafc_int64_result call_result = calculator_calc_div(call_ctx, obj, hdrs, arg0, arg1);
    if (YAAFC_IS_ERR(call_result)) {
        snprintf(err, err_cap, "%s: %s", "calculator_calc_div",
                 call_result.error.msg ? call_result.error.msg : "<no message>");
        yaafc_error_destroy(call_result.error);
        return -1;
    }
    yjson_w_int(result, (int64_t)call_result.value);
    return 0;
}

struct object_ptr_result calculator_calc_create(struct ctx *ctx)
{
    ydebug("class=calculator_calc");
    struct class_ptr_result _kr = calculator_calc_class_get();
    if (YAAFC_IS_ERR(_kr))
        return YAAFC_ERR(object_ptr, "calculator_calc_create: class accessor failed", _kr);
    /* A service dependency is acquired once and cached for the connection
     * (remote) / process (in-process) lifetime — no per-call create. */
    return rpc_object_acquire(ctx, _kr.value, "calculator_calc");
}


/* ---- calculator: jinvoke table ------------------------------------ */

struct calculator_jinvoke_row { const char *name; jinvoke_fn fn; };

static const struct calculator_jinvoke_row calculator_jinvoke_rows[] = {
    {"calculator_calc_add", calculator_calc_add_jinvoke},
    {"calculator_calc_sub", calculator_calc_sub_jinvoke},
    {"calculator_calc_mul", calculator_calc_mul_jinvoke},
    {"calculator_calc_div", calculator_calc_div_jinvoke}
};

static jinvoke_fn calculator_jinvoke_lookup(const char *qname)
{
    for (size_t i = 0;
         i < sizeof(calculator_jinvoke_rows) / sizeof(calculator_jinvoke_rows[0]); ++i)
        if (strcmp(calculator_jinvoke_rows[i].name, qname) == 0)
            return calculator_jinvoke_rows[i].fn;
    return NULL;
}
/* ---- calculator: class name → accessor (lazy) ---------------------- */

static struct class_ptr_result calculator_accessor_lookup(const char *name)
{
    if (strcmp(name, "calculator_calc") == 0) return calculator_calc_class_get();
    return YAAFC_OK(class_ptr, NULL);
}

/* ---- calculator: slot → skel, name-keyed static data --------------- */

struct calculator_skel_row { const char *name; rpc_skel_fn fn; };

static const struct calculator_skel_row calculator_skel_rows[] = {
    {"calculator_calc_add", calculator_calc_add_skel},
    {"calculator_calc_sub", calculator_calc_sub_skel},
    {"calculator_calc_mul", calculator_calc_mul_skel},
    {"calculator_calc_div", calculator_calc_div_skel}
};

static rpc_skel_fn calculator_skel_lookup(method_slot slot)
{
    struct const_char_ptr_result nr = method_slot_name(slot);
    if (YAAFC_IS_ERR(nr)) { yaafc_error_destroy(nr.error); return NULL; }
    const char *name = nr.value;
    for (size_t i = 0; i < sizeof(calculator_skel_rows) / sizeof(calculator_skel_rows[0]); ++i)
        if (strcmp(calculator_skel_rows[i].name, name) == 0)
            return calculator_skel_rows[i].fn;
    return NULL;
}

/* ---- calculator: install hooks before main ------------------------- */

__attribute__((constructor))
static void calculator_install_hooks(void)
{
    struct yaafc_void_result _ar = class_add_accessor_lookup(calculator_accessor_lookup);
    if (YAAFC_IS_ERR(_ar)) {
        yaafc_error_print(stderr, "calculator_install_hooks", _ar.error);
        yaafc_error_destroy(_ar.error);
        abort();
    }
    rpc_add_skel_lookup(calculator_skel_lookup);
    jinvoke_add_lookup(calculator_jinvoke_lookup);
    { struct class_ptr_result reg = calculator_calc_class_get();
      if (YAAFC_IS_ERR(reg)) yaafc_error_destroy(reg.error); }
}
