# SPDX-License-Identifier: MIT
"""CLI entry point for zprojc — the ZRM compiler."""

from __future__ import annotations

import sys
from pathlib import Path

import click

from . import __version__
from .compiler import CompileOptions, compile_model
from .diagnostics import DiagnosticCollector
from .parser import parse_model
from .schema import validate_schema
from .validator import validate_model


@click.group()
@click.version_option(version=__version__, prog_name="zprojc")
def main() -> None:
    """zprojc — ZRM compiler for Zephyr Reasoning Models."""


@main.command()
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option("--strict", is_flag=True, help="Enable strict validation.")
@click.option("--safety-profile", type=str, default=None)
@click.option("--fail-on-warning", is_flag=True)
def validate(model: Path, strict: bool, safety_profile: str | None, fail_on_warning: bool) -> None:
    """Validate a .zrm.yaml model file."""
    diag = DiagnosticCollector()

    data = parse_model(model, diag)
    if data is None:
        click.echo(diag.format(), err=True)
        sys.exit(1)

    validate_schema(data, diag)
    validate_model(data, diag, strict=strict, safety_profile=safety_profile)

    if diag.diagnostics:
        click.echo(diag.format(), err=True)

    if diag.has_errors() or (fail_on_warning and diag.has_warnings()):
        sys.exit(1)

    click.echo(f"✓ {model.name} is valid")


@main.command()
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option("--out-c", type=click.Path(path_type=Path), default=None)
@click.option("--out-h", type=click.Path(path_type=Path), default=None)
@click.option("--out-blob", type=click.Path(path_type=Path), default=None)
@click.option("--strict", is_flag=True)
@click.option("--safety-profile", type=str, default=None)
@click.option("--canonical-json", type=click.Path(path_type=Path), default=None)
@click.option("--model-hash-out", type=click.Path(path_type=Path), default=None)
@click.option("--fail-on-warning", is_flag=True)
@click.option("--emit-trace-strings/--no-trace-strings", default=True)
def compile(
    model: Path,
    out_c: Path | None,
    out_h: Path | None,
    out_blob: Path | None,
    strict: bool,
    safety_profile: str | None,
    canonical_json: Path | None,
    model_hash_out: Path | None,
    fail_on_warning: bool,
    emit_trace_strings: bool,
) -> None:
    """Compile a .zrm.yaml model to C source or binary blob."""
    options = CompileOptions(
        out_c=out_c,
        out_h=out_h,
        out_blob=out_blob,
        strict=strict,
        safety_profile=safety_profile,
        canonical_json=canonical_json,
        model_hash_out=model_hash_out,
        fail_on_warning=fail_on_warning,
        emit_trace_strings=emit_trace_strings,
    )

    result = compile_model(model, options)

    if result.diagnostics.diagnostics:
        click.echo(result.diagnostics.format(), err=True)

    if not result.success:
        sys.exit(1)

    click.echo(f"✓ Compiled {model.name}")
    click.echo(f"  Hash: {result.model_hash[:16]}...")
    for f in result.generated_files:
        click.echo(f"  → {f}")


@main.command("emit-docs")
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option("--out", type=click.Path(path_type=Path), required=True)
def emit_docs_cmd(model: Path, out: Path) -> None:
    """Generate Markdown documentation from a model."""
    options = CompileOptions(out_docs=out)
    result = compile_model(model, options)

    if not result.success:
        click.echo(result.diagnostics.format(), err=True)
        sys.exit(1)

    click.echo(f"✓ Documentation written to {out}")


@main.command("emit-tests")
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option("--out", type=click.Path(path_type=Path), required=True)
def emit_tests_cmd(model: Path, out: Path) -> None:
    """Generate test vectors from a model (placeholder)."""
    click.echo(f"⚠ emit-tests not yet implemented for {model.name}")
    sys.exit(0)


@main.command()
@click.argument("blob", type=click.Path(exists=True, path_type=Path))
def inspect(blob: Path) -> None:
    """Inspect a .zrmb binary blob."""
    import struct

    data = blob.read_bytes()
    if len(data) < 80:
        click.echo("Error: file too small to be a valid .zrmb blob", err=True)
        sys.exit(1)

    magic = data[:4]
    if magic != b"ZRMB":
        click.echo(f"Error: invalid magic: {magic!r}", err=True)
        sys.exit(1)

    version, flags = struct.unpack_from("<HH", data, 4)
    header_len, total_len = struct.unpack_from("<II", data, 8)
    model_hash = data[16:48].hex()
    schema_hash = data[48:80].hex()

    click.echo("Magic:       ZRMB")
    click.echo(f"Version:     {version}")
    click.echo(f"Flags:       0x{flags:04x}")
    click.echo(f"Header len:  {header_len}")
    click.echo(f"Total len:   {total_len}")
    click.echo(f"Model hash:  {model_hash[:32]}...")
    click.echo(f"Schema hash: {schema_hash[:32]}...")


if __name__ == "__main__":
    main()
