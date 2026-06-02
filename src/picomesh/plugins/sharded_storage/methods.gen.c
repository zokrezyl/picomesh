/* GENERATED — do not edit. */
#include "sharded_storage.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_int_result sharded_storage_db_set(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key, const char * value)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_set);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_set");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_set: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(value ? strlen(value) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, value, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "sharded_storage_db_set: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_set: no impl on this class");
        return ((sharded_storage_db_set_fn)fn)(ctx, obj, hdrs, context, key, value);
    }
}

struct picomesh_string_result sharded_storage_db_get(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_get);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_get");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_get: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_get: pack overflow"); return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_get: pack overflow"); return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_get: pack overflow"); return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "sharded_storage_db_get: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "sharded_storage_db_get: no impl on this class");
        return ((sharded_storage_db_get_fn)fn)(ctx, obj, hdrs, context, key);
    }
}

struct picomesh_int_result sharded_storage_db_exists(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_exists);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_exists");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_exists: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_exists: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_exists: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_exists: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "sharded_storage_db_exists: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_exists: no impl on this class");
        return ((sharded_storage_db_exists_fn)fn)(ctx, obj, hdrs, context, key);
    }
}

struct picomesh_int_result sharded_storage_db_del(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_del);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_del");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_del: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_del: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_del: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_del: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "sharded_storage_db_del: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_del: no impl on this class");
        return ((sharded_storage_db_del_fn)fn)(ctx, obj, hdrs, context, key);
    }
}

struct picomesh_size_result sharded_storage_db_count(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_count);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_count");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_count: header serialize overflow");
                return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_count: pack overflow"); return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_count: pack overflow"); return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "sharded_storage_db_count: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "sharded_storage_db_count: no impl on this class");
        return ((sharded_storage_db_count_fn)fn)(ctx, obj, hdrs, context);
    }
}

struct picomesh_int64_result sharded_storage_db_incr(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key, int64_t delta)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_incr);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_incr");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_incr: header serialize overflow");
                return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_incr: pack overflow"); return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_incr: pack overflow"); return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_incr: pack overflow"); return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        if (_off + sizeof(delta) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_incr: pack overflow"); return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: pack overflow"); }
        memcpy(_a + _off, &delta, sizeof(delta)); _off += sizeof(delta);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int64, _msg[0] ? strdup(_msg) : "sharded_storage_db_incr: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int64_t)) return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: truncated RPC payload");
        int64_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int64, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int64, "sharded_storage_db_incr: no impl on this class");
        return ((sharded_storage_db_incr_fn)fn)(ctx, obj, hdrs, context, key, delta);
    }
}

struct picomesh_int_result sharded_storage_db_put_if_absent(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key, const char * value)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_put_if_absent);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_put_if_absent");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_put_if_absent: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_put_if_absent: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_put_if_absent: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_put_if_absent: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(value ? strlen(value) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_put_if_absent: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, value, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "sharded_storage_db_put_if_absent: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_put_if_absent: no impl on this class");
        return ((sharded_storage_db_put_if_absent_fn)fn)(ctx, obj, hdrs, context, key, value);
    }
}

struct picomesh_int_result sharded_storage_db_compare_and_set(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * context, const char * key, const char * expected, const char * replacement)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("sharded_storage", (method_id_t)sharded_storage_db_compare_and_set);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.sharded_storage_db_compare_and_set");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(context ? strlen(context) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, context, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(key ? strlen(key) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, key, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(expected ? strlen(expected) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, expected, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(replacement ? strlen(replacement) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "sharded_storage_db_compare_and_set: pack overflow"); return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, replacement, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "sharded_storage_db_compare_and_set: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "sharded_storage_db_compare_and_set: no impl on this class");
        return ((sharded_storage_db_compare_and_set_fn)fn)(ctx, obj, hdrs, context, key, expected, replacement);
    }
}

