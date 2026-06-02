# Requirements

## Architecture Requirements

## REQ-ARCH-001
- **Title**: Engine initialization and context management
- **Component**: lib/arbiter_engine.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL provide init, context management, and fact accessor functions.

## REQ-ARCH-002
- **Title**: Rule evaluation loop
- **Component**: lib/arbiter_eval.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL evaluate rules in deterministic canonical order with condition matching.

## REQ-ARCH-003
- **Title**: Compute expression interpreter
- **Component**: lib/arbiter_eval.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL support 15 expression opcodes (add, sub, mul, div, scale, accumulate, clamp, abs, min, max, shift_l, shift_r, assign, negate, mod) with 64-bit widening.

## REQ-ARCH-004
- **Title**: Fact store with timestamps
- **Component**: lib/arbiter_fact_store.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL store fact values with timestamps and validate ranges.

## REQ-ARCH-005
- **Title**: Trace subsystem
- **Component**: lib/arbiter_trace.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL record rule firings when tracing is enabled.

## REQ-ARCH-006
- **Title**: Binary blob loading
- **Component**: lib/arbiter_blob.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL support loading compiled models from binary blobs.

## REQ-ARCH-007
- **Title**: Action dispatcher
- **Component**: lib/arbiter_action.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The engine SHALL dispatch actions (callbacks) when rules fire.

## REQ-ARCH-008
- **Title**: Public C API
- **Component**: include/arbiter/arbiter.h
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: The module SHALL expose init, set/get facts, snapshot, and eval via a public header.

## REQ-ARCH-009
- **Title**: Model data structures
- **Component**: include/arbiter/arbiter_model.h
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: Compiled model tables SHALL be const data in .rodata.

## REQ-ARCH-010
- **Title**: Evaluation result structure
- **Component**: include/arbiter/arbiter_result.h
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: Evaluation SHALL produce a result with fired rules, mode, and action list.

## REQ-ARCH-011
- **Title**: Trace API
- **Component**: include/arbiter/arbiter_trace.h
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-012
- **Title**: Version constants
- **Component**: include/arbiter/arbiter_version.h
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-013
- **Title**: Shell commands
- **Component**: subsys/arbiter/arbiter_shell.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: Live fact and rule inspection via Zephyr shell.

## REQ-ARCH-014
- **Title**: Runtime evaluation thread
- **Component**: subsys/arbiter/arbiter_runtime.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-015
- **Title**: Watchdog supervision
- **Component**: subsys/arbiter/arbiter_watchdog.c
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-016
- **Title**: Kconfig feature flags
- **Component**: subsys/arbiter/Kconfig
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-017
- **Title**: arbiterc CLI
- **Component**: python/arbiter/cli.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: Click CLI with compile, validate, inspect, docs subcommands.

## REQ-ARCH-018
- **Title**: YAML model parser
- **Component**: python/arbiter/parser.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-019
- **Title**: Model validator
- **Component**: python/arbiter/validator.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-020
- **Title**: JSON Schema validation
- **Component**: python/arbiter/schema.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-021
- **Title**: Canonical ordering
- **Component**: python/arbiter/canonical.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-022
- **Title**: C code emitter
- **Component**: python/arbiter/emit_c.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-023
- **Title**: Blob emitter
- **Component**: python/arbiter/emit_blob.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-024
- **Title**: Documentation emitter
- **Component**: python/arbiter/emit_docs.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-025
- **Title**: Compiler pipeline
- **Component**: python/arbiter/compiler.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-026
- **Title**: Diagnostics reporting
- **Component**: python/arbiter/diagnostics.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## REQ-ARCH-027
- **Title**: ARB model format
- **Component**: schema/arb.schema.json
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model
- **Description**: YAML models with facts, rules, modes, actions, and compute expressions.

## REQ-ARCH-028
- **Title**: Reusable include library
- **Component**: lib/arb/
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine internals; integrator provides callbacks and model

## Safety Requirements

## REQ-SAFETY-001
- **Title**: Deterministic evaluation order
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: Safety guards SHALL be evaluated before all other rule classes.
- **Verification**: Unit test + model validator

## REQ-SAFETY-002
- **Title**: No dynamic allocation
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: The engine SHALL NOT call malloc/realloc/free.
- **Verification**: Code review, static analysis

## REQ-SAFETY-003
- **Title**: Bounded execution
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: All loops bounded by model table sizes. No recursion.
- **Verification**: Code review

## REQ-SAFETY-004
- **Title**: Overflow protection
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: Arithmetic SHALL use 64-bit intermediates.
- **Verification**: Unit tests with boundary values

## REQ-SAFETY-005
- **Title**: Model integrity hash
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: Models carry SHA-256 hash for verification.
- **Verification**: arbiterc compile output

## REQ-SAFETY-006
- **Title**: Staleness detection
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: Facts with stale_after_ms SHALL be detectable via stale operator.
- **Verification**: Unit test

## REQ-SAFETY-007
- **Title**: Division by zero handling
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine runtime; excludes application callbacks and model correctness
- **Description**: div/mod by zero SHALL return 0.
- **Verification**: Unit test

## Build Requirements

## REQ-BUILD-001
- **Title**: Zephyr module build
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Zephyr build system integration
- **Description**: Project builds as a Zephyr module via CMake and zephyr/module.yml.

## REQ-BUILD-002
- **Title**: clang-tidy CI gate
- **Status**: Accepted
- **Platform:** all
- **Boundary:** CI/CD pipeline
- **Description**: CI SHALL run clang-tidy on all engine C sources and headers, gating on zero diagnostics.
- **Verification**: CI job pass/fail

## Engine Scaling & Acceleration Requirements

## REQ-ARCH-029
- **Title**: Engine scaling profiles with auto-detection
- **Component**: subsys/arbiter/Kconfig, python/arbiter/cli.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Engine configuration; integrator selects or auto-detects profile
- **Description**: The compiler and runtime SHALL support four scaling profiles — nano, micro, standard, full — selectable via Kconfig ARBITER_PROFILE. Default is ARBITER_PROFILE_AUTO, which selects the profile automatically based on Zephyr target Kconfig symbols (CPU family, SRAM size). Users can override to any fixed profile. Profiles cap max facts, rules, trace depth, string inclusion, and index type to fit target resource budgets.
- **Verification**: Kconfig build test per profile, compiler --profile validation test

## REQ-ARCH-030
- **Title**: HW-accelerated expression evaluation
- **Component**: lib/arbiter_accel.c
- **Status**: Accepted
- **Platform:** ARM Cortex-M4+, RISC-V with P-extension
- **Boundary:** Engine internals; transparent to integrator API
- **Description**: When CONFIG_ARBITER_HW_ACCEL=y, the expression evaluator SHALL use platform-optimized intrinsics for batch operations (saturating arithmetic, SIMD min/max/clamp). Detection via Kconfig depends on CPU_CORTEX_M_HAS_DSP or RISCV_ISA_EXT_P. Zero overhead when disabled.
- **Verification**: Benchmark comparison with/without HW_ACCEL on Cortex-M4 target

## REQ-ARCH-031
- **Title**: Optional assembly hot paths
- **Component**: lib/arch/arm/arbiter_eval_arm.S, lib/arch/riscv/arbiter_eval_rv.S
- **Status**: Accepted
- **Platform:** ARM Cortex-M4/M7, RISC-V RV32
- **Boundary:** Engine internals; transparent to integrator API
- **Description**: Performance-critical inner loops (expression batch eval, condition scan) MAY provide hand-tuned assembly implementations gated behind CONFIG_ARBITER_ASM_OPT=y with depends on for specific CPU Kconfig symbols. Assembly is only added when benchmarks demonstrate >15% speedup.
- **Verification**: Benchmark comparison with/without ASM_OPT

## REQ-ARCH-032
- **Title**: FPGA offload interface (future)
- **Component**: include/arbiter/arbiter_offload.h
- **Status**: Accepted
- **Platform:** FPGA-equipped targets (Intel MAX10, Xilinx Zynq, Lattice iCE40/ECP5, Microsemi SmartFusion2)
- **Boundary:** Engine-to-hardware interface; board driver implements ops struct
- **Description**: The engine SHALL define an ARBITER_hw_offload_ops struct allowing a board-level driver to offload bulk condition evaluation and expression execution to FPGA fabric. This is a future-work interface; no implementation is shipped in v1.
- **Verification**: Header compiles, struct layout test

## REQ-ARCH-033
- **Title**: Model complexity analysis
- **Component**: python/arbiter/cli.py, python/arbiter/canonical.py
- **Status**: Accepted
- **Platform:** all
- **Boundary:** Compiler toolchain output
- **Description**: The arbiterc compiler SHALL emit a resource budget report showing estimated RAM (per profile), WCET op count (worst-case condition + expression evaluations), and .rodata size for the generated model tables.
- **Verification**: Python test verifying report output for sample models

