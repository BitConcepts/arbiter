#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Parse benchmark timing from Twister log output.

Extracts ns/tick for both hand-coded and engine implementations from
PID and Kalman benchmark logs.  Prints a summary table suitable for
CI log inspection and future regression gating.

Usage::

    python tools/parse_benchmark.py twister-out/benchmarks/

Reads all handler.log files under the given directory tree.
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class BenchmarkResult:
    """Timing result from one benchmark variant."""

    benchmark: str
    variant: str
    ns_per_tick: int


# Patterns matching the LOG_INF lines from benchmark main.c files.
# PID:    "  Total: 123456 ns  (42 ns/tick)"   or   "  Total: 123 ms  (42 ns/tick)"
# Kalman: "  Total: 123 ms  (42 ns/tick)"
_NS_PER_TICK_RE = re.compile(r"\((\d+)\s*ns/tick\)")

# Section headers that identify which variant we're parsing.
_HAND_CODED_RE = re.compile(r"---\s*Hand-coded\s+(\w+)\s*---")
_ENGINE_RE = re.compile(r"---\s*arbiter\s+Engine\s+(\w+)\s*---")


def parse_log(log_text: str, benchmark_name: str) -> list[BenchmarkResult]:
    """Parse a single benchmark log and extract timing results."""
    results: list[BenchmarkResult] = []
    current_variant: str | None = None
    current_algo: str | None = None

    for line in log_text.splitlines():
        # Check for section headers
        m = _HAND_CODED_RE.search(line)
        if m:
            current_algo = m.group(1)
            current_variant = "hand-coded"
            continue

        m = _ENGINE_RE.search(line)
        if m:
            current_algo = m.group(1)
            current_variant = "engine"
            continue

        # Check for ns/tick value
        m = _NS_PER_TICK_RE.search(line)
        if m and current_variant and current_algo:
            ns = int(m.group(1))
            results.append(BenchmarkResult(
                benchmark=f"{benchmark_name}/{current_algo}",
                variant=current_variant,
                ns_per_tick=ns,
            ))
            current_variant = None
            current_algo = None

    return results


def find_and_parse(base_dir: Path) -> list[BenchmarkResult]:
    """Find all handler.log files under base_dir and parse them."""
    results: list[BenchmarkResult] = []

    for log_path in sorted(base_dir.rglob("handler.log")):
        # Infer benchmark name from path: .../pid_benchmark/... or .../kalman_benchmark/...
        parts = log_path.parts
        bench_name = "unknown"
        for part in parts:
            if "pid" in part.lower():
                bench_name = "pid"
                break
            if "kalman" in part.lower():
                bench_name = "kalman"
                break

        log_text = log_path.read_text(encoding="utf-8", errors="replace")
        results.extend(parse_log(log_text, bench_name))

    return results


def print_summary(results: list[BenchmarkResult]) -> None:
    """Print a formatted summary table of benchmark results."""
    if not results:
        print("No benchmark timing data found.")
        return

    # Header
    print()
    print("=" * 60)
    print("  Benchmark Performance Summary")
    print("=" * 60)
    print(f"  {'Benchmark':<25} {'Variant':<15} {'ns/tick':>10}")
    print("-" * 60)

    for r in results:
        print(f"  {r.benchmark:<25} {r.variant:<15} {r.ns_per_tick:>10}")

    print("-" * 60)

    # Compute overhead for each benchmark that has both variants
    by_bench: dict[str, dict[str, int]] = {}
    for r in results:
        by_bench.setdefault(r.benchmark, {})[r.variant] = r.ns_per_tick

    for bench, variants in sorted(by_bench.items()):
        hand = variants.get("hand-coded")
        engine = variants.get("engine")
        if hand and engine and hand > 0:
            overhead_pct = ((engine - hand) * 100) // hand
            print(f"  {bench:<25} overhead: {overhead_pct}%")

    print("=" * 60)
    print()


def main() -> int:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <twister-out-dir>", file=sys.stderr)
        return 1

    base_dir = Path(sys.argv[1])
    if not base_dir.exists():
        print(f"Error: directory not found: {base_dir}", file=sys.stderr)
        return 1

    results = find_and_parse(base_dir)
    print_summary(results)
    return 0


if __name__ == "__main__":
    sys.exit(main())
