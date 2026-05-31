# SPDX-License-Identifier: MIT
"""Main compiler pipeline for zprojc."""

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


@dataclass
class CompileResult:
    """Result of a compilation."""

    success: bool
    diagnostics: DiagnosticCollector
    model_hash: str = ""
    generated_files: list[str] = field(default_factory=list)
    canonical_model: CanonicalModel | None = None


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

    result = CompileResult(
        success=True,
        diagnostics=diag,
        model_hash=model.model_hash,
        canonical_model=model,
    )

    # Phase 11: Emit outputs
    if options.out_h:
        header = emit_c_header(model, emit_trace_strings=options.emit_trace_strings)
        options.out_h.write_text(header, encoding="utf-8")
        result.generated_files.append(str(options.out_h))

    if options.out_c:
        header_name = options.out_h.name if options.out_h else "zproj_model.h"
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
