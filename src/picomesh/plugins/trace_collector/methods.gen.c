/* GENERATED — do not edit. */
#include "trace_collector.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_void_result trace_collector_store_ingest(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * span_json)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_ingest);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_ingest");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_ingest: header serialize overflow");
                return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_ingest: pack overflow"); return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(span_json ? strlen(span_json) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_ingest: pack overflow"); return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, span_json, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_void, _msg[0] ? strdup(_msg) : "trace_collector_store_ingest: remote error (no msg)");
        }
        return PICOMESH_OK_VOID();
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_void, "trace_collector_store_ingest: no impl on this class");
        return ((trace_collector_store_ingest_fn)fn)(ctx, obj, hdrs, span_json);
    }
}

struct picomesh_string_result trace_collector_store_get_trace(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * trace_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_get_trace);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_get_trace");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_get_trace: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_get_trace: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(trace_id ? strlen(trace_id) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_get_trace: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, trace_id, _slen); _off += _slen; }
        }
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_get_trace: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_get_trace: no impl on this class");
        return ((trace_collector_store_get_trace_fn)fn)(ctx, obj, hdrs, trace_id);
    }
}

struct picomesh_string_result trace_collector_store_services(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_services);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_services");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_services: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_services: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_services: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_services: no impl on this class");
        return ((trace_collector_store_services_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_string_result trace_collector_store_operations(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * service)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_operations);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_operations");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_operations: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_operations: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(service ? strlen(service) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_operations: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, service, _slen); _off += _slen; }
        }
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_operations: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_operations: no impl on this class");
        return ((trace_collector_store_operations_fn)fn)(ctx, obj, hdrs, service);
    }
}

struct picomesh_string_result trace_collector_store_latency(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, const char * service, const char * operation, uint32_t window_secs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_latency);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_latency");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_latency: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_latency: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        {
            uint32_t _slen = (uint32_t)(service ? strlen(service) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_latency: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, service, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(operation ? strlen(operation) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_latency: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: pack overflow"); }
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, operation, _slen); _off += _slen; }
        }
        if (_off + sizeof(window_secs) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_latency: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: pack overflow"); }
        memcpy(_a + _off, &window_secs, sizeof(window_secs)); _off += sizeof(window_secs);
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_latency: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_latency: no impl on this class");
        return ((trace_collector_store_latency_fn)fn)(ctx, obj, hdrs, service, operation, window_secs);
    }
}

struct picomesh_string_result trace_collector_store_stats(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_stats);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_stats");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_stats: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_stats: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_stats: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_stats: no impl on this class");
        return ((trace_collector_store_stats_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_string_result trace_collector_store_errors(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t since_secs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("trace_collector", (method_id_t)trace_collector_store_errors);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.trace_collector_store_errors");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "trace_collector_store_errors: header serialize overflow");
                return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_errors: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(since_secs) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "trace_collector_store_errors: pack overflow"); return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: pack overflow"); }
        memcpy(_a + _off, &since_secs, sizeof(since_secs)); _off += sizeof(since_secs);
        uint8_t _wbuf[65536];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "trace_collector_store_errors: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "trace_collector_store_errors: no impl on this class");
        return ((trace_collector_store_errors_fn)fn)(ctx, obj, hdrs, since_secs);
    }
}

