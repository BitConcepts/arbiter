# SPDX-License-Identifier: MIT
"""Tests for the arbiterc compiler pipeline."""

import tempfile
from pathlib import Path

from arbiter.compiler import CompileOptions, compile_model

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


def test_compile_c_output():
    model = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts = CompileOptions(
            out_c=Path(tmp) / "model.c",
            out_h=Path(tmp) / "model.h",
        )
        result = compile_model(model, opts)
        assert result.success
        assert (Path(tmp) / "model.c").exists()
        assert (Path(tmp) / "model.h").exists()
        assert result.model_hash


def test_compile_blob_output():
    model = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts = CompileOptions(out_blob=Path(tmp) / "model.zrmb")
        result = compile_model(model, opts)
        assert result.success
        blob = (Path(tmp) / "model.zrmb").read_bytes()
        assert blob[:4] == b"ZRMB"


def test_deterministic_output():
    model = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts1 = CompileOptions(out_c=Path(tmp) / "a.c", out_h=Path(tmp) / "a.h")
        r1 = compile_model(model, opts1)

        opts2 = CompileOptions(out_c=Path(tmp) / "b.c", out_h=Path(tmp) / "b.h")
        r2 = compile_model(model, opts2)

        assert r1.model_hash == r2.model_hash
        # Strip #include lines since they embed the output filename
        import re
        strip = lambda s: re.sub(r'#include "[^"]+"', '#include "model.h"', s)
        assert strip((Path(tmp) / "a.c").read_text()) == strip(
            (Path(tmp) / "b.c").read_text()
        )


def test_strict_mode():
    model = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts = CompileOptions(
            out_c=Path(tmp) / "model.c",
            out_h=Path(tmp) / "model.h",
            strict=True,
        )
        result = compile_model(model, opts)
        assert result.success
