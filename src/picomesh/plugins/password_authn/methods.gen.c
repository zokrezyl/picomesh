/* GENERATED — do not edit. */
#include "password_authn.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_int_result password_authn_store_register(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_register);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_register: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "password_authn_store_register: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "password_authn_store_register: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.password_authn_store_register");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "password_authn_store_register: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "password_authn_store_register: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_register: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_register: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "password_authn_store_register: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "password_authn_store_register: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "password_authn_store_register: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "password_authn_store_register: no impl on this class");
        return ((password_authn_store_register_fn)fn)(ctx, obj, hdrs, user_id, hash);
    }
}

struct picomesh_int_result password_authn_store_authenticate(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_authenticate);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.password_authn_store_authenticate");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "password_authn_store_authenticate: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "password_authn_store_authenticate: no impl on this class");
        return ((password_authn_store_authenticate_fn)fn)(ctx, obj, hdrs, user_id, hash);
    }
}

struct picomesh_int_result password_authn_store_change_password(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_change_password);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.password_authn_store_change_password");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "password_authn_store_change_password: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "password_authn_store_change_password: no impl on this class");
        return ((password_authn_store_change_password_fn)fn)(ctx, obj, hdrs, user_id, hash);
    }
}

struct picomesh_size_result password_authn_store_count_registered(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_count_registered);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.password_authn_store_count_registered");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "password_authn_store_count_registered: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "password_authn_store_count_registered: no impl on this class");
        return ((password_authn_store_count_registered_fn)fn)(ctx, obj, hdrs);
    }
}

