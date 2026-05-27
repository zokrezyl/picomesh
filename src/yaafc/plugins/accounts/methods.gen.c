/* GENERATED — do not edit. */
#include <yaafc/accounts/methods.gen.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/rpc.h>
#include <stdint.h>
#include <string.h>

struct yaafc_int_result accounts_store_register(struct ctx * ctx, struct object * obj, uint32_t uid)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("accounts", (method_id_t)accounts_store_register);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "accounts_store_register: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "accounts_store_register: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "accounts_store_register: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "accounts_store_register: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(uid) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "accounts_store_register: pack overflow");
        memcpy(_a + _off, &uid, sizeof(uid)); _off += sizeof(uid);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "accounts_store_register: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "accounts_store_register: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "accounts_store_register: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "accounts_store_register: no impl on this class");
        return ((accounts_store_register_fn)fn)(ctx, obj, uid);
    }
}

struct yaafc_int_result accounts_store_exists(struct ctx * ctx, struct object * obj, uint32_t uid)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("accounts", (method_id_t)accounts_store_exists);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "accounts_store_exists: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "accounts_store_exists: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "accounts_store_exists: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "accounts_store_exists: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(uid) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "accounts_store_exists: pack overflow");
        memcpy(_a + _off, &uid, sizeof(uid)); _off += sizeof(uid);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "accounts_store_exists: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "accounts_store_exists: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "accounts_store_exists: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "accounts_store_exists: no impl on this class");
        return ((accounts_store_exists_fn)fn)(ctx, obj, uid);
    }
}

struct yaafc_int_result accounts_store_set_balance(struct ctx * ctx, struct object * obj, uint32_t uid, int64_t n)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("accounts", (method_id_t)accounts_store_set_balance);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(uid) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: pack overflow");
        memcpy(_a + _off, &uid, sizeof(uid)); _off += sizeof(uid);
        if (_off + sizeof(n) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: pack overflow");
        memcpy(_a + _off, &n, sizeof(n)); _off += sizeof(n);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "accounts_store_set_balance: no impl on this class");
        return ((accounts_store_set_balance_fn)fn)(ctx, obj, uid, n);
    }
}

struct yaafc_int64_result accounts_store_balance(struct ctx * ctx, struct object * obj, uint32_t uid)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("accounts", (method_id_t)accounts_store_balance);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int64, "accounts_store_balance: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int64, "accounts_store_balance: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int64, "accounts_store_balance: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int64, "accounts_store_balance: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(uid) > sizeof(_a))
            return YAAFC_ERR(yaafc_int64, "accounts_store_balance: pack overflow");
        memcpy(_a + _off, &uid, sizeof(uid)); _off += sizeof(uid);
        uint8_t _wbuf[1 + sizeof(int64_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int64, "accounts_store_balance: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int64, "accounts_store_balance: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int64, "accounts_store_balance: truncated RPC payload");
        int64_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int64, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int64, "accounts_store_balance: no impl on this class");
        return ((accounts_store_balance_fn)fn)(ctx, obj, uid);
    }
}

struct yaafc_size_result accounts_store_count(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("accounts", (method_id_t)accounts_store_count);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "accounts_store_count: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "accounts_store_count: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "accounts_store_count: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "accounts_store_count: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "accounts_store_count: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "accounts_store_count: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "accounts_store_count: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "accounts_store_count: no impl on this class");
        return ((accounts_store_count_fn)fn)(ctx, obj);
    }
}

