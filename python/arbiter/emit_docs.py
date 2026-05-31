# SPDX-License-Identifier: MIT
"""Documentation emitter for ARB models."""

from __future__ import annotations

from .canonical import CanonicalModel


def emit_docs(model: CanonicalModel) -> str:
    """Generate Markdown documentation for a canonical ARB model."""
    lines = [
        f"# {model.name}",
        "",
        f"**ARB Version:** {model.arb_version}  ",
        f"**Model Hash:** `{model.model_hash[:16]}...`  ",
        f"**Compiler:** arbiterc {model.compiler_version}",
        "",
    ]

    # Facts
    lines.append("## Facts")
    lines.append("")
    lines.append("| # | ID | Type | Range | Stale (ms) | Safety |")
    lines.append("|---|-----|------|-------|------------|--------|")
    for i, f in enumerate(model.facts):
        rng = f.get("range", "—")
        if isinstance(rng, list) and len(rng) == 2:
            rng = f"[{rng[0]}, {rng[1]}]"
        safety = "✓" if f.get("safety_relevant") else ""
        lines.append(
            f"| {i} | `{f['id']}` | {f.get('type', '?')} | {rng} "
            f"| {f.get('stale_after_ms', '—')} | {safety} |"
        )
    lines.append("")

    # Modes
    if model.modes:
        lines.append("## Modes")
        lines.append("")
        for i, m in enumerate(model.modes):
            if isinstance(m, dict) and "id" in m:
                lines.append(f"- `{m['id']}` (index {i})")
        lines.append("")

    # Rules
    lines.append("## Rules")
    lines.append("")
    for i, r in enumerate(model.rules):
        then = r.get("then", {})
        explanation = then.get("explanation", "") if isinstance(then, dict) else ""
        lines.append(f"### {r['id']}")
        lines.append(f"- **Class:** {r.get('class', '?')}")
        if explanation:
            lines.append(f"- **Explanation:** {explanation}")
        lines.append("")

    # Actions
    if model.actions:
        lines.append("## Actions")
        lines.append("")
        for i, a in enumerate(model.actions):
            lines.append(f"- `{a['id']}` — type: {a.get('type', '?')}")
        lines.append("")

    return "\n".join(lines)
