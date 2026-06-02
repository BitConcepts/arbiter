# SPDX-License-Identifier: MIT
"""Tests for emit_graph — Mermaid and DOT model visualization."""

from pathlib import Path

from arbiter.canonical import canonicalize
from arbiter.diagnostics import DiagnosticCollector
from arbiter.emit_graph import emit_dot, emit_mermaid
from arbiter.parser import parse_model
from arbiter.schema import validate_schema
from arbiter.validator import validate_model

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"
BATTERY_MODEL = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"


def _load_canonical(path: Path):
    diag = DiagnosticCollector()
    data = parse_model(path, diag)
    assert data is not None, diag.format()
    validate_schema(data, diag)
    validate_model(data, diag)
    assert not diag.has_errors(), diag.format()
    return canonicalize(data)


def test_mermaid_contains_expected_nodes():
    model = _load_canonical(BATTERY_MODEL)
    output = emit_mermaid(model)

    # Should start with flowchart declaration
    assert output.startswith("flowchart TD")

    # Fact nodes present
    assert "battery_voltage_mv" in output
    assert "battery_temp_c" in output
    assert "charger_enabled" in output

    # Rule nodes present
    assert "rule_low_battery" in output
    assert "rule_critical_battery" in output
    assert "rule_thermal_shutdown" in output
    assert "rule_charging" in output

    # Action nodes present
    assert "disable_load" in output
    assert "disable_charger" in output


def test_mermaid_contains_edges():
    model = _load_canonical(BATTERY_MODEL)
    output = emit_mermaid(model)

    # Condition edges (fact → rule)
    assert "battery_voltage_mv" in output
    assert "rule_critical_battery" in output

    # Action edges (rule → action)
    assert "disable_load" in output


def test_mermaid_has_style_classes():
    model = _load_canonical(BATTERY_MODEL)
    output = emit_mermaid(model)

    # Green fact styles
    assert "fill:#90EE90" in output
    # Red safety_guard styles
    assert "fill:#FF6B6B" in output
    # Blue inference styles
    assert "fill:#87CEEB" in output
    # Orange action styles
    assert "fill:#FFA500" in output


def test_dot_output():
    model = _load_canonical(BATTERY_MODEL)
    output = emit_dot(model)

    assert output.startswith("digraph arbiter {")
    assert output.strip().endswith("}")

    # Fact nodes
    assert "battery_voltage_mv" in output
    assert "battery_temp_c" in output

    # Rule nodes
    assert "rule_critical_battery" in output

    # Action nodes
    assert "disable_load" in output

    # Edges
    assert "->" in output
