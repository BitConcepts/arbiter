# SPDX-License-Identifier: MIT
"""Canonicalization and deterministic hashing for ZRM models."""

from __future__ import annotations

import hashlib
import json
from dataclasses import dataclass, field
from typing import Any


@dataclass
class CanonicalModel:
    """Canonicalized ZRM model with computed hashes and ordered tables."""

    name: str
    zrm_version: float
    facts: list[dict[str, Any]]
    rules: list[dict[str, Any]]
    conditions: list[dict[str, Any]]
    actions: list[dict[str, Any]]
    modes: list[dict[str, Any]]
    hazards: list[dict[str, Any]] = field(default_factory=list)
    safety_goals: list[dict[str, Any]] = field(default_factory=list)
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


def canonicalize(data: dict[str, Any]) -> CanonicalModel:
    """Canonicalize a parsed ZRM model.

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

    # Sort rules by id and flatten conditions
    rules = sorted(data.get("rules", []), key=lambda r: r.get("id", ""))
    rule_id_map = {r["id"]: i for i, r in enumerate(rules)}

    # Flatten condition trees from all rules
    conditions: list[dict[str, Any]] = []
    for rule in rules:
        when = rule.get("when", {})
        if isinstance(when, dict):
            _flatten_conditions(when, conditions, fact_id_map)

    model = CanonicalModel(
        name=data.get("model", "unnamed"),
        zrm_version=data.get("zrm_version", 0.1),
        facts=facts,
        rules=rules,
        conditions=conditions,
        actions=actions,
        modes=modes,
        hazards=data.get("hazards", []),
        safety_goals=data.get("safety_goals", []),
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
        "zrm_version": model.zrm_version,
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
