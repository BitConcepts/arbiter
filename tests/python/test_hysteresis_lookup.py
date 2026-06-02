# SPDX-License-Identifier: MIT
"""Tests for hysteresis condition operator and lookup table support."""

from __future__ import annotations

import pytest

from arbiter.evaluator import ArbiterEvaluator, _table_lookup


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _model(facts=None, rules=None, actions=None, modes=None, tables=None, *, name="test_model"):
    m = {
        "arb_version": 0.1,
        "model": name,
        "target": {"rtos": "zephyr"},
        "facts": facts or [],
        "rules": rules or [],
        "actions": actions or [],
        "modes": modes or [],
    }
    if tables:
        m["tables"] = tables
    return m


def _fact(fid, ftype="int32", **kwargs):
    return {"id": fid, "type": ftype, **kwargs}


def _rule(rid, when=None, then=None, rclass="inference"):
    r = {"id": rid, "class": rclass}
    if when is not None:
        r["when"] = when
    if then is not None:
        r["then"] = then
    return r


# ===================================================================
# HYSTERESIS OPERATOR
# ===================================================================


class TestHysteresis:
    """Test the hysteresis condition operator."""

    def test_rising_edge_triggers(self):
        """Value >= rising triggers condition to true."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("temp", 85)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_falling_edge_clears(self):
        """Value <= falling clears condition to false."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        # First trigger high
        ev.set_fact("temp", 85)
        r1 = ev.eval()
        assert r1.fired_rules == ["r"]
        # Then drop below falling
        ev.set_fact("temp", 55)
        r2 = ev.eval()
        assert r2.fired_rules == []

    def test_deadband_holds_true(self):
        """Value between falling and rising holds previous true state."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        # Trigger high
        ev.set_fact("temp", 85)
        ev.eval()
        # Drop into deadband — should stay true
        ev.set_fact("temp", 70)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_deadband_holds_false(self):
        """Value between falling and rising holds previous false state."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        # Start in deadband — never triggered → false
        ev.set_fact("temp", 70)
        r = ev.eval()
        assert r.fired_rules == []

    def test_exact_rising_threshold(self):
        """Value exactly at rising threshold triggers true."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("temp", 80)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_exact_falling_threshold(self):
        """Value exactly at falling threshold clears to false."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        # Trigger
        ev.set_fact("temp", 85)
        ev.eval()
        # Drop to exactly falling
        ev.set_fact("temp", 60)
        r = ev.eval()
        assert r.fired_rules == []

    def test_hysteresis_pid_enable_disable(self):
        """PID-like model: hysteresis controls enable/disable."""
        m = _model(
            facts=[_fact("speed"), _fact("enabled", "bool")],
            rules=[
                _rule("enable_pid", when={"all": [
                    {"fact": "speed", "op": "hysteresis", "rising": 1000, "falling": 500},
                ]}, then={"compute": [
                    {"target": "enabled", "op": "assign", "left_literal": 1},
                ]}),
            ],
        )
        ev = ArbiterEvaluator(m)
        # Below both thresholds
        ev.set_fact("speed", 400)
        ev.eval()
        assert ev._fact_values["enabled"] == 0

        # Above rising
        ev.set_fact("speed", 1200)
        ev.eval()
        assert ev._fact_values["enabled"] == 1

    def test_hysteresis_persists_across_evals(self):
        """Hysteresis state persists across multiple eval() cycles."""
        m = _model(
            facts=[_fact("temp")],
            rules=[_rule("r", when={"all": [
                {"fact": "temp", "op": "hysteresis", "rising": 80, "falling": 60},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        # Trigger
        ev.set_fact("temp", 90)
        assert ev.eval().fired_rules == ["r"]
        # Deadband
        ev.set_fact("temp", 75)
        assert ev.eval().fired_rules == ["r"]
        ev.set_fact("temp", 65)
        assert ev.eval().fired_rules == ["r"]
        # Drop below falling
        ev.set_fact("temp", 59)
        assert ev.eval().fired_rules == []
        # Deadband again — stays false
        ev.set_fact("temp", 70)
        assert ev.eval().fired_rules == []


# ===================================================================
# LOOKUP TABLE — _table_lookup helper
# ===================================================================


class TestTableLookupHelper:
    """Test the _table_lookup helper directly."""

    def test_exact_key(self):
        assert _table_lookup([0, 25, 50, 75, 100], [33000, 10000, 3300, 1200, 470], 25) == 10000

    def test_exact_first_key(self):
        assert _table_lookup([0, 50, 100], [0, 500, 1000], 0) == 0

    def test_exact_last_key(self):
        assert _table_lookup([0, 50, 100], [0, 500, 1000], 100) == 1000

    def test_interpolation_midpoint(self):
        # Between 0→0 and 100→1000, at key 50 → 500
        assert _table_lookup([0, 100], [0, 1000], 50) == 500

    def test_interpolation_quarter(self):
        # Between 0→0 and 100→1000, at key 25 → 250
        assert _table_lookup([0, 100], [0, 1000], 25) == 250

    def test_below_min_clamps(self):
        assert _table_lookup([10, 50, 100], [100, 500, 1000], -5) == 100

    def test_above_max_clamps(self):
        assert _table_lookup([10, 50, 100], [100, 500, 1000], 200) == 1000

    def test_ntc_curve(self):
        """NTC thermistor curve: interpolation between known points."""
        keys = [0, 25, 50, 75, 100]
        values = [33000, 10000, 3300, 1200, 470]
        # At 0 → 33000
        assert _table_lookup(keys, values, 0) == 33000
        # At 100 → 470
        assert _table_lookup(keys, values, 100) == 470
        # Midpoint between 0 and 25: (33000 + 10000) / 2 ≈ 21500
        # Actually: 33000 + (10000-33000)*(12-0)/(25-0) = 33000 + (-23000*12/25) = 33000 - 11040 = 21960
        result = _table_lookup(keys, values, 12)
        assert 21000 < result < 22500  # approximate

    def test_empty_table(self):
        assert _table_lookup([], [], 50) == 0

    def test_descending_values(self):
        """Tables with descending values (inverse relationship)."""
        keys = [0, 50, 100]
        values = [1000, 500, 0]
        assert _table_lookup(keys, values, 25) == 750  # (1000+500)/2 = 750


# ===================================================================
# LOOKUP TABLE — via evaluator
# ===================================================================


class TestLookupTableEvaluator:
    """Test lookup table support through the evaluator."""

    def test_lookup_at_exact_key(self):
        m = _model(
            facts=[_fact("temperature"), _fact("resistance")],
            tables=[{
                "id": "ntc_curve",
                "keys": [0, 25, 50, 75, 100],
                "values": [33000, 10000, 3300, 1200, 470],
            }],
            rules=[_rule("r", then={"compute": [
                {"target": "resistance", "op": "lookup", "table": "ntc_curve", "left": "temperature"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("temperature", 25)
        ev.eval()
        assert ev._fact_values["resistance"] == 10000

    def test_lookup_interpolation(self):
        m = _model(
            facts=[_fact("input"), _fact("output")],
            tables=[{
                "id": "linear",
                "keys": [0, 100],
                "values": [0, 1000],
            }],
            rules=[_rule("r", then={"compute": [
                {"target": "output", "op": "lookup", "table": "linear", "left": "input"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("input", 50)
        ev.eval()
        assert ev._fact_values["output"] == 500

    def test_lookup_below_min(self):
        m = _model(
            facts=[_fact("input"), _fact("output")],
            tables=[{
                "id": "tbl",
                "keys": [10, 100],
                "values": [100, 1000],
            }],
            rules=[_rule("r", then={"compute": [
                {"target": "output", "op": "lookup", "table": "tbl", "left": "input"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("input", -10)
        ev.eval()
        assert ev._fact_values["output"] == 100  # clamped to first value

    def test_lookup_above_max(self):
        m = _model(
            facts=[_fact("input"), _fact("output")],
            tables=[{
                "id": "tbl",
                "keys": [10, 100],
                "values": [100, 1000],
            }],
            rules=[_rule("r", then={"compute": [
                {"target": "output", "op": "lookup", "table": "tbl", "left": "input"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("input", 200)
        ev.eval()
        assert ev._fact_values["output"] == 1000  # clamped to last value

    def test_lookup_with_condition(self):
        """Lookup only runs when rule fires."""
        m = _model(
            facts=[_fact("enable", "bool"), _fact("input"), _fact("output")],
            tables=[{
                "id": "tbl",
                "keys": [0, 100],
                "values": [0, 1000],
            }],
            rules=[_rule("r",
                when={"all": [{"fact": "enable", "op": "==", "value": 1}]},
                then={"compute": [
                    {"target": "output", "op": "lookup", "table": "tbl", "left": "input"},
                ]},
            )],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("enable", False)
        ev.set_fact("input", 50)
        ev.eval()
        assert ev._fact_values["output"] == 0  # rule didn't fire

        ev.set_fact("enable", True)
        ev.eval()
        assert ev._fact_values["output"] == 500  # rule fired

    def test_multiple_tables(self):
        """Multiple tables can coexist in one model."""
        m = _model(
            facts=[_fact("temp"), _fact("pressure"), _fact("r_temp"), _fact("r_press")],
            tables=[
                {"id": "temp_tbl", "keys": [0, 100], "values": [0, 1000]},
                {"id": "press_tbl", "keys": [0, 100], "values": [0, 5000]},
            ],
            rules=[_rule("r", then={"compute": [
                {"target": "r_temp", "op": "lookup", "table": "temp_tbl", "left": "temp"},
                {"target": "r_press", "op": "lookup", "table": "press_tbl", "left": "pressure"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("temp", 50)
        ev.set_fact("pressure", 50)
        ev.eval()
        assert ev._fact_values["r_temp"] == 500
        assert ev._fact_values["r_press"] == 2500
