# SPDX-License-Identifier: MIT
"""CLI entry point for arbiterc — the ARB compiler."""

from __future__ import annotations

import sys
from pathlib import Path

import click

from . import __version__
from .compiler import CompileOptions, compile_model
from .diagnostics import DiagnosticCollector
from .evaluator import ArbiterEvaluator
from .parser import parse_model
from .schema import validate_schema
from .validator import validate_model


@click.group()
@click.version_option(version=__version__, prog_name="arbiterc")
def main() -> None:
    """arbiterc — ARB compiler for Zephyr Reasoning Models."""


@main.command()
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option("--strict", is_flag=True, help="Enable strict validation.")
@click.option("--safety-profile", type=str, default=None)
@click.option("--fail-on-warning", is_flag=True)
def validate(model: Path, strict: bool, safety_profile: str | None, fail_on_warning: bool) -> None:
    """Validate a .arb.yaml model file."""
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
@click.option(
    "--profile",
    type=click.Choice(["auto", "nano", "micro", "standard", "full"], case_sensitive=False),
    default="auto",
    help="Engine scaling profile. 'auto' defaults to standard when no board info.",
)
@click.option("--force-strings", is_flag=True, help="Force string emission even on nano/micro.")
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
    profile: str,
    force_strings: bool,
) -> None:
    """Compile a .arb.yaml model to C source or binary blob."""
    # Resolve profile: auto -> standard (no board info in CLI)
    resolved_profile = profile if profile != "auto" else "standard"

    # Profile-driven string stripping
    if resolved_profile in ("nano", "micro") and not force_strings:
        emit_trace_strings = False

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
        profile=resolved_profile,
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

    # Resource budget report (REQ-ARCH-033)
    if result.resource_report:
        click.echo(result.resource_report)


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


@main.command()
@click.argument("model", type=click.Path(exists=True, path_type=Path))
@click.option(
    "--facts",
    multiple=True,
    help="Set facts as key=value pairs (e.g. battery.voltage_mv=3300).",
)
@click.option(
    "--timestamps",
    multiple=True,
    help="Set fact timestamps as key=ms pairs (e.g. battery.voltage_mv=100).",
)
@click.option(
    "--snapshot-ts",
    type=int,
    default=0,
    help="Snapshot timestamp in ms (for staleness checks).",
)
@click.option("--json", "emit_json", is_flag=True, help="Output result as JSON.")
def eval(model: Path, facts: tuple[str, ...], timestamps: tuple[str, ...],
         snapshot_ts: int, emit_json: bool) -> None:
    """Evaluate a .arb.yaml model with given facts."""
    import json as json_mod

    diag = DiagnosticCollector()
    data = parse_model(model, diag)
    if data is None:
        click.echo(diag.format(), err=True)
        sys.exit(1)

    evaluator = ArbiterEvaluator(data)

    for kv in facts:
        if "=" not in kv:
            click.echo(f"Error: invalid fact '{kv}', expected key=value", err=True)
            sys.exit(1)
        key, val = kv.split("=", 1)
        try:
            evaluator.set_fact(key, val)
        except KeyError as e:
            click.echo(f"Error: {e}", err=True)
            sys.exit(1)

    for kv in timestamps:
        if "=" not in kv:
            click.echo(f"Error: invalid timestamp '{kv}', expected key=ms", err=True)
            sys.exit(1)
        key, val = kv.split("=", 1)
        try:
            evaluator.set_timestamp(key, int(val))
        except (KeyError, ValueError) as e:
            click.echo(f"Error: {e}", err=True)
            sys.exit(1)

    evaluator.set_snapshot_timestamp(snapshot_ts)
    result = evaluator.eval()

    if emit_json:
        click.echo(json_mod.dumps(result.to_dict(), indent=2))
    else:
        click.echo(f"Fired rules: {result.fired_rules}")
        if result.current_mode:
            click.echo(f"Mode: {result.current_mode}")
        if result.requested_actions:
            click.echo(f"Actions: {result.requested_actions}")
        if result.raised_faults:
            click.echo(f"Faults: {sorted(result.raised_faults)}")
        click.echo(f"Op count: {result.op_count}")


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
