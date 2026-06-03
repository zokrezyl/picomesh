#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["msgpack>=1.0"]
# ///
"""A minimal FOREIGN msgpack service implementing the picomesh calculator
contract — proves picomesh C can call OUT to a non-picomesh msgpack service.

Speaks the Picomesh MessagePack envelope (u32 BE length frame + msgpack map):
implements calculator.calc.{add,sub,mul,div} over positional args, strict
serial request/response.
"""
import socket
import struct
import sys

import msgpack

HANDLERS = {
    "calculator.calc.add": lambda a: a[0] + a[1],
    "calculator.calc.sub": lambda a: a[0] - a[1],
    "calculator.calc.mul": lambda a: a[0] * a[1],
    "calculator.calc.div": lambda a: (a[0] // a[1]) if a[1] else 0,
}


def _recv_exact(conn: socket.socket, n: int):
    buf = bytearray()
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            return None
        buf += chunk
    return bytes(buf)


def handle(env: dict) -> dict:
    if env.get("op", "invoke") != "invoke":
        return {"v": 1, "ok": False,
                "error": {"message": f"unsupported op {env.get('op')}", "code": "bad_op"}}
    fn = HANDLERS.get(env.get("path", ""))
    if not fn:
        return {"v": 1, "ok": False,
                "error": {"message": f"no method {env.get('path')}", "code": "no_such_method"}}
    try:
        return {"v": 1, "ok": True, "result": fn(env.get("args", []))}
    except Exception as exc:  # noqa: BLE001 — reflect any handler failure to the client
        return {"v": 1, "ok": False, "error": {"message": str(exc), "code": "call_error"}}


def serve(host: str, port: int) -> None:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host, port))
    srv.listen(8)
    sys.stderr.write(f"echo_server: listening on {host}:{port}\n")
    sys.stderr.flush()
    while True:
        conn, _ = srv.accept()
        with conn:
            while True:
                hdr = _recv_exact(conn, 4)
                if not hdr:
                    break
                (n,) = struct.unpack(">I", hdr)
                body = _recv_exact(conn, n)
                if body is None:
                    break
                resp = msgpack.packb(handle(msgpack.unpackb(body, raw=False)), use_bin_type=True)
                conn.sendall(struct.pack(">I", len(resp)) + resp)


if __name__ == "__main__":
    serve(sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1",
          int(sys.argv[2]) if len(sys.argv) > 2 else 7912)
