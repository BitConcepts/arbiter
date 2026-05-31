# SPDX-License-Identifier: MIT
"""Compiler diagnostic types and collector."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Literal


@dataclass
class Diagnostic:
    """A single compiler diagnostic."""

    severity: Literal["error", "warning", "note"]
    code: str
    path: str
    message: str
    suggestion: str | None = None

    def __str__(self) -> str:
        prefix = {"error": "E", "warning": "W", "note": "N"}[self.severity]
        s = f"{self.code} [{prefix}] {self.path}: {self.message}"
        if self.suggestion:
            s += f"\n  suggestion: {self.suggestion}"
        return s


class DiagnosticCollector:
    """Collects diagnostics during compilation."""

    def __init__(self) -> None:
        self.diagnostics: list[Diagnostic] = []

    def error(
        self,
        code: str,
        path: str,
        message: str,
        suggestion: str | None = None,
    ) -> None:
        self.diagnostics.append(
            Diagnostic("error", code, path, message, suggestion)
        )

    def warning(
        self,
        code: str,
        path: str,
        message: str,
        suggestion: str | None = None,
    ) -> None:
        self.diagnostics.append(
            Diagnostic("warning", code, path, message, suggestion)
        )

    def note(
        self,
        code: str,
        path: str,
        message: str,
        suggestion: str | None = None,
    ) -> None:
        self.diagnostics.append(
            Diagnostic("note", code, path, message, suggestion)
        )

    def has_errors(self) -> bool:
        return any(d.severity == "error" for d in self.diagnostics)

    def has_warnings(self) -> bool:
        return any(d.severity == "warning" for d in self.diagnostics)

    def format(self) -> str:
        return "\n".join(str(d) for d in self.diagnostics)
