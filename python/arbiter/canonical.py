# SPDX-License-Identifier: MIT
"""Canonicalization and deterministic hashing for ARB models."""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field
from typing import Any


@dataclass
class CanonicalModel:
    """Canonicalized ARB model with computed hashes and ordered tables."""

    name: str
    arb_version: float
    facts: list[dict[str, Any]]
    rules: list[dict[str, Any]]
    conditions: list[dict[str, Any]]
    actions: list[dict[str, Any]]
    modes: list[dict[str, Any]]
    expressions: list[dict[str, Any]] = field(default_factory=list)
    hazards: list[dict[str, Any]] = field(default_factory=list)
    safety_goals: list[dict[str, Any]] = field(default_factory=list)
    version: str | None = None
    model_hash: str = ""
    schema_hash: str = ""
    compiler_version: str = "0.1.0"
    fact_id_map: dict[str, int] = field(default_factory=dict)
    rule_id_map: dict[str, int] = field(default_factory=dict)
    action_id_map: dict[str, int] = field(default_factory=dict)
    mode_id_map: dict[str, int] = field(default_factory=dict)

    @property
    def max_facts(self) -> int:
        return len(self.facts)

    @property
    def max_rules(self) -> int:
        return len(self.rules)

    @property
    def max_conditions(self) -> int:
        return len(self.conditions)

    @property
    def max_actions(self) -> int:
        return len(self.actions)

    @property
    def max_expressions(self) -> int:
        return len(self.expressions)


def canonicalize(data: dict[str, Any]) -> CanonicalModel:
    """Canonicalize a parsed ARB model.

    Sorts facts, rules, actions by their id strings, assigns sequential
    uint16 indices, flattens condition trees, and computes deterministic hashes.
    """
    # Sort facts by id
    facts = sorted(data.get("facts", []), key=lambda f: f.get("id", ""))
    fact_id_map = {f["id"]: i for i, f in enumerate(facts)}

    # Sort and index modes
    modes = data.get("modes", [])
    if isinstance(modes, list):
        modes = sorted(modes, key=lambda m: m.get("id", "") if isinstance(m, dict) else "")
    mode_id_map = {}
    for i, m in enumerate(modes):
        if isinstance(m, dict) and "id" in m:
            mode_id_map[m["id"]] = i

    # Sort actions by id
    actions = sorted(data.get("actions", []), key=lambda a: a.get("id", ""))
    action_id_map = {a["id"]: i for i, a in enumerate(actions)}

    # Sort rules by id and flatten conditions + expressions
    rules_raw = sorted(data.get("rules", []), key=lambda r: r.get("id", ""))
    rule_id_map = {r["id"]: i for i, r in enumerate(rules_raw)}

    # Flatten condition trees and compute expressions from all rules.
    # Annotate each rule dict with expr_start / expr_count so the emitter
    # can write them into the rules table without a second pass.
    conditions: list[dict[str, Any]] = []
    expressions: list[dict[str, Any]] = []
    rules: list[dict[str, Any]] = []
    for rule in rules_raw:
        when = rule.get("when", {})
        if isinstance(when, dict):
            _flatten_conditions(when, conditions, fact_id_map)

        expr_start = len(expressions)
        rule_exprs = _flatten_expressions(
            rule.get("then", {}), fact_id_map
        )
        expressions.extend(rule_exprs)

        # Attach start/count to a copy of the rule so we don't mutate input.
        annotated = dict(rule)
        annotated["_expr_start"] = expr_start
        annotated["_expr_count"] = len(rule_exprs)
        rules.append(annotated)

    model = CanonicalModel(
        name=data.get("model", "unnamed"),
        arb_version=data.get("arb_version", 0.1),
        facts=facts,
        rules=rules,
        conditions=conditions,
        actions=actions,
        modes=modes,
        expressions=expressions,
        hazards=data.get("hazards", []),
        safety_goals=data.get("safety_goals", []),
        version=data.get("version"),
        fact_id_map=fact_id_map,
        rule_id_map=rule_id_map,
        action_id_map=action_id_map,
        mode_id_map=mode_id_map,
        compiler_version="0.1.0",
    )

    # Compute hashes
    canonical_json = to_canonical_json(model)
    model.model_hash = hashlib.sha256(canonical_json.encode("utf-8")).hexdigest()
    model.schema_hash = hashlib.sha256(b"zrm_schema_v0.1").hexdigest()

    return model


_UINT16_MAX = 65535  # UINT16_MAX sentinel for "use literal"

_EXPR_OP_ALIASES: dict[str, str] = {
    "assign": "assign", "add": "add", "sub": "sub", "mul": "mul",
    "div": "div", "mod": "mod", "abs": "abs", "negate": "negate",
    "min": "min", "max": "max", "clamp": "clamp",
    "shift_r": "shift_r", "shift_l": "shift_l",
    "scale": "scale", "accumulate": "accumulate",
}


def _flatten_expressions(
    then: Any,
    fact_id_map: dict[str, int],
) -> list[dict[str, Any]]:
    """Extract compute expressions from a rule's then block.

    Each entry maps to an ``ARBITER_expr_def`` with resolved fact indices.
    ``UINT16_MAX`` (65535) signals "use the literal instead of a fact".
    """
    if not isinstance(then, dict):
        return []
    compute = then.get("compute", [])
    if not isinstance(compute, list):
        return []

    out: list[dict[str, Any]] = []
    for expr in compute:
        if not isinstance(expr, dict):
            continue

        target = expr.get("target", "")
        target_id = fact_id_map.get(target, 0)

        # Left operand
        left_name = expr.get("left")
        if left_name is not None and left_name in fact_id_map:
            left_fact_id = fact_id_map[left_name]
            left_literal: int = 0
        else:
            left_fact_id = _UINT16_MAX
            left_literal = int(expr.get("left_literal", 0))

        # Right operand
        right_name = expr.get("right")
        if right_name is not None and right_name in fact_id_map:
            right_fact_id = fact_id_map[right_name]
            right_literal: int = 0
        else:
            right_fact_id = _UINT16_MAX
            right_literal = int(expr.get("right_literal", 0))

        op = _EXPR_OP_ALIASES.get(expr.get("op", "assign"), "assign")
        scale = int(expr.get("scale", 1))

        out.append({
            "target_fact_id": target_id,
            "op": op,
            "left_fact_id": left_fact_id,
            "left_literal": left_literal,
            "right_fact_id": right_fact_id,
            "right_literal": right_literal,
            "scale": scale,
        })
    return out


def _flatten_conditions(
    when: dict[str, Any],
    conditions: list[dict[str, Any]],
    fact_id_map: dict[str, int],
) -> None:
    """Flatten condition groups into a linear list."""
    for group_type in ("all", "any", "not"):
        group = when.get(group_type)
        if group is None:
            continue
        if not isinstance(group, list):
            group = [group]
        for cond in group:
            if not isinstance(cond, dict):
                continue
            flat = {
                "group": group_type,
                "fact": cond.get("fact", ""),
                "fact_id": fact_id_map.get(cond.get("fact", ""), 0),
                "op": cond.get("op", "=="),
                "value": cond.get("value", 0),
            }
            conditions.append(flat)


def to_canonical_json(model: CanonicalModel) -> str:
    """Produce deterministic JSON representation of the canonical model."""
    obj = {
        "name": model.name,
        "arb_version": model.arb_version,
        "compiler_version": model.compiler_version,
        "facts": [
            {"id": f["id"], "type": f.get("type", "bool"), "index": i}
            for i, f in enumerate(model.facts)
        ],
        "rules": [
            {"id": r["id"], "class": r.get("class", "inference"), "index": i}
            for i, r in enumerate(model.rules)
        ],
        "actions": [
            {"id": a["id"], "type": a.get("type", "callback"), "index": i}
            for i, a in enumerate(model.actions)
        ],
        "modes": [
            {"id": m["id"], "index": i}
            for i, m in enumerate(model.modes)
            if isinstance(m, dict) and "id" in m
        ],
    }
    return json.dumps(obj, sort_keys=True, indent=None, separators=(",", ":"))
