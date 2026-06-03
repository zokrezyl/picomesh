#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["msgpack>=1.0"]
# ///
"""Reference client for the picomesh `msgpack` frontend.

Speaks the Picomesh MessagePack envelope: a big-endian u32 length prefix
followed by a msgpack map, strict serial request/response. This is the
hand-written reference the generated polyglot clients (Part 3) mirror.

Wire:
    request  -> u32 BE len | msgpack({v,op,path,args,kwargs,headers})
    response <- u32 BE len | msgpack({v,ok, result|error})

CLI:
    picomesh_msgpack.py invoke   HOST PORT PATH [ARGS_JSON] [--kwargs JSON] [--headers JSON]
    picomesh_msgpack.py describe HOST PORT PATH
    picomesh_msgpack.py oversize HOST PORT          # send an over-cap frame
"""
import argparse
import json
import socket
import struct
import sys

import msgpack

FRAME_CAP = 1 << 20


def _recv_exact(sock: socket.socket, n: int) -> bytes:
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError(f"peer closed after {len(buf)}/{n} bytes")
        buf += chunk
    return bytes(buf)


def _round_trip(host: str, port: int, payload: bytes) -> dict:
    frame = struct.pack(">I", len(payload)) + payload
    with socket.create_connection((host, port), timeout=5) as sock:
        sock.sendall(frame)
        (resp_len,) = struct.unpack(">I", _recv_exact(sock, 4))
        body = _recv_exact(sock, resp_len)
    return msgpack.unpackb(body, raw=False)


def call(host: str, port: int, op: str, path: str,
         args=None, kwargs=None, headers=None) -> dict:
    env = {
        "v": 1,
        "op": op,
        "path": path,
        "args": args if args is not None else [],
        "kwargs": kwargs if kwargs is not None else {},
        "headers": headers if headers is not None else {},
    }
    return _round_trip(host, port, msgpack.packb(env, use_bin_type=True))


def oversize(host: str, port: int) -> dict:
    """Send a frame whose declared length exceeds the server cap; the server
    must answer `frame_too_large` and close."""
    payload = msgpack.packb({"v": 1, "op": "invoke", "path": "x.y.z",
                             "args": [], "kwargs": {}, "headers": {}},
                            use_bin_type=True)
    frame = struct.pack(">I", FRAME_CAP + 1) + payload
    with socket.create_connection((host, port), timeout=5) as sock:
        sock.sendall(frame)
        (resp_len,) = struct.unpack(">I", _recv_exact(sock, 4))
        return msgpack.unpackb(_recv_exact(sock, resp_len), raw=False)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("op", choices=["invoke", "describe", "oversize"])
    ap.add_argument("host")
    ap.add_argument("port", type=int)
    ap.add_argument("path", nargs="?", default="")
    ap.add_argument("args", nargs="?", default="[]", help="positional args as JSON array")
    ap.add_argument("--kwargs", default="{}")
    ap.add_argument("--headers", default="{}")
    a = ap.parse_args()

    if a.op == "oversize":
        resp = oversize(a.host, a.port)
    elif a.op == "describe":
        resp = call(a.host, a.port, "describe", a.path)
    else:
        resp = call(a.host, a.port, "invoke", a.path,
                    args=json.loads(a.args),
                    kwargs=json.loads(a.kwargs),
                    headers=json.loads(a.headers))
    json.dump(resp, sys.stdout)
    sys.stdout.write("\n")
    return 0 if resp.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
