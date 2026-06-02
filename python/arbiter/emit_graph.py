# SPDX-License-Identifier: MIT
"""Graph emitters for ARB model visualization (Mermaid and Graphviz DOT)."""

from __future__ import annotations

from .canonical import CanonicalModel


def _sanitize_id(name: str) -> str:
    """Convert an ARB identifier to a graph-safe node ID."""
    return name.replace(".", "_").replace("-", "_")


def emit_mermaid(model: CanonicalModel) -> str:
    """Emit a Mermaid flowchart from a canonical ARB model.

    Node styles:
      - Facts: green stadium (input)
      - Rules: blue (inference) or red (safety_guard) boxes
      - Actions: orange hexagons (output)

    Edges:
      - fact → rule (condition dependency)
      - rule → action (then.action)
      - rule → fact (compute target)
    """
    lines: list[str] = ["flowchart TD"]

    # Collect fact IDs referenced by conditions so we know which facts to show
    fact_ids_used: set[str] = set()
    for c in model.conditions:
        fact_name = c.get("fact", "")
        if fact_name:
            fact_ids_used.add(fact_name)

    # Fact nodes (green stadium shapes)
    for f in model.facts:
        fid = f["id"]
        nid = _sanitize_id(fid)
        lines.append(f"    {nid}([{fid}])")

    # Action nodes (orange hexagon shapes)
    for a in model.actions:
        aid = a["id"]
        nid = _sanitize_id(aid)
        lines.append(f"    {nid}{{{{{aid}}}}}")

    # Rule nodes and edges
    cond_offset = 0
    for r in model.rules:
        rid = r["id"]
        rnid = _sanitize_id(rid)
        rclass = r.get("class", "inference")

        # Rule node shape: rectangle with label
        lines.append(f"    {rnid}[{rid}]")

        # Condition edges: fact → rule
        when = r.get("when", {})
        if isinstance(when, dict):
            for gk in ("all", "any", "not"):
                g = when.get(gk)
                if isinstance(g, list):
                    for cond in g:
                        if isinstance(cond, dict):
                            fact_name = cond.get("fact", "")
                            if fact_name:
                                fnid = _sanitize_id(fact_name)
                                op = cond.get("op", "==")
                                val = cond.get("value", "")
                                lines.append(
                                    f"    {fnid} -->|{op} {val}| {rnid}"
                                )

        # Action edges: rule → action
        then = r.get("then", {})
        if isinstance(then, dict):
            action_name = then.get("action")
            if action_name:
                anid = _sanitize_id(action_name)
                lines.append(f"    {rnid} --> {anid}")

            # Compute edges: rule → fact (target)
            compute = then.get("compute", [])
            if isinstance(compute, list):
                for expr in compute:
                    if isinstance(expr, dict):
                        target = expr.get("target", "")
                        if target:
                            tnid = _sanitize_id(target)
                            lines.append(f"    {rnid} -.->|compute| {tnid}")

            # Mode edges: rule → mode (shown as text label)
            mode_name = then.get("set_mode")
            if mode_name:
                mnid = _sanitize_id(mode_name)
                lines.append(f"    {rnid} -->|set_mode| {mnid}")

    # Style classes
    lines.append("")
    # Facts: green
    for f in model.facts:
        nid = _sanitize_id(f["id"])
        lines.append(f"    style {nid} fill:#90EE90,stroke:#228B22")
    # Rules: blue or red
    for r in model.rules:
        rnid = _sanitize_id(r["id"])
        rclass = r.get("class", "inference")
        if rclass == "safety_guard":
            lines.append(f"    style {rnid} fill:#FF6B6B,stroke:#CC0000")
        else:
            lines.append(f"    style {rnid} fill:#87CEEB,stroke:#4682B4")
    # Actions: orange
    for a in model.actions:
        nid = _sanitize_id(a["id"])
        lines.append(f"    style {nid} fill:#FFA500,stroke:#CC7000")

    return "\n".join(lines) + "\n"


def emit_dot(model: CanonicalModel) -> str:
    """Emit a Graphviz DOT digraph from a canonical ARB model.

    Node styles:
      - Facts: green ellipse (input)
      - Rules: blue (inference) or red (safety_guard) box
      - Actions: orange hexagon (output)
    """
    lines: list[str] = [
        "digraph arbiter {",
        "    rankdir=TD;",
        '    node [fontname="Helvetica", fontsize=10];',
        '    edge [fontname="Helvetica", fontsize=8];',
        "",
    ]

    # Fact nodes
    for f in model.facts:
        fid = f["id"]
        nid = _sanitize_id(fid)
        lines.append(
            f'    {nid} [label="{fid}", shape=ellipse, '
            f'style=filled, fillcolor="#90EE90"];'
        )

    # Action nodes
    for a in model.actions:
        aid = a["id"]
        nid = _sanitize_id(aid)
        lines.append(
            f'    {nid} [label="{aid}", shape=hexagon, '
            f'style=filled, fillcolor="#FFA500"];'
        )

    # Rule nodes
    for r in model.rules:
        rid = r["id"]
        rnid = _sanitize_id(rid)
        rclass = r.get("class", "inference")
        color = "#FF6B6B" if rclass == "safety_guard" else "#87CEEB"
        lines.append(
            f'    {rnid} [label="{rid}", shape=box, '
            f'style=filled, fillcolor="{color}"];'
        )

    lines.append("")

    # Edges
    for r in model.rules:
        rid = r["id"]
        rnid = _sanitize_id(rid)

        # Condition edges: fact → rule
        when = r.get("when", {})
        if isinstance(when, dict):
            for gk in ("all", "any", "not"):
                g = when.get(gk)
                if isinstance(g, list):
                    for cond in g:
                        if isinstance(cond, dict):
                            fact_name = cond.get("fact", "")
                            if fact_name:
                                fnid = _sanitize_id(fact_name)
                                op = cond.get("op", "==")
                                val = cond.get("value", "")
                                lines.append(
                                    f'    {fnid} -> {rnid} '
                                    f'[label="{op} {val}"];'
                                )

        # Action edges
        then = r.get("then", {})
        if isinstance(then, dict):
            action_name = then.get("action")
            if action_name:
                anid = _sanitize_id(action_name)
                lines.append(f"    {rnid} -> {anid};")

            # Compute edges
            compute = then.get("compute", [])
            if isinstance(compute, list):
                for expr in compute:
                    if isinstance(expr, dict):
                        target = expr.get("target", "")
                        if target:
                            tnid = _sanitize_id(target)
                            lines.append(
                                f'    {rnid} -> {tnid} '
                                f'[style=dashed, label="compute"];'
                            )

    lines.append("}")
    return "\n".join(lines) + "\n"
