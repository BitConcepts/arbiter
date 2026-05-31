# Architecture — zproj

## Overview
zproj is a deterministic reasoning and safety-policy engine for Zephyr RTOS.
It provides a YAML-based model language (ZRM), a Python compiler (`zprojc`),
a C runtime engine, and Zephyr subsystem integration.

- **Languages**: C (runtime), Python (compiler), YAML (models)
- **Target RTOS**: Zephyr
- **License**: MIT

## System Architecture

```
 Developer:  *.zrm.yaml --> zprojc compile --> generated .c/.h
                               |
                      zprojc validate / inspect / docs

 Python Compiler (zprojc)        C Runtime Engine
  - Parser                        - zproj_eval()
  - Schema validator               - Expression evaluator (15 ops)
  - Canonicalizer                  - Condition evaluator
  - C code emitter                 - Fact store + snapshots
  - Blob emitter                   - Action dispatcher
  - Docs emitter                   - Trace subsystem

                        Zephyr Subsystem
                         - Shell commands
                         - Runtime thread
                         - Watchdog integration
                         - Kconfig feature flags
```

## Component Breakdown

### C Runtime (`lib/`)
- **zproj_engine.c**: Init, context management, fact accessors (REQ-ARCH-001)
- **zproj_eval.c**: Rule evaluation loop, condition matching (REQ-ARCH-002)
- **zproj_eval.c (expressions)**: Compute expression interpreter — 15 opcodes
  including add, sub, mul, div, scale, accumulate, clamp, abs, min, max,
  shift, assign. Uses 64-bit widening for overflow protection. (REQ-ARCH-003)
- **zproj_fact_store.c**: Fact value storage, timestamp tracking, range validation (REQ-ARCH-004)
- **zproj_trace.c**: Rule-firing trace log (REQ-ARCH-005)
- **zproj_blob.c**: Binary model loading stub (REQ-ARCH-006)
- **zproj_action.c**: Action callback dispatcher (REQ-ARCH-007)

### Public API (`include/zproj/`)
- **zproj.h**: Top-level API — init, set/get facts, snapshot, eval (REQ-ARCH-008)
- **zproj_model.h**: Compiled model structures — facts, rules, conditions,
  expressions, actions. All const data in .rodata. (REQ-ARCH-009)
- **zproj_result.h**: Evaluation result — fired rules, mode, action list (REQ-ARCH-010)
- **zproj_trace.h**: Trace API (REQ-ARCH-011)
- **zproj_version.h**: Version constants (REQ-ARCH-012)

### Zephyr Subsystem (`subsys/zproj/`)
- **zproj_shell.c**: Shell commands for live fact/rule inspection (REQ-ARCH-013)
- **zproj_runtime.c**: Periodic evaluation thread (REQ-ARCH-014)
- **zproj_watchdog.c**: Hardware watchdog supervision (REQ-ARCH-015)
- **Kconfig**: Feature flags — ZPROJ, ZPROJ_TRACE, ZPROJ_SHELL,
  ZPROJ_WATCHDOG, ZPROJ_MAX_FACTS, etc. (REQ-ARCH-016)

### Python Compiler (`python/zproj/`)
- **cli.py**: `zprojc` Click CLI — compile, validate, inspect, docs (REQ-ARCH-017)
- **parser.py**: YAML model parser with include resolution (REQ-ARCH-018)
- **validator.py**: Rule consistency and safety checks (REQ-ARCH-019)
- **schema.py**: JSON Schema validation against `schema/zrm.schema.json` (REQ-ARCH-020)
- **canonical.py**: Canonical ordering — facts alphabetical, safety_guard first (REQ-ARCH-021)
- **emit_c.py**: C code generation — const model tables (REQ-ARCH-022)
- **emit_blob.py**: Binary blob emission (REQ-ARCH-023)
- **emit_docs.py**: Markdown documentation generation (REQ-ARCH-024)
- **compiler.py**: Pipeline orchestrator (REQ-ARCH-025)
- **diagnostics.py**: Error/warning reporting (REQ-ARCH-026)

### ZRM Model Format
- YAML-based declarative models with `facts`, `rules`, `modes`, `actions` (REQ-ARCH-027)
- Rules have `class` (safety_guard, inference, obligation, etc.)
- Compute expressions in `then.compute` blocks
- Include mechanism for reusable fragments (`lib/zrm/`)
- Schema: `schema/zrm.schema.json`

### Reusable Include Library (`lib/zrm/`)
- sensor_health, estop, safety_common, network_common fragments (REQ-ARCH-028)

## Data Flow
1. **Compile time**: `.zrm.yaml` → `zprojc compile` → `generated_model.c` + `generated_model.h`
2. **Build time**: Generated C compiled into Zephyr application alongside engine library
3. **Runtime**: App writes facts → `zproj_snapshot_begin()` → `zproj_eval()` → result with fired rules, mode, actions → app reads outputs and dispatches callbacks

## Safety Architecture
- Safety guards evaluated first (deterministic priority) (REQ-SAFETY-001)
- No dynamic allocation in engine (REQ-SAFETY-002)
- Bounded execution — loops bounded by model table sizes (REQ-SAFETY-003)
- 64-bit widening in expressions prevents overflow (REQ-SAFETY-004)
- Model SHA-256 hash for integrity verification (REQ-SAFETY-005)
- Staleness detection on fact timestamps (REQ-SAFETY-006)
- Division by zero returns 0 (safe default) (REQ-SAFETY-007)

## Test Architecture
- **C unit tests**: `tests/zephyr/` — ztest-based, covers eval, expressions, blob loading
- **Python tests**: `tests/python/` — pytest, covers compiler, validator, schema, canonicalization
- **Benchmarks**: `tests/benchmarks/` — PID and Kalman engine vs hand-coded C
- **Golden vectors**: `tests/vectors/` — deterministic test vectors for regression
- **Sample builds**: 17 samples, each with `testcase.yaml` for Twister integration

## Build System
- **C**: CMake + Zephyr module system (`zephyr/module.yml`) (REQ-BUILD-001)
- **Python**: pyproject.toml with click CLI entry point
- **CI**: GitHub Actions — lint (ruff), typecheck (mypy), test (pytest), build (west)
