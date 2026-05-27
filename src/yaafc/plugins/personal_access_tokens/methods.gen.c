/* GENERATED — do not edit. */
#include <yaafc/personal_access_tokens/methods.gen.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/rpc.h>
#include <stdint.h>
#include <string.h>

struct yaafc_uint32_result personal_access_tokens_store_mint(struct ctx * ctx, struct object * obj, uint32_t user_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_mint);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        uint8_t _wbuf[1 + sizeof(uint32_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_mint: no impl on this class");
        return ((personal_access_tokens_store_mint_fn)fn)(ctx, obj, user_id);
    }
}

struct yaafc_uint32_result personal_access_tokens_store_lookup(struct ctx * ctx, struct object * obj, uint32_t pat_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_lookup);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pat_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: pack overflow");
        memcpy(_a + _off, &pat_id, sizeof(pat_id)); _off += sizeof(pat_id);
        uint8_t _wbuf[1 + sizeof(uint32_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_uint32, "personal_access_tokens_store_lookup: no impl on this class");
        return ((personal_access_tokens_store_lookup_fn)fn)(ctx, obj, pat_id);
    }
}

struct yaafc_int_result personal_access_tokens_store_revoke(struct ctx * ctx, struct object * obj, uint32_t pat_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_revoke);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pat_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: pack overflow");
        memcpy(_a + _off, &pat_id, sizeof(pat_id)); _off += sizeof(pat_id);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "personal_access_tokens_store_revoke: no impl on this class");
        return ((personal_access_tokens_store_revoke_fn)fn)(ctx, obj, pat_id);
    }
}

struct yaafc_size_result personal_access_tokens_store_list_for_user(struct ctx * ctx, struct object * obj, uint32_t user_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_list_for_user);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_list_for_user: no impl on this class");
        return ((personal_access_tokens_store_list_for_user_fn)fn)(ctx, obj, user_id);
    }
}

struct yaafc_size_result personal_access_tokens_store_count_active(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("personal_access_tokens", (method_id_t)personal_access_tokens_store_count_active);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "personal_access_tokens_store_count_active: no impl on this class");
        return ((personal_access_tokens_store_count_active_fn)fn)(ctx, obj);
    }
}

