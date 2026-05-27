/* GENERATED — do not edit. */
#include <yaafc/password_authn/methods.gen.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/rpc.h>
#include <stdint.h>
#include <string.h>

struct yaafc_int_result password_authn_store_register(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_register);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "password_authn_store_register: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "password_authn_store_register: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "password_authn_store_register: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "password_authn_store_register: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_register: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_register: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "password_authn_store_register: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "password_authn_store_register: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "password_authn_store_register: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "password_authn_store_register: no impl on this class");
        return ((password_authn_store_register_fn)fn)(ctx, obj, user_id, hash);
    }
}

struct yaafc_int_result password_authn_store_authenticate(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_authenticate);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "password_authn_store_authenticate: no impl on this class");
        return ((password_authn_store_authenticate_fn)fn)(ctx, obj, user_id, hash);
    }
}

struct yaafc_int_result password_authn_store_change_password(struct ctx * ctx, struct object * obj, uint32_t user_id, int64_t hash)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_change_password);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(user_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: pack overflow");
        memcpy(_a + _off, &user_id, sizeof(user_id)); _off += sizeof(user_id);
        if (_off + sizeof(hash) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: pack overflow");
        memcpy(_a + _off, &hash, sizeof(hash)); _off += sizeof(hash);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "password_authn_store_change_password: no impl on this class");
        return ((password_authn_store_change_password_fn)fn)(ctx, obj, user_id, hash);
    }
}

struct yaafc_size_result password_authn_store_count_registered(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("password_authn", (method_id_t)password_authn_store_count_registered);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "password_authn_store_count_registered: no impl on this class");
        return ((password_authn_store_count_registered_fn)fn)(ctx, obj);
    }
}

