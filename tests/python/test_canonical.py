# SPDX-License-Identifier: MIT
"""Tests for canonicalization and deterministic hashing."""

from pathlib import Path
from zproj.canonical import canonicalize, to_canonical_json
from zproj.parser import parse_model
from zproj.diagnostics import DiagnosticCollector

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


def test_canonical_sorted_output():
    model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.zrm.yaml"
    data = parse_model(model_path)
    model = canonicalize(data)

    # Facts should be sorted by id
    fact_ids = [f["id"] for f in model.facts]
    assert fact_ids == sorted(fact_ids)


def test_deterministic_hash():
    model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.zrm.yaml"
    data = parse_model(model_path)

    m1 = canonicalize(data)
    m2 = canonicalize(data)

    assert m1.model_hash == m2.model_hash
    assert m1.model_hash != ""


def test_canonical_json_deterministic():
    model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.zrm.yaml"
    data = parse_model(model_path)

    m1 = canonicalize(data)
    m2 = canonicalize(data)

    assert to_canonical_json(m1) == to_canonical_json(m2)


def test_different_key_order_same_canonical():
    """Different YAML key ordering should produce same canonical form."""
    data1 = {
        "zrm_version": 0.1,
        "model": "test",
        "target": {"rtos": "zephyr"},
        "facts": [{"id": "b", "type": "bool"}, {"id": "a", "type": "int32"}],
        "rules": [],
    }
    data2 = {
        "model": "test",
        "zrm_version": 0.1,
        "facts": [{"type": "int32", "id": "a"}, {"type": "bool", "id": "b"}],
        "target": {"rtos": "zephyr"},
        "rules": [],
    }

    m1 = canonicalize(data1)
    m2 = canonicalize(data2)

    assert m1.model_hash == m2.model_hash
