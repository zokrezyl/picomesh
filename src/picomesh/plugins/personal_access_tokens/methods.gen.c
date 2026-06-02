/* GENERATED — do not edit. */
#include "personal_access_tokens.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_uint32_result personal_access_tokens_store_mint(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_mint);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.personal_access_tokens_store_mint");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_mint: header serialize overflow");
                return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_mint: pack overflow"); return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_mint: pack overflow"); return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: pack overflow"); }
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_uint32, _msg[0] ? strdup(_msg) : "personal_access_tokens_store_mint: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(uint32_t)) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_mint: no impl on this class");
        return ((personal_access_tokens_store_mint_fn)fn)(ctx, obj, hdrs, user_id);
    }
}

struct picomesh_uint32_result personal_access_tokens_store_lookup(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t pat_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_lookup);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.personal_access_tokens_store_lookup");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_lookup: header serialize overflow");
                return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_lookup: pack overflow"); return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pat_id) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_lookup: pack overflow"); return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: pack overflow"); }
        memcpy(_a + _off, &pat_id, sizeof(pat_id)); _off += sizeof(pat_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_uint32, _msg[0] ? strdup(_msg) : "personal_access_tokens_store_lookup: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(uint32_t)) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_uint32, "personal_access_tokens_store_lookup: no impl on this class");
        return ((personal_access_tokens_store_lookup_fn)fn)(ctx, obj, hdrs, pat_id);
    }
}

struct picomesh_int_result personal_access_tokens_store_revoke(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t pat_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_revoke);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.personal_access_tokens_store_revoke");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_revoke: header serialize overflow");
                return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_revoke: pack overflow"); return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pat_id) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_revoke: pack overflow"); return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: pack overflow"); }
        memcpy(_a + _off, &pat_id, sizeof(pat_id)); _off += sizeof(pat_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "personal_access_tokens_store_revoke: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "personal_access_tokens_store_revoke: no impl on this class");
        return ((personal_access_tokens_store_revoke_fn)fn)(ctx, obj, hdrs, pat_id);
    }
}

struct picomesh_size_result personal_access_tokens_store_list_for_user(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t user_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_list_for_user);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.personal_access_tokens_store_list_for_user");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_list_for_user: header serialize overflow");
                return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_list_for_user: pack overflow"); return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_list_for_user: pack overflow"); return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: pack overflow"); }
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "personal_access_tokens_store_list_for_user: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_list_for_user: no impl on this class");
        return ((personal_access_tokens_store_list_for_user_fn)fn)(ctx, obj, hdrs, user_id);
    }
}

struct picomesh_size_result personal_access_tokens_store_count_active(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_count_active);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.personal_access_tokens_store_count_active");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0) {
                ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_count_active: header serialize overflow");
                return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: header serialize overflow");
            }
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                { ytelemetry_span_end(&_tsp, 0, "personal_access_tokens_store_count_active: pack overflow"); return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: pack overflow"); }
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "personal_access_tokens_store_count_active: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "personal_access_tokens_store_count_active: no impl on this class");
        return ((personal_access_tokens_store_count_active_fn)fn)(ctx, obj, hdrs);
    }
}

