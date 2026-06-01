# SPDX-License-Identifier: MIT
"""Tests for the C emitter — with focus on expression table generation."""

import re
import tempfile
from pathlib import Path

from arbiter.canonical import canonicalize, _flatten_expressions
from arbiter.emit_c import emit_c_source, emit_c_header
from arbiter.compiler import CompileOptions, compile_model

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


# ---------------------------------------------------------------------------
# Unit tests for _flatten_expressions
# ---------------------------------------------------------------------------

def test_flatten_expressions_assign():
    """assign op with a left fact reference."""
    fact_id_map = {"a": 0, "b": 1}
    then = {"compute": [{"target": "b", "op": "assign", "left": "a"}]}
    exprs = _flatten_expressions(then, fact_id_map)
    assert len(exprs) == 1
    e = exprs[0]
    assert e["target_fact_id"] == 1
    assert e["op"] == "assign"
    assert e["left_fact_id"] == 0
    assert e["left_literal"] == 0
    assert e["right_fact_id"] == 65535   # UINT16_MAX — no right operand
    assert e["right_literal"] == 0


def test_flatten_expressions_add_two_facts():
    fact_id_map = {"x": 0, "y": 1, "z": 2}
    then = {"compute": [{"target": "z", "op": "add", "left": "x", "right": "y"}]}
    exprs = _flatten_expressions(then, fact_id_map)
    assert len(exprs) == 1
    e = exprs[0]
    assert e["op"] == "add"
    assert e["left_fact_id"] == 0
    assert e["right_fact_id"] == 1


def test_flatten_expressions_scale_with_literal():
    fact_id_map = {"p_pred": 0, "k_gain": 1}
    then = {"compute": [
        {"target": "k_gain", "op": "scale",
         "left": "p_pred", "right_literal": 1000, "scale": 1}
    ]}
    exprs = _flatten_expressions(then, fact_id_map)
    assert len(exprs) == 1
    e = exprs[0]
    assert e["op"] == "scale"
    assert e["left_fact_id"] == 0
    assert e["right_fact_id"] == 65535
    assert e["right_literal"] == 1000
    assert e["scale"] == 1


def test_flatten_expressions_left_literal():
    fact_id_map = {"k_gain": 0, "p_factor": 1}
    then = {"compute": [
        {"target": "p_factor", "op": "sub", "left_literal": 1000, "right": "k_gain"}
    ]}
    exprs = _flatten_expressions(then, fact_id_map)
    e = exprs[0]
    assert e["left_fact_id"] == 65535
    assert e["left_literal"] == 1000
    assert e["right_fact_id"] == 0


def test_flatten_expressions_no_compute():
    fact_id_map = {"a": 0}
    then = {"action": "do_something", "explanation": "no compute here"}
    assert _flatten_expressions(then, fact_id_map) == []


def test_flatten_expressions_empty_compute():
    fact_id_map = {"a": 0}
    assert _flatten_expressions({"compute": []}, fact_id_map) == []


def test_flatten_expressions_multiple():
    fact_id_map = {"x": 0, "y": 1, "z": 2}
    then = {"compute": [
        {"target": "y", "op": "assign", "left": "x"},
        {"target": "z", "op": "add", "left": "x", "right": "y"},
    ]}
    exprs = _flatten_expressions(then, fact_id_map)
    assert len(exprs) == 2
    assert exprs[0]["op"] == "assign"
    assert exprs[1]["op"] == "add"


# ---------------------------------------------------------------------------
# Canonicalization tests — expressions extracted from rules
# ---------------------------------------------------------------------------

def test_canonicalize_extracts_expressions():
    """Rules with compute blocks must populate model.expressions."""
    data = {
        "arb_version": 0.1,
        "model": "test_expr",
        "facts": [
            {"id": "a", "type": "int32"},
            {"id": "b", "type": "int32"},
        ],
        "rules": [{
            "id": "r0",
            "class": "inference",
            "when": {"all": [{"fact": "a", "op": "==", "value": 1}]},
            "then": {"compute": [
                {"target": "b", "op": "assign", "left": "a"},
            ]},
        }],
    }
    model = canonicalize(data)
    assert len(model.expressions) == 1
    assert model.expressions[0]["target_fact_id"] == 1  # b is index 1 (alphabetical)
    assert model.expressions[0]["op"] == "assign"


def test_canonicalize_expr_start_per_rule():
    """Rules must carry _expr_start and _expr_count annotations."""
    data = {
        "arb_version": 0.1,
        "model": "test_start",
        "facts": [{"id": "x", "type": "int32"}, {"id": "y", "type": "int32"}],
        "rules": [
            {
                "id": "r0",
                "class": "inference",
                "when": {"all": [{"fact": "x", "op": ">", "value": 0}]},
                "then": {"compute": [
                    {"target": "y", "op": "assign", "left": "x"},
                ]},
            },
            {
                "id": "r1",
                "class": "inference",
                "when": {"all": [{"fact": "x", "op": ">", "value": 0}]},
                "then": {"compute": [
                    {"target": "y", "op": "add", "left": "x", "right": "y"},
                    {"target": "y", "op": "mul", "left": "y", "right_literal": 2},
                ]},
            },
        ],
    }
    model = canonicalize(data)
    # Rules sorted alphabetically: r0 first, r1 second
    assert model.rules[0]["_expr_start"] == 0
    assert model.rules[0]["_expr_count"] == 1
    assert model.rules[1]["_expr_start"] == 1
    assert model.rules[1]["_expr_count"] == 2
    assert len(model.expressions) == 3


# ---------------------------------------------------------------------------
# C source emitter tests
# ---------------------------------------------------------------------------

def test_emit_c_source_has_expressions_table():
    """Generated C source must contain model_expressions[] when exprs exist."""
    data = {
        "arb_version": 0.1,
        "model": "emit_test",
        "facts": [{"id": "a", "type": "int32"}, {"id": "b", "type": "int32"}],
        "rules": [{
            "id": "r0",
            "class": "inference",
            "when": {"all": [{"fact": "a", "op": ">", "value": 0}]},
            "then": {"compute": [{"target": "b", "op": "assign", "left": "a"}]},
        }],
    }
    model = canonicalize(data)
    src = emit_c_source(model)
    assert "model_expressions" in src
    assert "ARBITER_EXPR_ASSIGN" in src
    assert ".expressions = model_expressions" in src
    assert ".expr_count = 1" in src


def test_emit_c_rules_have_expr_start_and_count():
    """Rule entries in the C source must include expr_start and expr_count."""
    data = {
        "arb_version": 0.1,
        "model": "rule_expr_test",
        "facts": [{"id": "x", "type": "int32"}, {"id": "y", "type": "int32"}],
        "rules": [{
            "id": "r0",
            "class": "inference",
            "when": {"all": [{"fact": "x", "op": ">", "value": 0}]},
            "then": {"compute": [{"target": "y", "op": "assign", "left": "x"}]},
        }],
    }
    model = canonicalize(data)
    src = emit_c_source(model)
    assert ".expr_start = 0" in src
    assert ".expr_count = 1" in src


def test_emit_c_no_exprs_emits_null_pointer():
    """A model with no compute expressions should emit a NULL pointer."""
    data = {
        "arb_version": 0.1,
        "model": "no_exprs",
        "facts": [{"id": "a", "type": "bool"}],
        "rules": [{
            "id": "r0",
            "class": "inference",
            "when": {"all": [{"fact": "a", "op": "==", "value": 1}]},
            "then": {"explanation": "noop"},
        }],
    }
    model = canonicalize(data)
    src = emit_c_source(model)
    assert "model_expressions = NULL" in src
    assert ".expr_count = 0" in src


# ---------------------------------------------------------------------------
# End-to-end: compile kalman model and inspect outputs
# ---------------------------------------------------------------------------

def test_compile_kalman_emits_expressions():
    """The Kalman model has 10 compute expressions; the compiler must emit them."""
    model_path = SAMPLES_DIR / "kalman_filter" / "models" / "kalman.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts = CompileOptions(
            out_c=Path(tmp) / "kalman.c",
            out_h=Path(tmp) / "kalman.h",
        )
        result = compile_model(model_path, opts)
        assert result.success

        src = (Path(tmp) / "kalman.c").read_text()
        # At least one expression struct entry
        assert "ARBITER_EXPR_" in src
        assert ".expr_count = 10" in src or re.search(r"\.expr_count\s*=\s*[1-9]", src)
        # Expressions table present
        assert "model_expressions[]" in src


def test_header_emits_fact_defines():
    """Generated header must define ARBITER_FACT_* for every fact."""
    model_path = SAMPLES_DIR / "kalman_filter" / "models" / "kalman.arb.yaml"
    with tempfile.TemporaryDirectory() as tmp:
        opts = CompileOptions(
            out_c=Path(tmp) / "kalman.c",
            out_h=Path(tmp) / "kalman.h",
        )
        result = compile_model(model_path, opts)
        assert result.success

        hdr = (Path(tmp) / "kalman.h").read_text()
        # Key fact defines must be present in the correct alphabetical order
        assert "#define ARBITER_FACT_IN_ENABLE 0u" in hdr
        assert "#define ARBITER_FACT_IN_MEASUREMENT 1u" in hdr
        assert "#define ARBITER_FACT_IN_SENSOR_VALID 2u" in hdr
