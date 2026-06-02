# SPDX-License-Identifier: MIT
"""Binary blob emitter for compiled ARB models (.zrmb format).

Layout:
  [ZRMB header — 84 bytes]
  [Section table — N * 8 bytes]
  [Section data …]

Header (84 bytes):
  magic        4B  "ZRMB"
  version      2B  uint16 LE (currently 1)
  flags        2B  uint16 LE (bits 0-7: major, 8-15: minor packed version)
  header_len   4B  uint32 LE (always 84)
  total_len    4B  uint32 LE
  model_hash  32B  SHA-256
  schema_hash 32B  SHA-256
  crc32        4B  uint32 LE — CRC-32 over everything except this field

Section table entry (8 bytes):
  type       1B  uint8
  pad        1B  0
  offset     2B  uint16 LE — byte offset from start of blob
  count      2B  uint16 LE — element count
  elem_size  2B  uint16 LE — size of one element in bytes
"""

from __future__ import annotations

import hashlib
import hmac
import struct
import zlib
from typing import Any

from .canonical import CanonicalModel

# Flag bit indicating the blob carries an HMAC-SHA256 signature.
BLOB_FLAG_SIGNED = 1 << 0

# Section type constants
SECTION_FACTS       = 1
SECTION_RULES       = 2
SECTION_CONDITIONS  = 3
SECTION_EXPRESSIONS = 4
SECTION_ACTIONS     = 5
SECTION_STRINGS     = 6
SECTION_MODES       = 7

_HEADER_LEN = 84
_SECTION_ENTRY_SIZE = 8
_BLOB_VERSION = 1

# Wire sizes for packed structs (all little-endian, uint16 indices)
_FACT_ELEM_SIZE = 16   # id(2) + type(1) + pad(1) + range_min(4) + range_max(4) + default(4) + stale(2) + safety(1) + pad(1) => rearranged below
_RULE_ELEM_SIZE = 22
_COND_ELEM_SIZE = 8
_EXPR_ELEM_SIZE = 20
_ACTION_ELEM_SIZE = 12

# ── Operator / type maps matching arbiter_model.h enums ──────────────

_TYPE_MAP = {"bool": 0, "int32": 1, "uint32": 2, "enum": 3}
_CLASS_MAP = {
    "inference": 0, "constraint": 1, "mode_guard": 2,
    "safety_guard": 3, "obligation": 4, "advisory": 5,
}
_OP_MAP = {
    "==": 0, "!=": 1, "<": 2, "<=": 3, ">": 4, ">=": 5,
    "in": 6, "not_in": 7, "stale": 8, "not_stale": 9,
    "changed": 10, "delta_gt": 11, "delta_lt": 12,
}
_COND_GROUP_MAP = {"all": 0, "any": 1, "not": 2}
_EXPR_OP_MAP = {
    "add": 0, "sub": 1, "mul": 2, "div": 3, "mod": 4,
    "abs": 5, "negate": 6, "min": 7, "max": 8, "clamp": 9,
    "shift_r": 10, "shift_l": 11, "scale": 12, "assign": 13,
    "accumulate": 14,
}
_ACTION_TYPE_MAP = {
    "callback": 0, "log": 1, "notify": 2, "set_fact": 3,
    "set_mode": 4, "raise_fault": 5, "clear_fault": 6,
}


def _pack_hash(hex_str: str) -> bytes:
    """Convert a 64-char hex hash to 32 bytes, zero-padded if short."""
    h = hex_str[:64].ljust(64, "0")
    return bytes.fromhex(h)


def _encode_version_flags(model: CanonicalModel) -> int:
    """Encode model version into the 16-bit flags field.

    Bits 0-7: major version, bits 8-15: minor version.
    If the model has no version, returns 0.
    """
    version = getattr(model, "version", None)
    if version and isinstance(version, str):
        parts = version.split(".")
        try:
            major = int(parts[0]) & 0xFF
            minor = int(parts[1]) & 0xFF if len(parts) > 1 else 0
            return (minor << 8) | major
        except (ValueError, IndexError):
            pass
    return 0


def _pack_facts(model: CanonicalModel) -> bytes:
    """Pack fact definitions.

    Wire layout per fact (16 bytes):
      id:            uint16 LE
      type:          uint8
      safety_rel:    uint8 (bool)
      range_min:     int32 LE
      range_max:     int32 LE
      default_value: int32 LE
    """
    buf = bytearray()
    for i, f in enumerate(model.facts):
        rng = f.get("range", [0, 0])
        rmin = rng[0] if isinstance(rng, list) and len(rng) >= 2 else 0
        rmax = rng[1] if isinstance(rng, list) and len(rng) >= 2 else 0
        fact_type = _TYPE_MAP.get(f.get("type", "bool"), 0)
        safety = 1 if f.get("safety_relevant", False) else 0
        default_val = int(f.get("default", 0))
        buf += struct.pack("<HBBiii", i, fact_type, safety, rmin, rmax, default_val)
    return bytes(buf)


def _pack_rules(model: CanonicalModel) -> bytes:
    """Pack rule definitions.

    Wire layout per rule (22 bytes):
      id:              uint16 LE
      rule_class:      uint8
      safety_critical: uint8
      cond_start:      uint16 LE
      cond_count:      uint16 LE
      action_start:    uint16 LE
      action_count:    uint16 LE
      expr_start:      uint16 LE
      expr_count:      uint16 LE
      safety_goal_id:  uint16 LE
      set_mode:        uint16 LE
      required_mode:   uint16 LE
    """
    buf = bytearray()
    cond_offset = 0
    for i, r in enumerate(model.rules):
        rclass = _CLASS_MAP.get(r.get("class", "inference"), 0)
        then = r.get("then", {})

        set_mode = 0xFFFF
        if isinstance(then, dict) and "set_mode" in then:
            mid = model.mode_id_map.get(then["set_mode"])
            if mid is not None:
                set_mode = mid

        when = r.get("when", {})
        cond_count = 0
        if isinstance(when, dict):
            for gk in ("all", "any", "not"):
                g = when.get(gk)
                if isinstance(g, list):
                    cond_count += len(g)

        action_start = 0
        action_count = 0
        if isinstance(then, dict) and "action" in then:
            aid = model.action_id_map.get(then["action"])
            if aid is not None:
                action_start = aid
                action_count = 1

        safety_critical = 1 if (
            isinstance(then, dict) and then.get("criticality") == "safety_critical"
        ) else 0

        expr_start = r.get("_expr_start", 0)
        expr_count = r.get("_expr_count", 0)

        # required_mode: 0xFFFF means any mode
        required_mode = 0xFFFF

        buf += struct.pack(
            "<HBBHHHHHHHHH",
            i, rclass, safety_critical,
            cond_offset, cond_count,
            action_start, action_count,
            expr_start, expr_count,
            0xFFFF,  # safety_goal_id
            set_mode,
            required_mode,
        )
        cond_offset += cond_count
    return bytes(buf)


def _pack_conditions(model: CanonicalModel) -> bytes:
    """Pack condition definitions.

    Wire layout per condition (8 bytes):
      fact_id:     uint16 LE
      op:          uint8
      group:       uint8
      value:       int32 LE
    """
    buf = bytearray()
    for c in model.conditions:
        fact_id = c.get("fact_id", 0)
        op = _OP_MAP.get(c.get("op", "=="), 0)
        group = _COND_GROUP_MAP.get(c.get("group", "all"), 0)
        val = c.get("value", 0)
        if isinstance(val, bool):
            val = 1 if val else 0
        buf += struct.pack("<HBBi", fact_id, op, group, int(val))
    return bytes(buf)


def _pack_expressions(model: CanonicalModel) -> bytes:
    """Pack expression definitions.

    Wire layout per expression (20 bytes):
      target_fact_id: uint16 LE
      op:             uint8
      pad:            uint8
      left_fact_id:   uint16 LE
      left_literal:   int32 LE
      right_fact_id:  uint16 LE
      pad2:           uint16 LE
      right_literal:  int32 LE
      scale:          int32 LE
    """
    buf = bytearray()
    # Recalculate: we need 20 bytes. Let's lay it out more carefully.
    # target(2) + op(1) + pad(1) + left_fact(2) + pad(2) + left_lit(4) + right_fact(2) + pad(2) + right_lit(4) = 20
    # Actually let's use a cleaner layout:
    # target(2) + op(1) + pad(1) + left_fact(2) + right_fact(2) + left_lit(4) + right_lit(4) + scale(4) = 20
    for e in model.expressions:
        target = e.get("target_fact_id", 0)
        op = _EXPR_OP_MAP.get(e.get("op", "assign"), 13)
        left_fact = e.get("left_fact_id", 0xFFFF)
        right_fact = e.get("right_fact_id", 0xFFFF)
        left_lit = e.get("left_literal", 0)
        right_lit = e.get("right_literal", 0)
        scale = e.get("scale", 1)
        buf += struct.pack(
            "<HBBHHiii",
            target, op, 0,
            left_fact, right_fact,
            left_lit, right_lit, scale,
        )
    return bytes(buf)


def _pack_actions(model: CanonicalModel) -> bytes:
    """Pack action definitions.

    Wire layout per action (12 bytes):
      id:                      uint16 LE
      type:                    uint8
      safe_state_action:       uint8
      target_fact_id:          uint16 LE
      must_complete_within_ms: uint16 LE
      target_value:            int32 LE
    """
    buf = bytearray()
    for i, a in enumerate(model.actions):
        atype = _ACTION_TYPE_MAP.get(a.get("type", "callback"), 0)
        safe_state = 1 if a.get("safe_state_action", False) else 0
        must_complete = int(a.get("must_complete_within_ms", 0)) & 0xFFFF
        buf += struct.pack("<HBBHHi", i, atype, safe_state, 0, must_complete, 0)
    return bytes(buf)


def _pack_strings(model: CanonicalModel) -> tuple[bytes, list[int]]:
    """Pack all name strings into a string table section.

    Returns (string_blob, offsets_within_blob).
    Strings are null-terminated UTF-8.
    """
    buf = bytearray()
    offsets: list[int] = []
    seen: dict[str, int] = {}

    def _add(s: str | None) -> int:
        nonlocal buf
        if s is None:
            return 0xFFFF
        if s in seen:
            return seen[s]
        off = len(buf)
        seen[s] = off
        buf += s.encode("utf-8") + b"\x00"
        offsets.append(off)
        return off

    # Model name
    _add(model.name)

    # Fact names
    for f in model.facts:
        _add(f.get("id"))

    # Rule names and explanations
    for r in model.rules:
        _add(r.get("id"))
        then = r.get("then", {})
        if isinstance(then, dict):
            _add(then.get("explanation"))

    # Action names
    for a in model.actions:
        _add(a.get("id"))

    # Mode names
    for m in model.modes:
        if isinstance(m, dict):
            _add(m.get("id"))

    return bytes(buf), offsets


def _pack_modes(model: CanonicalModel) -> bytes:
    """Pack mode names as uint16 string offsets (placeholder — modes are
    identified by index; the blob consumer uses the string table)."""
    buf = bytearray()
    for i, m in enumerate(model.modes):
        if isinstance(m, dict) and "id" in m:
            buf += struct.pack("<H", i)
    return bytes(buf)


def emit_blob(model: CanonicalModel) -> bytes:
    """Emit a .zrmb binary blob from a canonical model.

    Returns the complete blob as bytes.
    """
    # Pack all section data
    facts_data = _pack_facts(model)
    rules_data = _pack_rules(model)
    cond_data = _pack_conditions(model)
    expr_data = _pack_expressions(model)
    action_data = _pack_actions(model)
    string_data, _ = _pack_strings(model)
    mode_data = _pack_modes(model)

    sections: list[tuple[int, bytes, int, int]] = []  # (type, data, count, elem_size)

    fact_elem = 16
    if model.facts:
        sections.append((SECTION_FACTS, facts_data, len(model.facts), fact_elem))

    rule_elem = 22
    if model.rules:
        sections.append((SECTION_RULES, rules_data, len(model.rules), rule_elem))

    cond_elem = 8
    if model.conditions:
        sections.append((SECTION_CONDITIONS, cond_data, len(model.conditions), cond_elem))

    expr_elem = 20
    if model.expressions:
        sections.append((SECTION_EXPRESSIONS, expr_data, len(model.expressions), expr_elem))

    action_elem = 12
    if model.actions:
        sections.append((SECTION_ACTIONS, action_data, len(model.actions), action_elem))

    if string_data:
        sections.append((SECTION_STRINGS, string_data, len(string_data), 1))

    mode_elem = 2
    mode_count = len([m for m in model.modes if isinstance(m, dict) and "id" in m])
    if mode_count:
        sections.append((SECTION_MODES, mode_data, mode_count, mode_elem))

    # Compute layout
    section_table_size = len(sections) * _SECTION_ENTRY_SIZE
    data_start = _HEADER_LEN + section_table_size

    # Build section table and collect data blobs
    table_buf = bytearray()
    data_buf = bytearray()
    current_offset = data_start

    for sec_type, sec_data, count, elem_size in sections:
        table_buf += struct.pack(
            "<BBHHH",
            sec_type, 0,  # type + pad
            current_offset, count, elem_size,
        )
        data_buf += sec_data
        current_offset += len(sec_data)

    total_len = _HEADER_LEN + len(table_buf) + len(data_buf)

    # Build header fields (80 bytes, without CRC)
    flags = _encode_version_flags(model)
    model_hash = _pack_hash(model.model_hash)
    schema_hash = _pack_hash(model.schema_hash)

    header_fields = struct.pack(
        "<4sHHII",
        b"ZRMB", _BLOB_VERSION, flags,
        _HEADER_LEN, total_len,
    )
    header_fields += model_hash + schema_hash  # 80 bytes total

    # Assemble blob with CRC placeholder (zeroed) at bytes 80-83
    blob = bytearray(header_fields) + b"\x00\x00\x00\x00" + table_buf + data_buf
    # CRC-32 over entire blob with CRC field zeroed
    crc = zlib.crc32(bytes(blob)) & 0xFFFFFFFF
    struct.pack_into("<I", blob, 80, crc)

    return bytes(blob)


def sign_blob(blob_bytes: bytes, key_bytes: bytes) -> bytes:
    """Append a 32-byte HMAC-SHA256 signature to a .zrmb blob.

    Sets bit 0 of the flags field (offset 6-7) to indicate the blob is
    signed.  The HMAC is computed over the entire blob (with the flag
    already set), and the 32-byte digest is appended at the end.

    The CRC-32 at bytes 80-83 is recomputed to cover the updated flags.
    """
    buf = bytearray(blob_bytes)

    # Set BLOB_FLAG_SIGNED in the 16-bit LE flags at offset 6.
    flags = struct.unpack_from("<H", buf, 6)[0]
    flags |= BLOB_FLAG_SIGNED
    struct.pack_into("<H", buf, 6, flags)

    # Recompute CRC-32 with the updated flags (CRC field zeroed).
    struct.pack_into("<I", buf, 80, 0)
    crc = zlib.crc32(bytes(buf)) & 0xFFFFFFFF
    struct.pack_into("<I", buf, 80, crc)

    # Compute HMAC-SHA256 over the complete blob and append.
    sig = hmac.new(key_bytes, bytes(buf), hashlib.sha256).digest()
    return bytes(buf) + sig
