/* GENERATED — do not edit. */
#include <yaafc/mesh/methods.gen.h>
#include <yaafc/ycore/result.h>
#include <yaafc/ycore/ytrace.h>
#include <yaafc/yclass/rpc.h>
#include <stdint.h>
#include <string.h>

struct yaafc_int_result mesh_store_register_service(struct ctx * ctx, struct object * obj, uint32_t service_id, uint32_t port)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_register_service);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_register_service: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_register_service: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_register_service: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_register_service: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "mesh_store_register_service: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        if (_off + sizeof(port) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "mesh_store_register_service: pack overflow");
        memcpy(_a + _off, &port, sizeof(port)); _off += sizeof(port);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_register_service: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_register_service: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_register_service: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_register_service: no impl on this class");
        return ((mesh_store_register_service_fn)fn)(ctx, obj, service_id, port);
    }
}

struct yaafc_uint32_result mesh_store_resolve(struct ctx * ctx, struct object * obj, uint32_t service_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_resolve);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        uint8_t _wbuf[1 + sizeof(uint32_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: truncated RPC payload");
        uint32_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_uint32, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_uint32, "mesh_store_resolve: no impl on this class");
        return ((mesh_store_resolve_fn)fn)(ctx, obj, service_id);
    }
}

struct yaafc_int_result mesh_store_forget(struct ctx * ctx, struct object * obj, uint32_t service_id)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_forget);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_forget: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_forget: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_forget: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_forget: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(service_id) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "mesh_store_forget: pack overflow");
        memcpy(_a + _off, &service_id, sizeof(service_id)); _off += sizeof(service_id);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_forget: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_forget: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_forget: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_forget: no impl on this class");
        return ((mesh_store_forget_fn)fn)(ctx, obj, service_id);
    }
}

struct yaafc_size_result mesh_store_count_services(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_count_services);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "mesh_store_count_services: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "mesh_store_count_services: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "mesh_store_count_services: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "mesh_store_count_services: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "mesh_store_count_services: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "mesh_store_count_services: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "mesh_store_count_services: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "mesh_store_count_services: no impl on this class");
        return ((mesh_store_count_services_fn)fn)(ctx, obj);
    }
}

struct yaafc_int_result mesh_store_spawn_yaafc(struct ctx * ctx, struct object * obj, uint32_t port)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_spawn_yaafc);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(port) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: pack overflow");
        memcpy(_a + _off, &port, sizeof(port)); _off += sizeof(port);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_spawn_yaafc: no impl on this class");
        return ((mesh_store_spawn_yaafc_fn)fn)(ctx, obj, port);
    }
}

struct yaafc_int_result mesh_store_kill_pid(struct ctx * ctx, struct object * obj, int32_t pid)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_kill_pid);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        if (_off + sizeof(pid) > sizeof(_a))
            return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: pack overflow");
        memcpy(_a + _off, &pid, sizeof(pid)); _off += sizeof(pid);
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_kill_pid: no impl on this class");
        return ((mesh_store_kill_pid_fn)fn)(ctx, obj, pid);
    }
}

struct yaafc_size_result mesh_store_count_children(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_count_children);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_size, "mesh_store_count_children: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_size, "mesh_store_count_children: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_size, "mesh_store_count_children: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_size, "mesh_store_count_children: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(size_t)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_size, "mesh_store_count_children: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_size, "mesh_store_count_children: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_size, "mesh_store_count_children: truncated RPC payload");
        size_t _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_size, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_size, "mesh_store_count_children: no impl on this class");
        return ((mesh_store_count_children_fn)fn)(ctx, obj);
    }
}

struct yaafc_int_result mesh_store_reconcile_from_config(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_reconcile_from_config);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile_from_config: no impl on this class");
        return ((mesh_store_reconcile_from_config_fn)fn)(ctx, obj);
    }
}

struct yaafc_int_result mesh_store_reconcile(struct ctx * ctx, struct object * obj)
{
    static method_slot _slot = METHOD_SLOT_UNDEFINED;
    if (_slot == METHOD_SLOT_UNDEFINED) {
        struct method_slot_result _sr =
            method_slot_get("mesh", (method_id_t)mesh_store_reconcile);
        if (YAAFC_IS_ERR(_sr))
            return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: method_slot_get failed", _sr);
        _slot = _sr.value;
    }

    if (!obj) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: NULL object");

    struct ctx *_s = ctx;
    if (_s && _s->session) {
        uint32_t _rid = rpc_session_ensure_remote_id(_s->session, _slot);
        if (_rid == RPC_REMOTE_ID_UNRESOLVED)
            return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: remote id unresolved");
        uint8_t _a[16384];
        size_t _off = 0;
        {
            uint64_t _h = *(uint64_t *)((char *)obj + sizeof(*obj));
            if (_off + 8 > sizeof(_a))
                return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: pack overflow");
            memcpy(_a + _off, &_h, 8); _off += 8;
        }
        uint8_t _wbuf[1 + sizeof(int)];
        size_t _wn = rpc_call(_s->session, RPC_OP_CALL, _rid, _a, _off,
                              _wbuf, sizeof(_wbuf));
        if (_wn < 1) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: short RPC response");
        if (_wbuf[0] != 0) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: remote impl returned error");
        if (_wn != sizeof(_wbuf)) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: truncated RPC payload");
        int _v;
        memcpy(&_v, _wbuf + 1, sizeof(_v));
        return YAAFC_OK(yaafc_int, _v);
    } else {
        impl_t fn = class_dispatch_lookup(object_class(obj), _slot);
        if (!fn) return YAAFC_ERR(yaafc_int, "mesh_store_reconcile: no impl on this class");
        return ((mesh_store_reconcile_fn)fn)(ctx, obj);
    }
}

