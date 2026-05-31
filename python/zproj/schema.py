# SPDX-License-Identifier: MIT
"""JSON Schema loading and validation for ZRM models."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

import jsonschema

from .diagnostics import DiagnosticCollector

_SCHEMA_PATH = Path(__file__).resolve().parent.parent.parent / "schema" / "zrm.schema.json"


def load_schema() -> dict[str, Any]:
    """Load the ZRM JSON schema."""
    return json.loads(_SCHEMA_PATH.read_text(encoding="utf-8"))


def validate_schema(
    data: dict[str, Any],
    diag: DiagnosticCollector | None = None,
) -> list:
    """Validate model data against the ZRM JSON schema.

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
            "ZPROJ-E-SCHEMA",
            path_str,
            err.message,
        )

    return errors
