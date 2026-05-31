# SPDX-License-Identifier: MIT
"""Tests for ZRM JSON schema validation."""

import pytest
from pathlib import Path
from zproj.diagnostics import DiagnosticCollector
from zproj.parser import parse_model
from zproj.schema import validate_schema

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


def test_valid_battery_model():
    model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.zrm.yaml"
    diag = DiagnosticCollector()
    data = parse_model(model_path, diag)
    assert data is not None
    errors = validate_schema(data, diag)
    assert len(errors) == 0


def test_missing_required_fields():
    diag = DiagnosticCollector()
    errors = validate_schema({"model": "test"}, diag)
    assert len(errors) > 0
    assert diag.has_errors()


def test_invalid_fact_type():
    diag = DiagnosticCollector()
    data = {
        "zrm_version": 0.1,
        "model": "test",
        "target": {"rtos": "zephyr"},
        "facts": [{"id": "f1", "type": "float64"}],
        "rules": [],
    }
    errors = validate_schema(data, diag)
    assert len(errors) > 0


def test_valid_safety_monitor_model():
    model_path = SAMPLES_DIR / "safety_monitor" / "models" / "safety_monitor.zrm.yaml"
    diag = DiagnosticCollector()
    data = parse_model(model_path, diag)
    assert data is not None
    errors = validate_schema(data, diag)
    assert len(errors) == 0
