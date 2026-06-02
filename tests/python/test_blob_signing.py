# SPDX-License-Identifier: MIT
"""Tests for blob signing (HMAC-SHA256)."""

import hashlib
import hmac
import struct
from pathlib import Path

from arbiter.compiler import CompileOptions, compile_model
from arbiter.emit_blob import BLOB_FLAG_SIGNED, emit_blob, sign_blob

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"
BATTERY_MODEL = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"


def _compile_blob() -> bytes:
    """Compile battery model to blob bytes."""
    result = compile_model(BATTERY_MODEL, CompileOptions())
    assert result.success
    assert result.canonical_model is not None
    return emit_blob(result.canonical_model)


def test_sign_blob_appends_32_bytes():
    blob = _compile_blob()
    key = b"test-secret-key-1234"
    signed = sign_blob(blob, key)
    assert len(signed) == len(blob) + 32


def test_sign_blob_sets_flag():
    blob = _compile_blob()
    key = b"test-secret-key-1234"
    signed = sign_blob(blob, key)

    flags = struct.unpack_from("<H", signed, 6)[0]
    assert flags & BLOB_FLAG_SIGNED


def test_sign_blob_valid_hmac():
    blob = _compile_blob()
    key = b"test-secret-key-1234"
    signed = sign_blob(blob, key)

    payload = signed[:-32]
    stored_sig = signed[-32:]

    expected = hmac.new(key, payload, hashlib.sha256).digest()
    assert stored_sig == expected


def test_sign_blob_invalid_key_fails():
    blob = _compile_blob()
    key = b"correct-key"
    signed = sign_blob(blob, key)

    payload = signed[:-32]
    stored_sig = signed[-32:]

    wrong = hmac.new(b"wrong-key", payload, hashlib.sha256).digest()
    assert stored_sig != wrong


def test_sign_blob_magic_preserved():
    blob = _compile_blob()
    key = b"test-key"
    signed = sign_blob(blob, key)
    assert signed[:4] == b"ZRMB"
