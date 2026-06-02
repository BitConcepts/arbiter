# SPDX-License-Identifier: MIT
"""Pure-Python evaluator that mirrors the C arbiter engine exactly.

This module provides a Python implementation of the deterministic eval loop
so that models can be exercised and tested without compiling C or flashing
a Zephyr target.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from .canonical import CanonicalModel, canonicalize

# ---------------------------------------------------------------------------
# Constants — mirror the C engine
# ---------------------------------------------------------------------------

INT32_MAX = 2_147_483_647
INT32_MIN = -2_147_483_648
UINT16_MAX = 65535  # sentinel for "use literal"

# Rule class evaluation order — safety_guard runs first.
_RULE_CLASS_ORDER = {
    "safety_guard": 0,
    "obligation": 1,
    "constraint": 2,
    "mode_guard": 3,
    "inference": 4,
    "advisory": 5,
}

# Action types that map to fault operations.
_ACTION_TYPE_MAP = {
    "callback": "callback",
    "log": "log",
    "notify": "notify",
    "set_fact": "set_fact",
    "set_mode": "set_mode",
    "raise_fault": "raise_fault",
    "clear_fault": "clear_fault",
}


# ---------------------------------------------------------------------------
# Result types
# ---------------------------------------------------------------------------


@dataclass
class TraceEntry:
    """One rule evaluation trace record."""

    rule_id: str
    fired: bool
    reason: str = ""


@dataclass
class EvalResult:
    """Output of a single evaluation pass."""

    fired_rules: list[str] = field(default_factory=list)
    current_mode: str | None = None
    raised_faults: set[str] = field(default_factory=set)
    requested_actions: list[str] = field(default_factory=list)
    op_count: int = 0
    trace: list[TraceEntry] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        """Serialise to a plain dict (for JSON output)."""
        return {
            "fired_rules": self.fired_rules,
            "current_mode": self.current_mode,
            "raised_faults": sorted(self.raised_faults),
            "requested_actions": self.requested_actions,
            "op_count": self.op_count,
            "trace": [
                {"rule_id": t.rule_id, "fired": t.fired, "reason": t.reason}
                for t in self.trace
            ],
        }


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _saturate32(value: int) -> int:
    """Clamp an arbitrary-precision int to the int32 range."""
    if value > INT32_MAX:
        return INT32_MAX
    if value < INT32_MIN:
        return INT32_MIN
    return value


# ---------------------------------------------------------------------------
# Evaluator
# ---------------------------------------------------------------------------


class ArbiterEvaluator:
    """Pure-Python evaluator for a canonicalised ARB model.

    Usage::

        ev = ArbiterEvaluator(model_data)
        ev.set_fact("battery.voltage_mv", 3400)
        result = ev.eval()
    """

    def __init__(self, model_data: dict[str, Any]) -> None:
        self._raw = model_data
        self._model: CanonicalModel = canonicalize(model_data)

        # Fact state: name -> int32 value.  Initialised to defaults.
        self._fact_values: dict[str, int] = {}
        self._fact_prev: dict[str, int] = {}
        self._fact_timestamps: dict[str, int] = {}
        self._fact_valid: dict[str, bool] = {}

        for f in self._model.facts:
            name = f["id"]
            default = int(f.get("default", 0))
            if f.get("type") == "bool":
                default = int(bool(f.get("default", False)))
            self._fact_values[name] = default
            self._fact_prev[name] = default
            self._fact_timestamps[name] = 0
            self._fact_valid[name] = False

        # Snapshot timestamp (set by caller for staleness tests).
        self._snapshot_ts: int = 0

        # Faults (persistent across evals until cleared).
        self._raised_faults: set[str] = set()

    # ---- public API -------------------------------------------------------

    def set_fact(self, name: str, value: Any) -> None:
        """Set a fact value (bool → 0/1, int/str)."""
        if name not in self._fact_values:
            raise KeyError(f"Unknown fact: {name}")
        if isinstance(value, bool):
            self._fact_values[name] = int(value)
        elif isinstance(value, str):
            # CLI may pass "true" / "false"
            if value.lower() == "true":
                self._fact_values[name] = 1
            elif value.lower() == "false":
                self._fact_values[name] = 0
            else:
                self._fact_values[name] = int(value)
        else:
            self._fact_values[name] = int(value)
        self._fact_valid[name] = True

    def set_timestamp(self, name: str, ms: int) -> None:
        """Set the timestamp for a fact (for staleness detection)."""
        if name not in self._fact_values:
            raise KeyError(f"Unknown fact: {name}")
        self._fact_timestamps[name] = int(ms)
        self._fact_valid[name] = True

    def set_snapshot_timestamp(self, ms: int) -> None:
        """Set the global snapshot (eval) timestamp."""
        self._snapshot_ts = int(ms)

    def eval(self) -> EvalResult:
        """Run one evaluation pass. Returns an :class:`EvalResult`."""
        result = EvalResult()
        op_count = 0

        # Snapshot prev values for "changed" / "delta" operators.
        prev_snapshot: dict[str, int] = dict(self._fact_prev)

        # Sort rules: safety_guard first, then by canonical order (alpha by id).
        ordered_rules = sorted(
            self._model.rules,
            key=lambda r: (
                _RULE_CLASS_ORDER.get(r.get("class", "inference"), 4),
                r.get("id", ""),
            ),
        )

        for rule in ordered_rules:
            rule_id = rule["id"]
            when = rule.get("when", {})
            then = rule.get("then", {})
            if not isinstance(then, dict):
                then = {}

            fired = self._eval_condition_block(when, prev_snapshot)
            op_count += 1  # condition evaluation counts as 1 op

            reason = then.get("explanation", "")

            result.trace.append(TraceEntry(
                rule_id=rule_id,
                fired=fired,
                reason=reason if fired else "",
            ))

            if not fired:
                continue

            result.fired_rules.append(rule_id)

            # --- Mode transition ---
            mode_target = then.get("set_mode")
            if mode_target:
                result.current_mode = mode_target

            # --- Compute expressions ---
            expr_start = rule.get("_expr_start", 0)
            expr_count = rule.get("_expr_count", 0)
            for i in range(expr_start, expr_start + expr_count):
                if i < len(self._model.expressions):
                    self._exec_expression(self._model.expressions[i])
                    op_count += 1

            # --- Action ---
            action_ref = then.get("action")
            if action_ref:
                result.requested_actions.append(action_ref)
                # Check if action is raise_fault or clear_fault
                action_def = self._find_action(action_ref)
                if action_def:
                    atype = action_def.get("type", "callback")
                    if atype == "raise_fault":
                        self._raised_faults.add(action_ref)
                    elif atype == "clear_fault":
                        self._raised_faults.discard(action_ref)

            # --- Inline raise_fault / clear_fault in then block ---
            if then.get("raise_fault"):
                fault_id = then["raise_fault"]
                self._raised_faults.add(fault_id)
                result.requested_actions.append(f"raise_fault:{fault_id}")
            if then.get("clear_fault"):
                fault_id = then["clear_fault"]
                self._raised_faults.discard(fault_id)
                result.requested_actions.append(f"clear_fault:{fault_id}")

        # Save prev values for next eval cycle.
        self._fact_prev = dict(self._fact_values)

        result.raised_faults = set(self._raised_faults)
        result.op_count = op_count
        return result

    # ---- condition evaluation ---------------------------------------------

    def _eval_condition_block(
        self,
        when: dict[str, Any],
        prev_snapshot: dict[str, int],
    ) -> bool:
        """Evaluate a top-level condition block (may have all/any/not groups)."""
        if not isinstance(when, dict) or not when:
            # Empty condition block → always true (unconditional rule).
            return True

        # Process each group type present.  Multiple groups are AND-ed.
        group_results: list[bool] = []

        for group_type in ("all", "any", "not"):
            group = when.get(group_type)
            if group is None:
                continue
            if not isinstance(group, list):
                group = [group]

            result = self._eval_group(group_type, group, prev_snapshot)
            group_results.append(result)

        if not group_results:
            return True
        return all(group_results)

    def _eval_group(
        self,
        group_type: str,
        conditions: list[Any],
        prev_snapshot: dict[str, int],
    ) -> bool:
        """Evaluate a condition group (ALL / ANY / NOT)."""
        if group_type == "all":
            for cond in conditions:
                if not isinstance(cond, dict):
                    continue
                if not self._eval_single_condition(cond, prev_snapshot):
                    return False  # short-circuit
            return True

        if group_type == "any":
            for cond in conditions:
                if not isinstance(cond, dict):
                    continue
                if self._eval_single_condition(cond, prev_snapshot):
                    return True  # short-circuit
            return False

        if group_type == "not":
            # NOT inverts: true if ALL inner conditions are false.
            for cond in conditions:
                if not isinstance(cond, dict):
                    continue
                if self._eval_single_condition(cond, prev_snapshot):
                    return False  # one was true → NOT fails
            return True

        return True  # unknown group → vacuously true

    def _eval_single_condition(
        self,
        cond: dict[str, Any],
        prev_snapshot: dict[str, int],
    ) -> bool:
        """Evaluate one condition (fact op value)."""
        fact_name = cond.get("fact", "")
        op = cond.get("op", "==")
        threshold = cond.get("value", 0)

        fact_val = self._fact_values.get(fact_name, 0)

        if op == "==":
            return self._coerce_eq(fact_val, threshold)
        if op == "!=":
            return not self._coerce_eq(fact_val, threshold)
        if op == "<":
            return fact_val < int(threshold)
        if op == "<=":
            return fact_val <= int(threshold)
        if op == ">":
            return fact_val > int(threshold)
        if op == ">=":
            return fact_val >= int(threshold)
        if op == "in":
            if isinstance(threshold, list):
                return fact_val in [int(v) for v in threshold]
            return fact_val == int(threshold)
        if op == "not_in":
            if isinstance(threshold, list):
                return fact_val not in [int(v) for v in threshold]
            return fact_val != int(threshold)
        if op == "stale":
            return self._check_stale(fact_name, int(threshold))
        if op == "not_stale":
            return not self._check_stale(fact_name, int(threshold))
        if op == "changed":
            prev = prev_snapshot.get(fact_name, 0)
            return fact_val != prev
        if op == "delta_gt":
            prev = prev_snapshot.get(fact_name, 0)
            delta = abs(fact_val - prev)
            return delta > int(threshold)
        if op == "delta_lt":
            prev = prev_snapshot.get(fact_name, 0)
            delta = abs(fact_val - prev)
            return delta < int(threshold)

        return False  # unknown operator

    @staticmethod
    def _coerce_eq(fact_val: int, threshold: Any) -> bool:
        """Equality with bool coercion: true/false → 1/0."""
        if isinstance(threshold, bool):
            return fact_val == int(threshold)
        return fact_val == int(threshold)

    def _check_stale(self, fact_name: str, threshold_ms: int) -> bool:
        """Return True if the fact's timestamp is stale w.r.t. snapshot time."""
        ts = self._fact_timestamps.get(fact_name, 0)
        if ts == 0 and not self._fact_valid.get(fact_name, False):
            # Never written → stale.
            return True
        age = self._snapshot_ts - ts
        return age > threshold_ms

    # ---- expression execution ---------------------------------------------

    def _exec_expression(self, expr: dict[str, Any]) -> None:
        """Execute one compute expression, writing the result to a fact."""
        target_id = expr.get("target_fact_id", 0)
        target_name = self._fact_name_by_index(target_id)
        if target_name is None:
            return

        op = expr.get("op", "assign")
        left = self._resolve_operand(
            expr.get("left_fact_id", UINT16_MAX),
            expr.get("left_literal", 0),
        )
        right = self._resolve_operand(
            expr.get("right_fact_id", UINT16_MAX),
            expr.get("right_literal", 0),
        )
        scale = expr.get("scale", 1)

        result = self._compute_op(op, target_name, left, right, scale)
        self._fact_values[target_name] = result

    def _compute_op(
        self,
        op: str,
        target_name: str,
        left: int,
        right: int,
        scale: int,
    ) -> int:
        """Compute one expression opcode. Returns saturated int32 result."""
        if op == "assign":
            return _saturate32(left)

        if op == "add":
            return _saturate32(left + right)

        if op == "sub":
            return _saturate32(left - right)

        if op == "mul":
            return _saturate32(left * right)

        if op == "div":
            if right == 0:
                return 0
            # Truncate toward zero (C behaviour).
            return _saturate32(int(left / right))

        if op == "mod":
            if right == 0:
                return 0
            # Python mod differs from C for negative numbers.
            # C99: result has the sign of the dividend.
            if left == 0:
                return 0
            result = abs(left) % abs(right)
            return _saturate32(-result if left < 0 else result)

        if op == "abs":
            return _saturate32(abs(left))

        if op == "negate":
            return _saturate32(-left)

        if op == "min":
            return _saturate32(min(left, right))

        if op == "max":
            return _saturate32(max(left, right))

        if op == "clamp":
            # clamp(left, lo=right, hi=scale)
            lo = right
            hi = scale
            return _saturate32(max(lo, min(left, hi)))

        if op == "shift_r":
            return _saturate32(left >> right)

        if op == "shift_l":
            return _saturate32(left << right)

        if op == "scale":
            # target = (left * right) / scale   (int64 widening)
            if scale == 0:
                return 0
            wide: int = left * right  # Python int is arbitrary precision
            return _saturate32(int(wide / scale))

        if op == "accumulate":
            # target = target + (left * right) / scale  (int64 widening)
            current = self._fact_values.get(target_name, 0)
            if scale == 0:
                return _saturate32(current)
            wide = left * right
            return _saturate32(current + int(wide / scale))

        return 0  # unknown op

    def _resolve_operand(self, fact_id: int, literal: int) -> int:
        """Resolve an operand: use fact value if fact_id != UINT16_MAX, else literal."""
        if fact_id == UINT16_MAX:
            return int(literal)
        name = self._fact_name_by_index(fact_id)
        if name is None:
            return int(literal)
        return self._fact_values.get(name, 0)

    def _fact_name_by_index(self, idx: int) -> str | None:
        """Map a canonical fact index back to its name."""
        if 0 <= idx < len(self._model.facts):
            return self._model.facts[idx]["id"]
        return None

    def _find_action(self, action_id: str) -> dict[str, Any] | None:
        """Look up an action definition by its id string."""
        for a in self._model.actions:
            if a.get("id") == action_id:
                return a
        return None
