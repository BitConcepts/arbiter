# SPDX-License-Identifier: MIT
"""Main compiler pipeline for arbiterc."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from .canonical import CanonicalModel, canonicalize, to_canonical_json
from .diagnostics import DiagnosticCollector
from .emit_blob import emit_blob
from .emit_c import emit_c_header, emit_c_source
from .emit_docs import emit_docs
from .parser import parse_model
from .schema import validate_schema
from .validator import validate_model

# Profile resource budgets
PROFILE_LIMITS: dict[str, dict[str, int]] = {
    "nano":     {"max_facts": 8,   "max_rules": 8,   "max_trace": 0,   "index_bits": 8},
    "micro":    {"max_facts": 16,  "max_rules": 32,  "max_trace": 4,   "index_bits": 8},
    "standard": {"max_facts": 64,  "max_rules": 64,  "max_trace": 64,  "index_bits": 16},
    "full":     {"max_facts": 256, "max_rules": 256, "max_trace": 256, "index_bits": 16},
}


@dataclass
class CompileOptions:
    """Options for the compiler pipeline."""

    out_c: Path | None = None
    out_h: Path | None = None
    out_blob: Path | None = None
    out_docs: Path | None = None
    canonical_json: Path | None = None
    model_hash_out: Path | None = None
    strict: bool = False
    safety_profile: str | None = None
    fail_on_warning: bool = False
    emit_trace_strings: bool = True
    profile: str = "standard"


@dataclass
class CompileResult:
    """Result of a compilation."""

    success: bool
    diagnostics: DiagnosticCollector
    model_hash: str = ""
    generated_files: list[str] = field(default_factory=list)
    canonical_model: CanonicalModel | None = None
    resource_report: str = ""


def compile_model(path: Path, options: CompileOptions) -> CompileResult:
    """Run the full compiler pipeline: parse -> validate -> canonicalize -> emit."""
    diag = DiagnosticCollector()

    # Phase 1: Parse YAML
    data = parse_model(path, diag)
    if data is None or diag.has_errors():
        return CompileResult(success=False, diagnostics=diag)

    # Phase 2: JSON Schema validation
    validate_schema(data, diag)

    # Phases 3-9: Semantic validation
    validate_model(data, diag, strict=options.strict, safety_profile=options.safety_profile)

    if diag.has_errors():
        return CompileResult(success=False, diagnostics=diag)

    if options.fail_on_warning and diag.has_warnings():
        return CompileResult(success=False, diagnostics=diag)

    # Phase 10: Canonicalize
    model = canonicalize(data)

    # Phase 10b: Profile validation (REQ-ARCH-029)
    limits = PROFILE_LIMITS.get(options.profile, PROFILE_LIMITS["standard"])
    if model.max_facts > limits["max_facts"]:
        diag.error(
            "ARB-PROFILE-001", "compiler",
            f"Model has {model.max_facts} facts, exceeds {options.profile} profile "
            f"limit of {limits['max_facts']}.",
            suggestion="Use a larger profile (e.g. --profile micro or --profile standard).",
        )
        return CompileResult(success=False, diagnostics=diag)
    if model.max_rules > limits["max_rules"]:
        diag.error(
            "ARB-PROFILE-002", "compiler",
            f"Model has {model.max_rules} rules, exceeds {options.profile} profile "
            f"limit of {limits['max_rules']}.",
            suggestion="Use a larger profile (e.g. --profile micro or --profile standard).",
        )
        return CompileResult(success=False, diagnostics=diag)

    # Phase 10c: Resource budget report (REQ-ARCH-033)
    report = _build_resource_report(model, options.profile, limits)

    result = CompileResult(
        success=True,
        diagnostics=diag,
        model_hash=model.model_hash,
        canonical_model=model,
        resource_report=report,
    )

    # Phase 11: Emit outputs
    if options.out_h:
        header = emit_c_header(model, emit_trace_strings=options.emit_trace_strings)
        options.out_h.write_text(header, encoding="utf-8")
        result.generated_files.append(str(options.out_h))

    if options.out_c:
        header_name = options.out_h.name if options.out_h else "arbiter_model.h"
        source = emit_c_source(
            model, header_name=header_name,
            emit_trace_strings=options.emit_trace_strings,
        )
        options.out_c.write_text(source, encoding="utf-8")
        result.generated_files.append(str(options.out_c))

    if options.out_blob:
        blob = emit_blob(model)
        options.out_blob.write_bytes(blob)
        result.generated_files.append(str(options.out_blob))

    if options.out_docs:
        docs = emit_docs(model)
        options.out_docs.write_text(docs, encoding="utf-8")
        result.generated_files.append(str(options.out_docs))

    if options.canonical_json:
        cjson = to_canonical_json(model)
        options.canonical_json.write_text(cjson, encoding="utf-8")
        result.generated_files.append(str(options.canonical_json))

    if options.model_hash_out:
        options.model_hash_out.write_text(model.model_hash + "\n", encoding="utf-8")
        result.generated_files.append(str(options.model_hash_out))

    return result


def _build_resource_report(
    model: CanonicalModel, profile: str, limits: dict[str, int]
) -> str:
    """Build a human-readable resource budget report (REQ-ARCH-033)."""
    fact_pct = (model.max_facts * 100 // limits["max_facts"]) if limits["max_facts"] else 0
    rule_pct = (model.max_rules * 100 // limits["max_rules"]) if limits["max_rules"] else 0

    # RAM estimate: sizeof(ARBITER_fact_value) * facts + ctx overhead
    # ARBITER_fact_value is ~14 bytes (2x int32 + uint32 + 2 bools)
    fact_value_size = 14
    ctx_overhead = 32  # model ptr, snapshot struct, counters, etc.
    ram_est = model.max_facts * fact_value_size + ctx_overhead

    # .rodata estimate: fact_def + rule_def + condition_def + expr_def + action_def
    idx_size = 1 if limits["index_bits"] == 8 else 2
    has_strings = profile not in ("nano", "micro")
    ptr_size = 4  # assume 32-bit target
    fact_def_size = idx_size + 4 + 12 + idx_size + 1 + (ptr_size if has_strings else 0)
    rule_def_size = idx_size * 8 + 1 + (ptr_size * 2 if has_strings else 0)
    cond_def_size = idx_size + 4 + 4
    expr_def_size = idx_size * 3 + 12
    action_def_size = idx_size * 2 + 4 + ptr_size + idx_size + 1 + (ptr_size if has_strings else 0)

    rodata_est = (
        model.max_facts * fact_def_size
        + model.max_rules * rule_def_size
        + model.max_conditions * cond_def_size
        + model.max_expressions * expr_def_size
        + len(model.actions) * action_def_size
    )

    # WCET: worst case = all conditions + all expressions + rule count
    wcet_ops = model.max_conditions + model.max_expressions + model.max_rules

    lines = [
        f"  Profile: {profile}",
        f"  Facts:       {model.max_facts:>3} / {limits['max_facts']:<3}   ({fact_pct}%)",
        f"  Rules:       {model.max_rules:>3} / {limits['max_rules']:<3}   ({rule_pct}%)",
        f"  Conditions:  {model.max_conditions:>3}",
        f"  Expressions: {model.max_expressions:>3}",
        f"  RAM estimate:    ~{ram_est} B",
        f"  .rodata:         ~{rodata_est} B",
        f"  WCET ops:        {wcet_ops}",
    ]

    # Fit check against all profiles
    for pname, plimits in PROFILE_LIMITS.items():
        fits = (
            model.max_facts <= plimits["max_facts"]
            and model.max_rules <= plimits["max_rules"]
        )
        mark = "\u2713" if fits else "\u2717"
        reason = ""
        if not fits:
            if model.max_facts > plimits["max_facts"]:
                reason = f" ({model.max_facts} facts > {plimits['max_facts']} max)"
            elif model.max_rules > plimits["max_rules"]:
                reason = f" ({model.max_rules} rules > {plimits['max_rules']} max)"
        lines.append(f"  {mark} {pname}{reason}")

    return "\n".join(lines)
