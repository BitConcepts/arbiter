# SPDX-License-Identifier: MIT
"""JSON Schema loading and validation for ARB models."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import jsonschema

from .diagnostics import DiagnosticCollector

_SCHEMA_PATH = Path(__file__).resolve().parent.parent.parent / "schema" / "arb.schema.json"


def load_schema() -> dict[str, Any]:
    """Load the ARB JSON schema."""
    result: dict[str, Any] = json.loads(_SCHEMA_PATH.read_text(encoding="utf-8"))
    return result


def validate_schema(
    data: dict[str, Any],
    diag: DiagnosticCollector | None = None,
) -> list:
    """Validate model data against the ARB JSON schema.

    Returns list of jsonschema.ValidationError instances.
    """
    if diag is None:
        diag = DiagnosticCollector()

    schema = load_schema()
    validator = jsonschema.Draft202012Validator(schema)
    errors = list(validator.iter_errors(data))

    for err in errors:
        path_str = ".".join(str(p) for p in err.absolute_path) or "(root)"
        diag.error(
            "arbiter-E-SCHEMA",
            path_str,
            err.message,
        )

    return errors
