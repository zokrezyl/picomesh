/* GENERATED — do not edit. */
#include "git_repo.internal.h"
#include <picomesh/ycore/result.h>
#include <picomesh/ycore/ytrace.h>
#include <picomesh/ycore/yspan.h>
#include <picomesh/ycore/ytelemetry.h>
#include <picomesh/yclass/rpc.h>
#include <picomesh/yclass/yheaders.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct picomesh_uint32_result git_repo_store_make(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id, const char * owner_name, const char * repo_name)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_make);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_make");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(owner_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: pack overflow");
        memcpy(_a + _off, &owner_id, sizeof(owner_id)); _off += sizeof(owner_id);
        {
            uint32_t _slen = (uint32_t)(owner_name ? strlen(owner_name) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, owner_name, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(repo_name ? strlen(repo_name) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, repo_name, _slen); _off += _slen; }
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_uint32, _msg[0] ? strdup(_msg) : "git_repo_store_make: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(uint32_t)) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_make: no impl on this class");
        return ((git_repo_store_make_fn)fn)(ctx, obj, hdrs, owner_id, owner_name, repo_name);
    }
}

struct picomesh_int_result git_repo_store_delete(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_delete);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_delete");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "git_repo_store_delete: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "git_repo_store_delete: no impl on this class");
        return ((git_repo_store_delete_fn)fn)(ctx, obj, hdrs, repo_id);
    }
}

struct picomesh_uint32_result git_repo_store_owner_of(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_owner_of);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_owner_of");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_uint32, _msg[0] ? strdup(_msg) : "git_repo_store_owner_of: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(uint32_t)) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_uint32, "git_repo_store_owner_of: no impl on this class");
        return ((git_repo_store_owner_of_fn)fn)(ctx, obj, hdrs, repo_id);
    }
}

struct picomesh_size_result git_repo_store_count_for_owner(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_count_for_owner);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_count_for_owner");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(owner_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: pack overflow");
        memcpy(_a + _off, &owner_id, sizeof(owner_id)); _off += sizeof(owner_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "git_repo_store_count_for_owner: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_for_owner: no impl on this class");
        return ((git_repo_store_count_for_owner_fn)fn)(ctx, obj, hdrs, owner_id);
    }
}

struct picomesh_size_result git_repo_store_count_total(struct ctx * ctx, struct object * obj, struct yheaders * hdrs)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_count_total);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_count_total");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_size, _msg[0] ? strdup(_msg) : "git_repo_store_count_total: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(size_t)) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_size, "git_repo_store_count_total: no impl on this class");
        return ((git_repo_store_count_total_fn)fn)(ctx, obj, hdrs);
    }
}

struct picomesh_string_result git_repo_store_list_for_owner(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t owner_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_list_for_owner);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_list_for_owner");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(owner_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: pack overflow");
        memcpy(_a + _off, &owner_id, sizeof(owner_id)); _off += sizeof(owner_id);
        uint8_t _wbuf[4101];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "git_repo_store_list_for_owner: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "git_repo_store_list_for_owner: no impl on this class");
        return ((git_repo_store_list_for_owner_fn)fn)(ctx, obj, hdrs, owner_id);
    }
}

struct picomesh_string_result git_repo_store_read_tree(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * ref, const char * path)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_read_tree);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_read_tree");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        {
            uint32_t _slen = (uint32_t)(ref ? strlen(ref) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, ref, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(path ? strlen(path) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, path, _slen); _off += _slen; }
        }
        uint8_t _wbuf[4101];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "git_repo_store_read_tree: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_tree: no impl on this class");
        return ((git_repo_store_read_tree_fn)fn)(ctx, obj, hdrs, repo_id, ref, path);
    }
}

struct picomesh_string_result git_repo_store_read_file(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * ref, const char * path)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_read_file);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_read_file");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        {
            uint32_t _slen = (uint32_t)(ref ? strlen(ref) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, ref, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(path ? strlen(path) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, path, _slen); _off += _slen; }
        }
        uint8_t _wbuf[4101];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "git_repo_store_read_file: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "git_repo_store_read_file: no impl on this class");
        return ((git_repo_store_read_file_fn)fn)(ctx, obj, hdrs, repo_id, ref, path);
    }
}

struct picomesh_string_result git_repo_store_put_file(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, const char * path, const char * content, const char * message, const char * author_name, const char * author_email)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_put_file);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_put_file");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        {
            uint32_t _slen = (uint32_t)(path ? strlen(path) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, path, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(content ? strlen(content) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, content, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(message ? strlen(message) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, message, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(author_name ? strlen(author_name) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, author_name, _slen); _off += _slen; }
        }
        {
            uint32_t _slen = (uint32_t)(author_email ? strlen(author_email) : 0);
            if (_off + 4 + _slen > sizeof(_a))
                return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: pack overflow");
            memcpy(_a + _off, &_slen, 4); _off += 4;
            if (_slen) { memcpy(_a + _off, author_email, _slen); _off += _slen; }
        }
        uint8_t _wbuf[4101];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_string, _msg[0] ? strdup(_msg) : "git_repo_store_put_file: remote error (no msg)");
        }
        if (_wn < 5) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: truncated string response");
        uint32_t _slen;
        memcpy(&_slen, _wbuf + 1, 4);
        if (_wn < (size_t)5 + _slen) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: truncated string payload");
        char *_sv = malloc((size_t)_slen + 1);
        if (!_sv) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: out of memory");
        if (_slen) memcpy(_sv, _wbuf + 5, _slen);
        _sv[_slen] = 0;
        return PICOMESH_OK(picomesh_string, _sv);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_string, "git_repo_store_put_file: no impl on this class");
        return ((git_repo_store_put_file_fn)fn)(ctx, obj, hdrs, repo_id, path, content, message, author_name, author_email);
    }
}

struct picomesh_int_result git_repo_store_is_public(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_is_public);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_is_public");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "git_repo_store_is_public: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "git_repo_store_is_public: no impl on this class");
        return ((git_repo_store_is_public_fn)fn)(ctx, obj, hdrs, repo_id);
    }
}

struct picomesh_int_result git_repo_store_set_public(struct ctx * ctx, struct object * obj, struct yheaders * hdrs, uint32_t repo_id, int is_public)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("git_repo", (method_id_t)git_repo_store_set_public);
        if (PICOMESH_IS_ERR(_sr))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->peer) {
        uint32_t _rid = peer_channel_ensure_remote_id(_s->peer, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        /* Client span for this downstream call. Minted BEFORE the header
         * bag is serialized so the wire carries this span as the remote
         * peer's parent. */
        struct ytelemetry_span _tsp;
        ytelemetry_client_span_begin(&_tsp, hdrs, "rpc.git_repo_store_set_public");
        /* Headers section: the FRAMEWORK serializes the request-header
         * bag (uid, trace context, or anything a caller injected) ahead
         * of the packed business args. The skel parses it straight back
         * into the `hdrs` argument. ytelemetry_client_serialize_headers swaps in
         * this client span's id as parent_span_id across the serialize. */
        {
            size_t _hn = ytelemetry_client_serialize_headers(&_tsp, hdrs, _a, sizeof(_a));
            if (_hn == 0)
                return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: header serialize overflow");
            _off = _hn;
        }
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(repo_id) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: pack overflow");
        memcpy(_a + _off, &repo_id, sizeof(repo_id)); _off += sizeof(repo_id);
        if (_off + sizeof(is_public) > sizeof(_a))
            return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: pack overflow");
        memcpy(_a + _off, &is_public, sizeof(is_public)); _off += sizeof(is_public);
        uint8_t _wbuf[261];
        size_t _wn = rpc_call(_s->peer, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        ytelemetry_span_end(&_tsp, _wn >= 1 && _wbuf[0] == 0, NULL);
        if (_wn < 1) return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: short RPC response");
        if (_wbuf[0] != 0) {
            uint32_t _msg_len = 0;
            if (_wn >= 5) memcpy(&_msg_len, _wbuf + 1, 4);
            char _msg[260];
            size_t _copy = _msg_len < sizeof(_msg) - 1 ? _msg_len : sizeof(_msg) - 1;
            if (_wn >= 5 + _copy) memcpy(_msg, _wbuf + 5, _copy);
            _msg[_copy] = 0;
            return PICOMESH_ERR(picomesh_int, _msg[0] ? strdup(_msg) : "git_repo_store_set_public: remote error (no msg)");
        }
        if (_wn != 1 + sizeof(int)) return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return PICOMESH_OK(picomesh_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return PICOMESH_ERR(picomesh_int, "git_repo_store_set_public: no impl on this class");
        return ((git_repo_store_set_public_fn)fn)(ctx, obj, hdrs, repo_id, is_public);
    }
}

