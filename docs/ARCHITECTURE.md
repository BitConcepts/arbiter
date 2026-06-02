# Architecture — arbiter

## Overview
arbiter is a deterministic reasoning and safety-policy engine for Zephyr RTOS.
It provides a YAML-based model language (ARB), a Python compiler (`arbiterc`),
a C runtime engine, and Zephyr subsystem integration.

- **Languages**: C (runtime), Python (compiler), YAML (models)
- **Target RTOS**: Zephyr
- **License**: MIT

## System Architecture

```
 Developer:  *.arb.yaml --> arbiterc compile --> generated .c/.h
                               |
                      arbiterc validate / inspect / docs

 Python Compiler (arbiterc)        C Runtime Engine
  - Parser                        - ARBITER_eval()
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
- **ARBITER_engine.c**: Init, context management, fact accessors (REQ-ARCH-001)
- **ARBITER_eval.c**: Rule evaluation loop, condition matching (REQ-ARCH-002)
- **ARBITER_eval.c (expressions)**: Compute expression interpreter — 15 opcodes
  including add, sub, mul, div, scale, accumulate, clamp, abs, min, max,
  shift, assign. Uses 64-bit widening for overflow protection. (REQ-ARCH-003)
- **ARBITER_fact_store.c**: Fact value storage, timestamp tracking, range validation (REQ-ARCH-004)
- **ARBITER_trace.c**: Rule-firing trace log (REQ-ARCH-005)
- **ARBITER_blob.c**: Binary model loading stub (REQ-ARCH-006)
- **ARBITER_action.c**: Action callback dispatcher (REQ-ARCH-007)

### Public API (`include/arbiter/`)
- **arbiter.h**: Top-level API — init, set/get facts, snapshot, eval (REQ-ARCH-008)
- **ARBITER_model.h**: Compiled model structures — facts, rules, conditions,
  expressions, actions. All const data in .rodata. (REQ-ARCH-009)
- **ARBITER_result.h**: Evaluation result — fired rules, mode, action list (REQ-ARCH-010)
- **ARBITER_trace.h**: Trace API (REQ-ARCH-011)
- **ARBITER_version.h**: Version constants (REQ-ARCH-012)

### Zephyr Subsystem (`subsys/arbiter/`)
- **ARBITER_shell.c**: Shell commands for live fact/rule inspection (REQ-ARCH-013)
- **ARBITER_runtime.c**: Periodic evaluation thread (REQ-ARCH-014)
- **ARBITER_watchdog.c**: Hardware watchdog supervision (REQ-ARCH-015)
- **Kconfig**: Feature flags — arbiter, ARBITER_TRACE, ARBITER_SHELL,
  ARBITER_WATCHDOG, ARBITER_MAX_FACTS, etc. (REQ-ARCH-016)

### Python Compiler (`python/arbiter/`)
- **cli.py**: `arbiterc` Click CLI — compile, validate, inspect, docs (REQ-ARCH-017)
- **parser.py**: YAML model parser with include resolution (REQ-ARCH-018)
- **validator.py**: Rule consistency and safety checks (REQ-ARCH-019)
- **schema.py**: JSON Schema validation against `schema/arb.schema.json` (REQ-ARCH-020)
- **canonical.py**: Canonical ordering — facts alphabetical, safety_guard first (REQ-ARCH-021)
- **emit_c.py**: C code generation — const model tables (REQ-ARCH-022)
- **emit_blob.py**: Binary blob emission (REQ-ARCH-023)
- **emit_docs.py**: Markdown documentation generation (REQ-ARCH-024)
- **compiler.py**: Pipeline orchestrator (REQ-ARCH-025)
- **diagnostics.py**: Error/warning reporting (REQ-ARCH-026)

### ARB Model Format
- YAML-based declarative models with `facts`, `rules`, `modes`, `actions` (REQ-ARCH-027)
- Rules have `class` (safety_guard, inference, obligation, etc.)
- Compute expressions in `then.compute` blocks
- Include mechanism for reusable fragments (`lib/arb/`)
- Schema: `schema/arb.schema.json`

### Reusable Include Library (`lib/arb/`)
- sensor_health, estop, safety_common, network_common fragments (REQ-ARCH-028)

## Data Flow
1. **Compile time**: `.arb.yaml` → `arbiterc compile` → `generated_model.c` + `generated_model.h`
2. **Build time**: Generated C compiled into Zephyr application alongside engine library
3. **Runtime**: App writes facts → `ARBITER_snapshot_begin()` → `ARBITER_eval()` → result with fired rules, mode, actions → app reads outputs and dispatches callbacks

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
- **CI**: GitHub Actions — lint (ruff), typecheck (mypy), test (pytest), clang-tidy (C), build (west) (REQ-BUILD-002)

## Engine Scaling Profiles (REQ-ARCH-029)

The engine supports four scaling profiles to match target hardware resources.
The default is **auto-detect** (`ARBITER_PROFILE_AUTO`), which selects the
profile based on Zephyr Kconfig symbols (CPU family, SRAM size). Users can
override to any fixed profile.

### Auto-Detection Logic

```
  Target CPU / SRAM          → Resolved Profile
  ─────────────────────────────────────────────
  Cortex-M0/M0+/M23, <16KB  → nano
  Cortex-M3, 16–64KB         → micro
  Cortex-M4/M7/M33, 64–512KB→ standard (default fallback)
  Cortex-A/R, x86, ≥512KB   → full
```

### Profile Resource Budgets

```
  Profile   Max    Max    Trace   Strings   Index    ~RAM
            Facts  Rules  Entries           Type
  ────────────────────────────────────────────────────────
  nano       8      8     0       no        uint8    200 B
  micro     16     32     4       no*       uint8    512 B
  standard  64     64     64      yes       uint16   ~3 KB
  full     256    256    256      yes       uint16   ~12 KB
```

*micro: strings disabled by default, overridable with `--force-strings`.

### Kconfig Integration

The Kconfig choice `ARBITER_PROFILE` provides `AUTO`, `NANO`, `MICRO`,
`STANDARD`, `FULL`. When AUTO is selected, hidden `ARBITER_RESOLVED_*`
symbols cascade based on `CPU_CORTEX_M0`, `SRAM_SIZE`, etc. Resolved
symbols drive `ARBITER_MAX_FACTS`, `ARBITER_MAX_TRACE_ENTRIES`, and
`ARBITER_STRINGS`.

### Compiler Profile Awareness

`arbiterc compile --profile auto|nano|micro|standard|full`:
- Validates models against profile limits (rejects oversized models).
- Strips string literals when profile is nano/micro.
- Emits `uint8_t` index typedefs for nano/micro.
- Prints resource budget report: RAM estimate, .rodata, WCET ops.

## HW Acceleration (REQ-ARCH-030)

When `CONFIG_ARBITER_HW_ACCEL=y`, the expression evaluator uses
platform-optimized intrinsics. Detection is automatic via Kconfig.

```
  CONFIG_ARBITER_HW_ACCEL
    depends on CPU_CORTEX_M_HAS_DSP || CPU_HAS_FPU || RISCV_ISA_EXT_P
```

### ARM Cortex-M4+ (CMSIS-DSP)
- Saturating add/sub: `__QADD`, `__QSUB` (single-cycle on M4+).
- Widening multiply: `__SMMUL`, `__SMMLA` (replaces 64-bit
  `(int64_t)a * b / scale` with single ARM instruction for SCALE/ACCUMULATE).
- SIMD min/max: conditional select via `__SEL` after `__USUB8`.

### RISC-V P-Extension
- Packed SIMD intrinsics via `__builtin_riscv_*` when available.
- Saturating arithmetic and clamp via `KADDW`, `KSUBW`.

### Fallback
When `CONFIG_ARBITER_HW_ACCEL=n` (default), the identical scalar C path
is compiled. Zero code-size or cycle overhead — the accelerated path is
fully `#if`-gated at compile time via `IS_ENABLED()`.

### Implementation
- **File**: `lib/arbiter_accel.c` (conditionally compiled)
- **Hook**: `arbiter_eval.c` calls `arbiter_eval_expressions_accel()` when
  HW_ACCEL is enabled, replacing the inner `eval_expression()` loop.

## Assembly Hot Paths (REQ-ARCH-031)

Optional hand-tuned assembly for the innermost hot loops, gated behind
`CONFIG_ARBITER_ASM_OPT=y` with CPU-specific `depends on`.

Assembly is **only added when benchmarks demonstrate >15% speedup** over
the C path. Candidates:

- `eval_condition_group` ALL loop: branch-free condition scan with IT
  blocks on Cortex-M.
- `eval_expression` SCALE/ACCUMULATE: widening multiply + saturate in a
  single `SMMLA`/`SMMUL` instruction on M4+.

```
  CONFIG_ARBITER_ASM_OPT
    depends on ARBITER_HW_ACCEL
    depends on CPU_CORTEX_M4 || CPU_CORTEX_M7 || CPU_CORTEX_M33
```

- **Files**: `lib/arch/arm/arbiter_eval_arm.S`,
  `lib/arch/riscv/arbiter_eval_rv.S`

## FPGA Offload Interface — Future Work (REQ-ARCH-032)

Defines a hardware abstraction for offloading bulk evaluation to FPGA
fabric. No implementation is shipped in v1.

### Interface

```c
struct ARBITER_hw_offload_ops {
    int (*eval_conditions)(const struct ARBITER_model *model,
                           const struct ARBITER_snapshot *snap,
                           uint32_t *results_bitmask);
    int (*eval_expressions)(const struct ARBITER_model *model,
                            struct ARBITER_snapshot *snap,
                            uint16_t start, uint16_t count);
    bool (*is_ready)(void);
};
```

The engine's eval loop checks `model->offload_ops != NULL` and delegates
condition evaluation and/or expression execution to the FPGA when the
offload driver reports ready.

### Target Zephyr FPGA Boards

- **Intel MAX10** (Nios II) — Quartus synthesis
- **Xilinx Zynq-7000** (Cortex-A9 + PL) — Vivado
- **Lattice iCE40** (RISC-V SoftCPU) — Yosys/nextpnr (open-source)
- **Lattice ECP5** (RISC-V via LiteX) — Yosys/nextpnr
- **Microsemi SmartFusion2** (Cortex-M3 + FPGA) — Libero SoC

### IP Block Concept

A Verilog/VHDL module implementing:
- Parallel comparator array for condition evaluation (single-cycle per
  condition vs. ~10 cycles in software).
- Pipelined ALU for expression execution.
- Shared SRAM interface: MCU writes snapshot, triggers FPGA, reads results.

### File
- `include/arbiter/arbiter_offload.h` — interface definition

## Model Complexity Analysis (REQ-ARCH-033)

`arbiterc compile` emits a resource budget report:

```
  Model: battery_policy
  Profile: standard (auto-detected)
  ──────────────────────────────────
  Facts:       12 / 64     (18%)
  Rules:        8 / 64     (12%)
  Conditions:  14 / ∞
  Expressions:  6 / ∞
  ──────────────────────────────────
  RAM estimate:    ~624 B  (fact_values + ctx)
  .rodata:         ~1.2 KB (model tables)
  WCET ops:        28      (worst-case: all conditions + all expressions)
  ──────────────────────────────────
  ✓ Model fits standard profile
  ✗ Model exceeds nano profile (12 facts > 8 max)
```

This enables integrators to right-size their target hardware and profile
selection at compile time.
