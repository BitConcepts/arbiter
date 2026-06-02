# SPDX-License-Identifier: MIT
"""Cross-validation harness — proves Python evaluator matches golden vectors.

Each vector in tests/vectors/<name>/vector.json contains an inline ARB model,
input facts/timestamps, and expected outputs.  The Python evaluator is the
reference implementation; these vectors will also be consumed by the C engine
under Zephyr to prove cross-platform equivalence.

Tests:
  1. Parametrised golden-vector evaluation (10+ vectors).
  2. Determinism: same input, 100 runs → identical output.
  3. Compile-to-C: each vector model compiles and the generated source
     contains the required ARBITER_generated_model symbol.
"""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

import pytest

from arbiter.compiler import CompileOptions, compile_model
from arbiter.evaluator import ArbiterEvaluator

VECTORS_DIR = Path(__file__).resolve().parent.parent / "vectors"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _discover_vectors() -> list[str]:
    """Return sorted list of vector directory names that contain vector.json."""
    if not VECTORS_DIR.exists():
        return []
    return sorted(
        d.name
        for d in VECTORS_DIR.iterdir()
        if d.is_dir() and (d / "vector.json").exists()
    )


def _load_vector(name: str) -> dict:
    """Load and parse a vector.json file."""
    path = VECTORS_DIR / name / "vector.json"
    return json.loads(path.read_text(encoding="utf-8"))


def _run_vector(vec: dict) -> tuple[ArbiterEvaluator, dict]:
    """Run the Python evaluator on a vector and return (evaluator, result_dict)."""
    model_data = vec["model"]
    ev = ArbiterEvaluator(model_data)

    # Set fact values
    for fact_name, value in vec.get("facts", {}).items():
        ev.set_fact(fact_name, value)

    # Set timestamps
    for fact_name, ms in vec.get("timestamps", {}).items():
        ev.set_timestamp(fact_name, ms)

    # Set snapshot timestamp
    snap_ts = vec.get("snapshot_timestamp_ms", 0)
    if snap_ts:
        ev.set_snapshot_timestamp(snap_ts)

    result = ev.eval()
    return ev, result


# ---------------------------------------------------------------------------
# 1. Golden vector evaluation
# ---------------------------------------------------------------------------

_VECTOR_NAMES = _discover_vectors()


@pytest.mark.parametrize("vector_name", _VECTOR_NAMES or ["_no_vectors_"])
def test_golden_vector(vector_name: str) -> None:
    """Evaluate each golden vector and assert output matches expected."""
    if vector_name == "_no_vectors_":
        pytest.fail("No golden vectors found in tests/vectors/")

    vec = _load_vector(vector_name)
    expected = vec["expected"]

    ev, result = _run_vector(vec)

    # --- fired_rules: exact ordered list ---
    assert result.fired_rules == expected["fired_rules"], (
        f"[{vector_name}] fired_rules mismatch"
    )

    # --- current_mode ---
    assert result.current_mode == expected.get("current_mode"), (
        f"[{vector_name}] current_mode mismatch"
    )

    # --- raised_faults: sorted set comparison ---
    assert sorted(result.raised_faults) == sorted(expected.get("raised_faults", [])), (
        f"[{vector_name}] raised_faults mismatch"
    )

    # --- requested_actions: ordered list ---
    assert result.requested_actions == expected.get("requested_actions", []), (
        f"[{vector_name}] requested_actions mismatch"
    )

    # --- fact_values: spot-check only the facts listed in expected ---
    expected_facts = expected.get("fact_values", {})
    for fact_name, expected_val in expected_facts.items():
        actual = ev._fact_values.get(fact_name)
        assert actual == expected_val, (
            f"[{vector_name}] fact {fact_name}: expected {expected_val}, got {actual}"
        )


# ---------------------------------------------------------------------------
# 2. Determinism — same input, 100 runs, identical output
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("vector_name", _VECTOR_NAMES[:3] or ["_no_vectors_"])
def test_determinism(vector_name: str) -> None:
    """Run the same vector 100 times and assert all outputs are identical."""
    if vector_name == "_no_vectors_":
        pytest.skip("No vectors for determinism test")

    vec = _load_vector(vector_name)
    results: list[dict] = []

    for _ in range(100):
        _, result = _run_vector(vec)
        results.append(result.to_dict())

    baseline = results[0]
    for i, r in enumerate(results[1:], start=1):
        assert r == baseline, (
            f"[{vector_name}] Non-deterministic result on iteration {i}"
        )


# ---------------------------------------------------------------------------
# 3. Compile-to-C — verify each vector model compiles to valid C source
# ---------------------------------------------------------------------------


@pytest.mark.parametrize("vector_name", _VECTOR_NAMES or ["_no_vectors_"])
def test_compile_to_c(vector_name: str) -> None:
    """Compile each vector model to C and verify the source contains required symbols."""
    if vector_name == "_no_vectors_":
        pytest.skip("No vectors for compile test")

    vec = _load_vector(vector_name)
    model_data = vec["model"]

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp = Path(tmpdir)
        # Write model as YAML for the compiler
        import yaml

        model_path = tmp / "model.arb.yaml"
        model_path.write_text(
            yaml.dump(model_data, default_flow_style=False), encoding="utf-8"
        )

        opts = CompileOptions(
            out_c=tmp / "model.c",
            out_h=tmp / "model.h",
        )
        result = compile_model(model_path, opts)

        assert result.success, (
            f"[{vector_name}] Compilation failed: "
            + "; ".join(
                d.message
                for d in result.diagnostics.errors
            )
        )

        # Verify generated C source contains required symbols
        c_source = (tmp / "model.c").read_text(encoding="utf-8")
        h_source = (tmp / "model.h").read_text(encoding="utf-8")

        assert "ARBITER_generated_model" in c_source, (
            f"[{vector_name}] Missing ARBITER_generated_model in C source"
        )
        assert "ARBITER_generated_model" in h_source, (
            f"[{vector_name}] Missing ARBITER_generated_model in header"
        )
        assert "ARBITER_MODEL_HASH" in h_source, (
            f"[{vector_name}] Missing ARBITER_MODEL_HASH in header"
        )
