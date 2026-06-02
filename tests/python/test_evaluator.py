# SPDX-License-Identifier: MIT
"""Comprehensive tests for the Python evaluator (REQ-ARCH-035)."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from arbiter.evaluator import (
    INT32_MAX,
    INT32_MIN,
    ArbiterEvaluator,
    EvalResult,
    TraceEntry,
    _saturate32,
)

SAMPLES_DIR = Path(__file__).resolve().parent.parent.parent / "samples"


# ---------------------------------------------------------------------------
# Helper: build a minimal model dict
# ---------------------------------------------------------------------------


def _model(
    facts=None,
    rules=None,
    actions=None,
    modes=None,
    *,
    name="test_model",
):
    """Return a minimal valid ARB model dict."""
    return {
        "arb_version": 0.1,
        "model": name,
        "target": {"rtos": "zephyr"},
        "facts": facts or [],
        "rules": rules or [],
        "actions": actions or [],
        "modes": modes or [],
    }


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
# 1. BASIC EVALUATION
# ===================================================================


class TestBasicEval:
    def test_no_rules(self):
        """Empty model with no rules should return empty result."""
        m = _model(facts=[_fact("x")])
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.fired_rules == []
        assert r.current_mode is None
        assert r.op_count == 0

    def test_single_rule_fires(self):
        """Rule fires when condition is met."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r1", when={"all": [{"fact": "x", "op": ">", "value": 10}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 20)
        r = ev.eval()
        assert r.fired_rules == ["r1"]

    def test_single_rule_does_not_fire(self):
        """Rule does not fire when condition is not met."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r1", when={"all": [{"fact": "x", "op": ">", "value": 10}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 5)
        r = ev.eval()
        assert r.fired_rules == []

    def test_unconditional_rule(self):
        """Rule with no 'when' block always fires."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r1")],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.fired_rules == ["r1"]


# ===================================================================
# 2. CONDITION OPERATORS (all 13)
# ===================================================================


class TestConditionOperators:
    def _eval_op(self, op, fact_val, threshold):
        """Helper: evaluate a single condition with the given operator."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": op, "value": threshold}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", fact_val)
        return ev.eval()

    def test_eq_true(self):
        assert self._eval_op("==", 42, 42).fired_rules == ["r"]

    def test_eq_false(self):
        assert self._eval_op("==", 42, 43).fired_rules == []

    def test_eq_bool_true(self):
        assert self._eval_op("==", 1, True).fired_rules == ["r"]

    def test_eq_bool_false(self):
        assert self._eval_op("==", 0, True).fired_rules == []

    def test_ne_true(self):
        assert self._eval_op("!=", 42, 43).fired_rules == ["r"]

    def test_ne_false(self):
        assert self._eval_op("!=", 42, 42).fired_rules == []

    def test_lt_true(self):
        assert self._eval_op("<", 5, 10).fired_rules == ["r"]

    def test_lt_false(self):
        assert self._eval_op("<", 10, 5).fired_rules == []

    def test_le_true_equal(self):
        assert self._eval_op("<=", 10, 10).fired_rules == ["r"]

    def test_le_true_less(self):
        assert self._eval_op("<=", 5, 10).fired_rules == ["r"]

    def test_le_false(self):
        assert self._eval_op("<=", 11, 10).fired_rules == []

    def test_gt_true(self):
        assert self._eval_op(">", 10, 5).fired_rules == ["r"]

    def test_gt_false(self):
        assert self._eval_op(">", 5, 10).fired_rules == []

    def test_ge_true_equal(self):
        assert self._eval_op(">=", 10, 10).fired_rules == ["r"]

    def test_ge_false(self):
        assert self._eval_op(">=", 9, 10).fired_rules == []

    def test_in_list(self):
        assert self._eval_op("in", 2, [1, 2, 3]).fired_rules == ["r"]

    def test_in_list_miss(self):
        assert self._eval_op("in", 5, [1, 2, 3]).fired_rules == []

    def test_not_in_list(self):
        assert self._eval_op("not_in", 5, [1, 2, 3]).fired_rules == ["r"]

    def test_not_in_list_miss(self):
        assert self._eval_op("not_in", 2, [1, 2, 3]).fired_rules == []

    def test_stale(self):
        """Fact is stale when timestamp age exceeds threshold."""
        m = _model(
            facts=[_fact("x", stale_after_ms=100)],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "stale", "value": 100}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 42)
        ev.set_timestamp("x", 0)
        ev.set_snapshot_timestamp(200)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_not_stale(self):
        """Fact is not stale when recently updated."""
        m = _model(
            facts=[_fact("x", stale_after_ms=100)],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "not_stale", "value": 100}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 42)
        ev.set_timestamp("x", 150)
        ev.set_snapshot_timestamp(200)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_changed(self):
        """Changed detects when a fact value differs from its previous value."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "changed", "value": 0}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 10)
        # First eval: prev is default (0), current is 10 → changed
        r = ev.eval()
        assert r.fired_rules == ["r"]
        # Second eval: prev is now 10, current still 10 → not changed
        r2 = ev.eval()
        assert r2.fired_rules == []

    def test_delta_gt(self):
        """Delta > threshold."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "delta_gt", "value": 5}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 10)
        r = ev.eval()
        assert r.fired_rules == ["r"]  # |10 - 0| = 10 > 5

    def test_delta_lt(self):
        """Delta < threshold."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "delta_lt", "value": 5}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 2)
        r = ev.eval()
        assert r.fired_rules == ["r"]  # |2 - 0| = 2 < 5


# ===================================================================
# 3. CONDITION GROUPS
# ===================================================================


class TestConditionGroups:
    def test_all_short_circuit(self):
        """ALL group short-circuits on first false."""
        m = _model(
            facts=[_fact("x"), _fact("y")],
            rules=[_rule("r", when={"all": [
                {"fact": "x", "op": ">", "value": 10},
                {"fact": "y", "op": ">", "value": 10},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 5)  # fails
        ev.set_fact("y", 20)
        r = ev.eval()
        assert r.fired_rules == []

    def test_any_short_circuit(self):
        """ANY group short-circuits on first true."""
        m = _model(
            facts=[_fact("x"), _fact("y")],
            rules=[_rule("r", when={"any": [
                {"fact": "x", "op": ">", "value": 10},
                {"fact": "y", "op": ">", "value": 10},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 20)
        ev.set_fact("y", 5)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_not_group(self):
        """NOT group inverts: fires if inner condition is false."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"not": [
                {"fact": "x", "op": "==", "value": 1},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 0)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_not_group_fails(self):
        """NOT group fails when inner condition is true."""
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", when={"not": [
                {"fact": "x", "op": "==", "value": 1},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 1)
        r = ev.eval()
        assert r.fired_rules == []


# ===================================================================
# 4. SAFETY GUARD ORDERING
# ===================================================================


class TestSafetyGuardOrdering:
    def test_safety_guard_fires_before_inference(self):
        """Safety guard rules must execute before inference rules."""
        m = _model(
            facts=[_fact("x")],
            rules=[
                _rule("infer_first_alpha", rclass="inference"),
                _rule("safety_second_alpha", rclass="safety_guard"),
            ],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        # Both fire (unconditional), but safety_guard must be first.
        assert r.fired_rules[0] == "safety_second_alpha"
        assert r.fired_rules[1] == "infer_first_alpha"

    def test_full_class_ordering(self):
        """All rule classes are evaluated in the correct priority order."""
        m = _model(
            facts=[_fact("x")],
            rules=[
                _rule("advisory_r", rclass="advisory"),
                _rule("inference_r", rclass="inference"),
                _rule("constraint_r", rclass="constraint"),
                _rule("mode_guard_r", rclass="mode_guard"),
                _rule("obligation_r", rclass="obligation"),
                _rule("safety_guard_r", rclass="safety_guard"),
            ],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.fired_rules == [
            "safety_guard_r",
            "obligation_r",
            "constraint_r",
            "mode_guard_r",
            "inference_r",
            "advisory_r",
        ]


# ===================================================================
# 5. EXPRESSION OPCODES (all 15)
# ===================================================================


class TestExpressionOpcodes:
    def _eval_compute(self, facts, exprs, fact_values=None):
        """Helper: run a rule with compute expressions and return fact values."""
        m = _model(
            facts=facts,
            rules=[_rule("r", then={"compute": exprs})],
        )
        ev = ArbiterEvaluator(m)
        if fact_values:
            for k, v in fact_values.items():
                ev.set_fact(k, v)
        ev.eval()
        return ev._fact_values

    def test_assign_literal(self):
        vals = self._eval_compute(
            [_fact("out")],
            [{"target": "out", "op": "assign", "left_literal": 42}],
        )
        assert vals["out"] == 42

    def test_assign_fact(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "assign", "left": "a"}],
            {"a": 99},
        )
        assert vals["out"] == 99

    def test_add(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "add", "left": "a", "right": "b"}],
            {"a": 100, "b": 200},
        )
        assert vals["out"] == 300

    def test_sub(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "sub", "left": "a", "right": "b"}],
            {"a": 100, "b": 30},
        )
        assert vals["out"] == 70

    def test_mul(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "mul", "left": "a", "right": "b"}],
            {"a": 7, "b": 6},
        )
        assert vals["out"] == 42

    def test_div(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "div", "left": "a", "right": "b"}],
            {"a": 100, "b": 3},
        )
        assert vals["out"] == 33  # truncate toward zero

    def test_div_by_zero(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "div", "left": "a", "right": "b"}],
            {"a": 100, "b": 0},
        )
        assert vals["out"] == 0

    def test_mod(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "mod", "left": "a", "right": "b"}],
            {"a": 17, "b": 5},
        )
        assert vals["out"] == 2

    def test_mod_negative(self):
        """C-style mod: sign follows dividend."""
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "mod", "left": "a", "right": "b"}],
            {"a": -17, "b": 5},
        )
        assert vals["out"] == -2

    def test_abs(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "abs", "left": "a"}],
            {"a": -42},
        )
        assert vals["out"] == 42

    def test_negate(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "negate", "left": "a"}],
            {"a": 42},
        )
        assert vals["out"] == -42

    def test_min(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "min", "left": "a", "right": "b"}],
            {"a": 10, "b": 3},
        )
        assert vals["out"] == 3

    def test_max(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "max", "left": "a", "right": "b"}],
            {"a": 10, "b": 3},
        )
        assert vals["out"] == 10

    def test_clamp(self):
        """clamp(left, lo=right, hi=scale)."""
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "clamp", "left": "a", "right_literal": -100, "scale": 100}],
            {"a": 200},
        )
        assert vals["out"] == 100  # clamped to hi

    def test_clamp_lo(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "clamp", "left": "a", "right_literal": -100, "scale": 100}],
            {"a": -200},
        )
        assert vals["out"] == -100  # clamped to lo

    def test_shift_r(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "shift_r", "left": "a", "right_literal": 2}],
            {"a": 100},
        )
        assert vals["out"] == 25

    def test_shift_l(self):
        vals = self._eval_compute(
            [_fact("a"), _fact("out")],
            [{"target": "out", "op": "shift_l", "left": "a", "right_literal": 3}],
            {"a": 5},
        )
        assert vals["out"] == 40

    def test_scale(self):
        """scale: target = (left * right) / scale."""
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "scale", "left": "a", "right": "b", "scale": 1000}],
            {"a": 5000, "b": 2500},
        )
        assert vals["out"] == 12500  # 5000*2500/1000

    def test_scale_saturation(self):
        """scale with large values should saturate to INT32_MAX."""
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [{"target": "out", "op": "scale", "left": "a", "right": "b", "scale": 1}],
            {"a": INT32_MAX, "b": 2},
        )
        assert vals["out"] == INT32_MAX

    def test_accumulate(self):
        """accumulate: target = target + (left * right) / scale."""
        vals = self._eval_compute(
            [_fact("a"), _fact("b"), _fact("out")],
            [
                {"target": "out", "op": "assign", "left_literal": 100},
                {"target": "out", "op": "accumulate", "left": "a", "right": "b", "scale": 10},
            ],
            {"a": 50, "b": 3},
        )
        assert vals["out"] == 115  # 100 + (50*3)/10 = 100 + 15


# ===================================================================
# 6. INT32 SATURATION
# ===================================================================


class TestSaturation:
    def test_saturate32_max(self):
        assert _saturate32(INT32_MAX + 1) == INT32_MAX

    def test_saturate32_min(self):
        assert _saturate32(INT32_MIN - 1) == INT32_MIN

    def test_add_overflow(self):
        m = _model(
            facts=[_fact("a"), _fact("b"), _fact("out")],
            rules=[_rule("r", then={"compute": [
                {"target": "out", "op": "add", "left": "a", "right": "b"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("a", INT32_MAX)
        ev.set_fact("b", 1)
        ev.eval()
        assert ev._fact_values["out"] == INT32_MAX

    def test_sub_underflow(self):
        m = _model(
            facts=[_fact("a"), _fact("b"), _fact("out")],
            rules=[_rule("r", then={"compute": [
                {"target": "out", "op": "sub", "left": "a", "right": "b"},
            ]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("a", INT32_MIN)
        ev.set_fact("b", 1)
        ev.eval()
        assert ev._fact_values["out"] == INT32_MIN


# ===================================================================
# 7. MODE TRANSITIONS
# ===================================================================


class TestModeTransitions:
    def test_mode_set(self):
        m = _model(
            facts=[_fact("x")],
            modes=[{"id": "mode.a"}, {"id": "mode.b"}],
            rules=[_rule("r", then={"set_mode": "mode.a"})],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.current_mode == "mode.a"

    def test_last_mode_wins(self):
        """When multiple rules set mode, the last one in eval order wins."""
        m = _model(
            facts=[_fact("x")],
            modes=[{"id": "mode.a"}, {"id": "mode.b"}],
            rules=[
                _rule("r1", then={"set_mode": "mode.a"}),
                _rule("r2", then={"set_mode": "mode.b"}),
            ],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.current_mode == "mode.b"


# ===================================================================
# 8. ACTION COLLECTION
# ===================================================================


class TestActionCollection:
    def test_action_collected(self):
        m = _model(
            facts=[_fact("x")],
            actions=[{"id": "act1", "type": "callback"}],
            rules=[_rule("r", then={"action": "act1"})],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert r.requested_actions == ["act1"]


# ===================================================================
# 9. FAULT RAISE / CLEAR
# ===================================================================


class TestFaults:
    def test_raise_fault(self):
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r", then={"raise_fault": "fault.overheat"})],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert "fault.overheat" in r.raised_faults

    def test_clear_fault(self):
        m = _model(
            facts=[_fact("x")],
            rules=[
                _rule("r1", then={"raise_fault": "fault.overheat"}),
                _rule("r2", then={"clear_fault": "fault.overheat"}),
            ],
        )
        ev = ArbiterEvaluator(m)
        r = ev.eval()
        assert "fault.overheat" not in r.raised_faults

    def test_fault_persists_across_evals(self):
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r1", when={"all": [{"fact": "x", "op": "==", "value": 1}]},
                         then={"raise_fault": "fault.overheat"})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 1)
        r1 = ev.eval()
        assert "fault.overheat" in r1.raised_faults
        # Second eval with x=0 — rule doesn't fire but fault persists.
        ev.set_fact("x", 0)
        r2 = ev.eval()
        assert "fault.overheat" in r2.raised_faults


# ===================================================================
# 10. DETERMINISM
# ===================================================================


class TestDeterminism:
    def test_same_input_same_output(self):
        """Running the same model with the same facts must produce identical results."""
        m = _model(
            facts=[_fact("x"), _fact("y")],
            rules=[
                _rule("r1", when={"all": [{"fact": "x", "op": ">", "value": 5}]},
                      then={"set_mode": "active"}),
                _rule("r2", when={"all": [{"fact": "y", "op": "==", "value": 1}]}),
            ],
            modes=[{"id": "active"}],
        )
        results = []
        for _ in range(5):
            ev = ArbiterEvaluator(m)
            ev.set_fact("x", 10)
            ev.set_fact("y", 1)
            results.append(ev.eval().to_dict())

        # All 5 runs should be identical.
        for r in results[1:]:
            assert r == results[0]


# ===================================================================
# 11. TRACE
# ===================================================================


class TestTrace:
    def test_trace_records_all_rules(self):
        m = _model(
            facts=[_fact("x")],
            rules=[
                _rule("r1", when={"all": [{"fact": "x", "op": ">", "value": 5}]}),
                _rule("r2", when={"all": [{"fact": "x", "op": "<", "value": 5}]}),
            ],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 10)
        r = ev.eval()
        assert len(r.trace) == 2
        ids = [t.rule_id for t in r.trace]
        assert "r1" in ids
        assert "r2" in ids

    def test_trace_records_fired_status(self):
        m = _model(
            facts=[_fact("x")],
            rules=[_rule("r1", when={"all": [{"fact": "x", "op": "==", "value": 1}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 1)
        r = ev.eval()
        assert r.trace[0].fired is True

        ev2 = ArbiterEvaluator(m)
        ev2.set_fact("x", 0)
        r2 = ev2.eval()
        assert r2.trace[0].fired is False


# ===================================================================
# 12. EVAL RESULT SERIALIZATION
# ===================================================================


class TestEvalResultSerialization:
    def test_to_dict(self):
        r = EvalResult(
            fired_rules=["r1"],
            current_mode="active",
            raised_faults={"fault.a", "fault.b"},
            requested_actions=["act1"],
            op_count=5,
            trace=[TraceEntry("r1", True, "reason")],
        )
        d = r.to_dict()
        assert d["fired_rules"] == ["r1"]
        assert d["current_mode"] == "active"
        assert d["raised_faults"] == ["fault.a", "fault.b"]  # sorted
        assert d["op_count"] == 5
        # JSON-roundtrip should work.
        assert json.loads(json.dumps(d)) == d


# ===================================================================
# 13. SAMPLE MODEL EVALUATION
# ===================================================================


class TestSampleModels:
    def test_battery_critical(self):
        """Battery model: voltage < 3000 triggers critical safety guard."""
        import yaml

        model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
        data = yaml.safe_load(model_path.read_text(encoding="utf-8"))
        ev = ArbiterEvaluator(data)
        ev.set_fact("battery.voltage_mv", 2900)
        ev.set_fact("battery.current_ma", 0)
        ev.set_fact("battery.temp_c", 25)
        ev.set_fact("charger.enabled", False)
        r = ev.eval()
        # Safety guards fire first.  critical_battery should fire.
        assert "rule.critical_battery" in r.fired_rules
        assert r.current_mode in ("mode.critical", "mode.low_battery")

    def test_battery_normal(self):
        """Battery model: normal voltage, charger on → charging mode."""
        import yaml

        model_path = SAMPLES_DIR / "battery_policy" / "models" / "battery.arb.yaml"
        data = yaml.safe_load(model_path.read_text(encoding="utf-8"))
        ev = ArbiterEvaluator(data)
        ev.set_fact("battery.voltage_mv", 3800)
        ev.set_fact("battery.current_ma", 500)
        ev.set_fact("battery.temp_c", 25)
        ev.set_fact("charger.enabled", True)
        r = ev.eval()
        assert "rule.charging" in r.fired_rules
        assert r.current_mode == "mode.charging"


# ===================================================================
# 14. PID COMPUTE MODEL
# ===================================================================


class TestPidModel:
    def test_pid_compute(self):
        """PID model: enabled + valid sensor → compute PID terms."""
        import yaml

        model_path = SAMPLES_DIR / "pid_controller" / "models" / "pid_engine.arb.yaml"
        data = yaml.safe_load(model_path.read_text(encoding="utf-8"))
        ev = ArbiterEvaluator(data)
        ev.set_fact("in.enable", True)
        ev.set_fact("in.sensor_valid", True)
        ev.set_fact("in.process_value", 90000)
        ev.set_fact("in.setpoint", 100000)
        ev.set_fact("in.dt_ms", 10)
        ev.set_fact("gain.kp", 2500)
        ev.set_fact("gain.ki", 100)
        ev.set_fact("gain.kd", 800)
        r = ev.eval()
        assert "10_pid.compute" in r.fired_rules
        # Error should be 100000 - 90000 = 10000
        assert ev._fact_values["pid.error"] == 10000
        # P-term should be (10000 * 2500) / 1000 = 25000
        assert ev._fact_values["pid.p_term"] == 25000

    def test_pid_sensor_fault(self):
        """PID model: sensor invalid → safety guard fires, output zeroed."""
        import yaml

        model_path = SAMPLES_DIR / "pid_controller" / "models" / "pid_engine.arb.yaml"
        data = yaml.safe_load(model_path.read_text(encoding="utf-8"))
        ev = ArbiterEvaluator(data)
        ev.set_fact("in.enable", True)
        ev.set_fact("in.sensor_valid", False)
        ev.set_fact("in.process_value", 90000)
        ev.set_fact("in.setpoint", 100000)
        ev.set_fact("in.dt_ms", 10)
        r = ev.eval()
        assert "01_fault.sensor" in r.fired_rules
        assert r.current_mode == "mode.sensor_fault"
        assert ev._fact_values["pid.output"] == 0


# ===================================================================
# 15. STALENESS EDGE CASES
# ===================================================================


class TestStalenessEdgeCases:
    def test_never_written_is_stale(self):
        """A fact that was never written should be considered stale."""
        m = _model(
            facts=[_fact("x", stale_after_ms=100)],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "stale", "value": 100}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_snapshot_timestamp(1000)
        r = ev.eval()
        assert r.fired_rules == ["r"]

    def test_exact_threshold_not_stale(self):
        """Age exactly equal to threshold should NOT be stale (> not >=)."""
        m = _model(
            facts=[_fact("x", stale_after_ms=100)],
            rules=[_rule("r", when={"all": [{"fact": "x", "op": "stale", "value": 100}]})],
        )
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", 42)
        ev.set_timestamp("x", 0)
        ev.set_snapshot_timestamp(100)
        r = ev.eval()
        assert r.fired_rules == []  # age = 100, threshold = 100 → not stale


# ===================================================================
# 16. SET_FACT FROM STRING (CLI COERCION)
# ===================================================================


class TestSetFactCoercion:
    def test_string_true(self):
        m = _model(facts=[_fact("x", "bool")])
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", "true")
        assert ev._fact_values["x"] == 1

    def test_string_false(self):
        m = _model(facts=[_fact("x", "bool")])
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", "false")
        assert ev._fact_values["x"] == 0

    def test_string_int(self):
        m = _model(facts=[_fact("x")])
        ev = ArbiterEvaluator(m)
        ev.set_fact("x", "42")
        assert ev._fact_values["x"] == 42

    def test_unknown_fact_raises(self):
        m = _model(facts=[_fact("x")])
        ev = ArbiterEvaluator(m)
        with pytest.raises(KeyError):
            ev.set_fact("nonexistent", 1)
