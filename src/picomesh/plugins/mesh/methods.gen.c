/* GENERATED — do not edit. */
#include "mesh.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_int_result mesh_store_register_service(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t service_id, uint32_t port)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_register_service);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_register_service");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        if (_off + sizeof(port) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: pack overflow");
        memcpy(_a + _off, &port, sizeof(port)); _off += sizeof(port);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_register_service: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_register_service: no impl on this class");
        return ((mesh_store_register_service_fn)fn)(ctx, obj, hdrs, service_id, port);
    }
}

struct picomesh_uint32_result mesh_store_resolve(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t service_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_resolve);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_resolve");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_uint32, _msg[0] ? strdup(_msg) : "mesh_store_resolve: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(uint32_t)) return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_uint32, "mesh_store_resolve: no impl on this class");
        return ((mesh_store_resolve_fn)fn)(ctx, obj, hdrs, service_id);
    }
}

struct picomesh_int_result mesh_store_forget(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t service_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_forget);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_forget: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_forget: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_forget: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_forget");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_forget: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_forget: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "mesh_store_forget: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_forget: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_forget: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_forget: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_forget: no impl on this class");
        return ((mesh_store_forget_fn)fn)(ctx, obj, hdrs, service_id);
    }
}

struct picomesh_size_result mesh_store_count_services(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_count_services);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_count_services");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "mesh_store_count_services: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "mesh_store_count_services: no impl on this class");
        return ((mesh_store_count_services_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_int_result mesh_store_spawn_picomesh(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t port)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_spawn_picomesh);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_spawn_picomesh");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(port) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: pack overflow");
        memcpy(_a + _off, &port, sizeof(port)); _off += sizeof(port);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_spawn_picomesh: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_spawn_picomesh: no impl on this class");
        return ((mesh_store_spawn_picomesh_fn)fn)(ctx, obj, hdrs, port);
    }
}

struct picomesh_int_result mesh_store_kill_pid(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, int32_t pid)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_kill_pid);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_kill_pid");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pid) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: pack overflow");
        memcpy(_a + _off, &pid, sizeof(pid)); _off += sizeof(pid);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_kill_pid: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_kill_pid: no impl on this class");
        return ((mesh_store_kill_pid_fn)fn)(ctx, obj, hdrs, pid);
    }
}

struct picomesh_size_result mesh_store_count_children(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_count_children);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_count_children");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "mesh_store_count_children: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "mesh_store_count_children: no impl on this class");
        return ((mesh_store_count_children_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_int_result mesh_store_reconcile_from_config(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_reconcile_from_config);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_reconcile_from_config");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_reconcile_from_config: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile_from_config: no impl on this class");
        return ((mesh_store_reconcile_from_config_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_int_result mesh_store_reconcile(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_reconcile);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.mesh_store_reconcile");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "mesh_store_reconcile: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "mesh_store_reconcile: no impl on this class");
        return ((mesh_store_reconcile_fn)fn)(ctx, obj, hdrs);
    }
}

