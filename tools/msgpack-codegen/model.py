"""Shared IR for the polyglot msgpack client generators.

Loads the codegen `model.yaml` files (the per-plugin class/method model that
also drives the C codegen) and normalizes each method to a language-neutral
shape the per-language emitters consume — no clang, no C parsing.
"""
import re

import yaml


def arg_kind(ctype: str) -> str:
    """C arg type -> neutral kind: 'int' | 'uint' | 'float' | 'str'."""
    t = ctype.strip()
    if t == "char *" or re.match(r"^\s*const\s+char\s*\*\s*$", t):
        return "str"
    if t in ("double", "float"):
        return "float"
    if t in ("uint8_t", "uint16_t", "uint32_t", "uint64_t", "size_t"):
        return "uint"
    return "int"


def _result_id(return_type: str) -> str:
    m = re.match(r"^struct\s+(\w+)_result\s*$", return_type.strip())
    return m.group(1) if m else return_type.strip()


def ret_kind(return_type: str) -> str:
    """Return-type -> neutral kind: 'int'|'uint'|'float'|'str'|'void'|'any'."""
    rid = _result_id(return_type)
    if rid == "picomesh_void":
        return "void"
    if rid in ("picomesh_int", "picomesh_int64"):
        return "int"
    if rid in ("picomesh_uint32", "picomesh_size"):
        return "uint"
    if rid in ("picomesh_string", "picomesh_json"):
        return "str"
    return "any"


def method_short_name(m: dict) -> str:
    """The method segment, slot minus its `<owning_class>_` prefix."""
    cls = m.get("owning_class") or ""
    slot = m["slot"]
    prefix = cls + "_"
    return slot[len(prefix):] if (cls and slot.startswith(prefix)) else slot


def dotted_path(m: dict) -> str:
    """Dotted `service.class.method` path the frontend resolves."""
    domain = m["domain"]
    cls = m.get("owning_class") or ""
    if not cls:
        return f"{domain}.{m['slot']}"
    return f"{domain}.{cls}.{method_short_name(m)}"


def load(paths) -> list:
    """Return a flat list of normalized methods across the given model.yaml
    paths. Each entry: {domain, owning_class, slot, name, path, args:[{name,
    kind, ctype}], ret_kind}."""
    out = []
    for path in paths:
        with open(path) as fh:
            data = yaml.safe_load(fh)
        if not data:
            continue
        for m in data.get("methods", []):
            user = m["args"][3:]  # drop framework ctx/obj/hdrs
            out.append({
                "domain": m["domain"],
                "owning_class": m.get("owning_class") or "",
                "slot": m["slot"],
                "name": method_short_name(m),
                "path": dotted_path(m),
                "args": [
                    {"name": a["name"], "kind": arg_kind(a["type"]), "ctype": a["type"]}
                    for a in user
                ],
                "ret_kind": ret_kind(m["return_type"]),
            })
    return out
