# SPDX-License-Identifier: MIT
"""YAML parser for .arb.yaml model files."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import yaml

from .diagnostics import DiagnosticCollector


def parse_model(path: Path, diag: DiagnosticCollector | None = None) -> dict[str, Any] | None:
    """Parse a .arb.yaml file and return the model dict.

    Returns None on parse failure.
    """
    if diag is None:
        diag = DiagnosticCollector()

    if not path.name.endswith(".arb.yaml"):
        diag.warning(
            "arbiter-W-FILE-EXT",
            str(path),
            f"File does not use .arb.yaml extension: {path.name}",
            "Rename to <name>.arb.yaml",
        )

    if not path.exists():
        diag.error("arbiter-E-FILE-NOT-FOUND", str(path), f"File not found: {path}")
        return None

    try:
        text = path.read_text(encoding="utf-8")
    except OSError as e:
        diag.error("arbiter-E-FILE-READ", str(path), f"Cannot read file: {e}")
        return None

    try:
        data = yaml.safe_load(text)
    except yaml.YAMLError as e:
        diag.error("arbiter-E-YAML-PARSE", str(path), f"YAML parse error: {e}")
        return None

    if not isinstance(data, dict):
        diag.error(
            "arbiter-E-YAML-ROOT",
            str(path),
            "Root element must be a mapping",
        )
        return None

    return data
