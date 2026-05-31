# SPDX-License-Identifier: MIT
"""Semantic validator for ARB models."""

from __future__ import annotations

from typing import Any

from .diagnostics import DiagnosticCollector

VALID_FACT_TYPES = {"bool", "int32", "uint32", "enum"}
VALID_RULE_CLASSES = {
    "inference", "constraint", "mode_guard",
    "safety_guard", "obligation", "advisory",
}
VALID_OPERATORS = {
    "==", "!=", "<", "<=", ">", ">=",
    "in", "not_in", "stale", "not_stale",
    "changed", "delta_gt", "delta_lt",
}
VALID_ACTION_TYPES = {
    "callback", "log", "notify", "set_fact",
    "set_mode", "raise_fault", "clear_fault",
}
STRICT_PROFILE = "zrm_safety_strict_v0"


def validate_model(
    data: dict[str, Any],
    diag: DiagnosticCollector,
    strict: bool = False,
    safety_profile: str | None = None,
) -> bool:
    """Run all semantic validation phases. Returns True if no errors."""
    _validate_required_keys(data, diag)
    if diag.has_errors():
        return False

    fact_ids = _validate_facts(data, diag)
    mode_ids = _validate_modes(data, diag)
    action_ids = _validate_actions(data, diag)
    _validate_rules(data, diag, fact_ids, mode_ids, action_ids)
    _validate_references(data, diag, fact_ids, mode_ids, action_ids)

    if strict or safety_profile == STRICT_PROFILE:
        _validate_strict_profile(data, diag)

    return not diag.has_errors()


def _validate_required_keys(data: dict[str, Any], diag: DiagnosticCollector) -> None:
    for key in ("arb_version", "model", "target", "facts", "rules"):
        if key not in data:
            diag.error("arbiter-E-MISSING-KEY", f"(root).{key}", f"Required key '{key}' is missing")

    if "arb_version" in data and data["arb_version"] != 0.1:
        diag.error(
            "arbiter-E-VERSION",
            "(root).arb_version",
            f"Unsupported ARB version: {data['arb_version']}",
        )

    if "target" in data:
        target = data["target"]
        if not isinstance(target, dict) or "rtos" not in target:
            diag.error("arbiter-E-TARGET", "(root).target", "target must have 'rtos' key")


def _validate_facts(data: dict[str, Any], diag: DiagnosticCollector) -> set[str]:
    fact_ids: set[str] = set()
    facts = data.get("facts", [])
    if not isinstance(facts, list):
        diag.error("arbiter-E-FACTS-TYPE", "(root).facts", "facts must be a list")
        return fact_ids

    for i, fact in enumerate(facts):
        path = f"facts[{i}]"
        if not isinstance(fact, dict):
            diag.error("arbiter-E-FACT-TYPE", path, "Each fact must be a mapping")
            continue

        fid = fact.get("id")
        if not fid:
            diag.error("arbiter-E-FACT-NO-ID", path, "Fact missing 'id'")
            continue

        if fid in fact_ids:
            diag.error("arbiter-E-FACT-DUPLICATE", path, f"Duplicate fact id '{fid}'")
        fact_ids.add(fid)

        ftype = fact.get("type")
        if ftype not in VALID_FACT_TYPES:
            diag.error("arbiter-E-FACT-BAD-TYPE", f"{path}.type", f"Invalid fact type '{ftype}'")

        rng = fact.get("range")
        if rng is not None:
            if not isinstance(rng, list) or len(rng) != 2:
                diag.error("arbiter-E-FACT-BAD-RANGE", f"{path}.range", "range must be [min, max]")
            elif rng[0] > rng[1]:
                diag.error("arbiter-E-FACT-BAD-RANGE", f"{path}.range", "range min > max")

    return fact_ids


def _validate_modes(data: dict[str, Any], diag: DiagnosticCollector) -> set[str]:
    mode_ids: set[str] = set()
    modes = data.get("modes", [])
    if not isinstance(modes, list):
        return mode_ids

    for i, mode in enumerate(modes):
        if isinstance(mode, dict) and "id" in mode:
            mode_ids.add(mode["id"])

    return mode_ids


def _validate_actions(data: dict[str, Any], diag: DiagnosticCollector) -> set[str]:
    action_ids: set[str] = set()
    actions = data.get("actions", [])
    if not isinstance(actions, list):
        return action_ids

    for i, action in enumerate(actions):
        path = f"actions[{i}]"
        if not isinstance(action, dict):
            continue

        aid = action.get("id")
        if aid:
            if aid in action_ids:
                diag.error("arbiter-E-ACTION-DUPLICATE", path, f"Duplicate action id '{aid}'")
            action_ids.add(aid)

        atype = action.get("type")
        if atype and atype not in VALID_ACTION_TYPES:
            diag.error(
                "arbiter-E-ACTION-BAD-TYPE",
                f"{path}.type",
                f"Invalid action type '{atype}'",
            )

    return action_ids


def _validate_rules(
    data: dict[str, Any],
    diag: DiagnosticCollector,
    fact_ids: set[str],
    mode_ids: set[str],
    action_ids: set[str],
) -> None:
    rules = data.get("rules", [])
    rule_ids: set[str] = set()

    for i, rule in enumerate(rules):
        path = f"rules[{i}]"
        if not isinstance(rule, dict):
            diag.error("arbiter-E-RULE-TYPE", path, "Each rule must be a mapping")
            continue

        rid = rule.get("id")
        if not rid:
            diag.error("arbiter-E-RULE-NO-ID", path, "Rule missing 'id'")
            continue

        if rid in rule_ids:
            diag.error("arbiter-E-RULE-DUPLICATE", path, f"Duplicate rule id '{rid}'")
        rule_ids.add(rid)

        rclass = rule.get("class")
        if rclass and rclass not in VALID_RULE_CLASSES:
            diag.error("arbiter-E-RULE-BAD-CLASS", f"{path}.class", f"Invalid class '{rclass}'")

        _validate_conditions(rule.get("when", {}), f"{path}.when", fact_ids, diag)

        then = rule.get("then", {})
        if isinstance(then, dict):
            if "set_mode" in then and mode_ids and then["set_mode"] not in mode_ids:
                diag.error(
                    "arbiter-E-REF-UNRESOLVED",
                    f"{path}.then.set_mode",
                    f"Mode '{then['set_mode']}' not defined",
                )
            if "action" in then and action_ids and then["action"] not in action_ids:
                diag.error(
                    "arbiter-E-REF-UNRESOLVED",
                    f"{path}.then.action",
                    f"Action '{then['action']}' not defined",
                )


def _validate_conditions(
    when: Any, path: str, fact_ids: set[str], diag: DiagnosticCollector
) -> None:
    if not isinstance(when, dict):
        return

    for group_key in ("all", "any"):
        group = when.get(group_key)
        if group is None:
            continue
        if not isinstance(group, list):
            diag.error("arbiter-E-COND-TYPE", f"{path}.{group_key}", "Must be a list")
            continue
        for j, cond in enumerate(group):
            if not isinstance(cond, dict):
                continue
            fact_ref = cond.get("fact")
            if fact_ref and fact_ids and fact_ref not in fact_ids:
                diag.error(
                    "arbiter-E-REF-UNRESOLVED",
                    f"{path}.{group_key}[{j}].fact",
                    f"Fact '{fact_ref}' not defined",
                )
            op = cond.get("op")
            if op and op not in VALID_OPERATORS:
                diag.error(
                    "arbiter-E-COND-BAD-OP",
                    f"{path}.{group_key}[{j}].op",
                    f"Invalid operator '{op}'",
                )


def _validate_references(
    data: dict[str, Any],
    diag: DiagnosticCollector,
    fact_ids: set[str],
    mode_ids: set[str],
    action_ids: set[str],
) -> None:
    """Cross-reference validation for safety goals and hazards."""
    hazard_ids = {h["id"] for h in data.get("hazards", []) if isinstance(h, dict) and "id" in h}
    for i, sg in enumerate(data.get("safety_goals", [])):
        if not isinstance(sg, dict):
            continue
        if "hazard" in sg and sg["hazard"] not in hazard_ids:
            diag.warning(
                "arbiter-W-REF-UNRESOLVED",
                f"safety_goals[{i}].hazard",
                f"Hazard '{sg['hazard']}' not defined",
            )
        if "safe_state" in sg and mode_ids and sg["safe_state"] not in mode_ids:
            diag.warning(
                "arbiter-W-REF-UNRESOLVED",
                f"safety_goals[{i}].safe_state",
                f"Mode '{sg['safe_state']}' not defined",
            )


def _validate_strict_profile(data: dict[str, Any], diag: DiagnosticCollector) -> None:
    """Validate strict safety profile constraints."""
    for i, fact in enumerate(data.get("facts", [])):
        if not isinstance(fact, dict):
            continue
        if fact.get("type") == "float":
            diag.error(
                "arbiter-E-SAFETY-NO-FLOAT",
                f"facts[{i}]",
                "Floating point not allowed in strict safety profile",
            )
        if fact.get("safety_relevant") and not fact.get("range"):
            diag.warning(
                "arbiter-W-SAFETY-NO-RANGE",
                f"facts[{i}]",
                "Safety-relevant fact should have a range",
            )
        if fact.get("safety_relevant") and not fact.get("stale_after_ms"):
            diag.warning(
                "arbiter-W-SAFETY-NO-STALE",
                f"facts[{i}]",
                "Safety-relevant fact should have stale_after_ms",
            )

    for i, rule in enumerate(data.get("rules", [])):
        if not isinstance(rule, dict):
            continue
        then = rule.get("then", {})
        if isinstance(then, dict) and then.get("criticality") == "safety_critical":
            if rule.get("class") not in ("safety_guard", "obligation"):
                diag.warning(
                    "arbiter-W-SAFETY-GUARD-CLASS",
                    f"rules[{i}]",
                    "Safety-critical rule should use safety_guard or obligation class",
                )
