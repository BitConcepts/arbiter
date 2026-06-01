# SPDX-License-Identifier: MIT
"""Binary blob (.zrmb) emitter for compiled ARB models."""

from __future__ import annotations

import struct
import zlib

from .canonical import CanonicalModel

ZRMB_MAGIC = b"ZRMB"
ZRMB_VERSION = 1


def emit_blob(model: CanonicalModel) -> bytes:
    """Emit a .zrmb binary blob from a canonical model.

    Format v0:
      magic:       4 bytes "ZRMB"
      version:     uint16_le
      flags:       uint16_le
      header_len:  uint32_le
      total_len:   uint32_le (placeholder, filled after)
      model_hash:  32 bytes
      schema_hash: 32 bytes
      crc32:       uint32_le (placeholder, filled after)
    """
    model_hash_bytes = bytes.fromhex(model.model_hash[:64].ljust(64, "0"))
    schema_hash_bytes = bytes.fromhex(model.schema_hash[:64].ljust(64, "0"))

    header = bytearray()
    header += ZRMB_MAGIC
    header += struct.pack("<H", ZRMB_VERSION)
    header += struct.pack("<H", 0)  # flags
    header += struct.pack("<I", 84)  # header_len
    header += struct.pack("<I", 0)   # total_len placeholder
    header += model_hash_bytes
    header += schema_hash_bytes
    header += struct.pack("<I", 0)   # crc32 placeholder

    # Section data (simplified for v0)
    sections = bytearray()

    # Facts section
    for i, f in enumerate(model.facts):
        ftype = {"bool": 0, "int32": 1, "uint32": 2, "enum": 3}.get(
            f.get("type", "bool"), 0
        )
        rng = f.get("range", [0, 0])
        rmin = rng[0] if isinstance(rng, list) and len(rng) >= 2 else 0
        rmax = rng[1] if isinstance(rng, list) and len(rng) >= 2 else 0
        sections += struct.pack(
            "<HBxiiHBx",
            i,           # id
            ftype,       # type
            rmin,        # range_min
            rmax,        # range_max
            f.get("stale_after_ms", 0),
            1 if f.get("safety_relevant") else 0,
        )

    # Compute total length and CRC
    total = len(header) + len(sections)
    struct.pack_into("<I", header, 12, total)  # total_len

    # CRC over everything except the CRC field (last 4 bytes of header)
    blob = bytes(header[:-4]) + bytes(sections)
    crc = zlib.crc32(blob) & 0xFFFFFFFF
    struct.pack_into("<I", header, 80, crc)  # crc32

    return bytes(header) + bytes(sections)
