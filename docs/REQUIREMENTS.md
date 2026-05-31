# Requirements

## Architecture Requirements

## REQ-ARCH-001
- **Title**: Engine initialization and context management
- **Component**: lib/zproj_engine.c
- **Status**: Implemented
- **Description**: The engine SHALL provide init, context management, and fact accessor functions.

## REQ-ARCH-002
- **Title**: Rule evaluation loop
- **Component**: lib/zproj_eval.c
- **Status**: Implemented
- **Description**: The engine SHALL evaluate rules in deterministic canonical order with condition matching.

## REQ-ARCH-003
- **Title**: Compute expression interpreter
- **Component**: lib/zproj_eval.c
- **Status**: Implemented
- **Description**: The engine SHALL support 15 expression opcodes (add, sub, mul, div, scale, accumulate, clamp, abs, min, max, shift_l, shift_r, assign, negate, mod) with 64-bit widening.

## REQ-ARCH-004
- **Title**: Fact store with timestamps
- **Component**: lib/zproj_fact_store.c
- **Status**: Implemented
- **Description**: The engine SHALL store fact values with timestamps and validate ranges.

## REQ-ARCH-005
- **Title**: Trace subsystem
- **Component**: lib/zproj_trace.c
- **Status**: Implemented
- **Description**: The engine SHALL record rule firings when tracing is enabled.

## REQ-ARCH-006
- **Title**: Binary blob loading
- **Component**: lib/zproj_blob.c
- **Status**: Stub
- **Description**: The engine SHALL support loading compiled models from binary blobs.

## REQ-ARCH-007
- **Title**: Action dispatcher
- **Component**: lib/zproj_action.c
- **Status**: Implemented
- **Description**: The engine SHALL dispatch actions (callbacks) when rules fire.

## REQ-ARCH-008
- **Title**: Public C API
- **Component**: include/zproj/zproj.h
- **Status**: Implemented
- **Description**: The module SHALL expose init, set/get facts, snapshot, and eval via a public header.

## REQ-ARCH-009
- **Title**: Model data structures
- **Component**: include/zproj/zproj_model.h
- **Status**: Implemented
- **Description**: Compiled model tables SHALL be const data in .rodata.

## REQ-ARCH-010
- **Title**: Evaluation result structure
- **Component**: include/zproj/zproj_result.h
- **Status**: Implemented
- **Description**: Evaluation SHALL produce a result with fired rules, mode, and action list.

## REQ-ARCH-011
- **Title**: Trace API
- **Component**: include/zproj/zproj_trace.h
- **Status**: Implemented

## REQ-ARCH-012
- **Title**: Version constants
- **Component**: include/zproj/zproj_version.h
- **Status**: Implemented

## REQ-ARCH-013
- **Title**: Shell commands
- **Component**: subsys/zproj/zproj_shell.c
- **Status**: Implemented
- **Description**: Live fact and rule inspection via Zephyr shell.

## REQ-ARCH-014
- **Title**: Runtime evaluation thread
- **Component**: subsys/zproj/zproj_runtime.c
- **Status**: Implemented

## REQ-ARCH-015
- **Title**: Watchdog supervision
- **Component**: subsys/zproj/zproj_watchdog.c
- **Status**: Implemented

## REQ-ARCH-016
- **Title**: Kconfig feature flags
- **Component**: subsys/zproj/Kconfig
- **Status**: Implemented

## REQ-ARCH-017
- **Title**: zprojc CLI
- **Component**: python/zproj/cli.py
- **Status**: Implemented
- **Description**: Click CLI with compile, validate, inspect, docs subcommands.

## REQ-ARCH-018
- **Title**: YAML model parser
- **Component**: python/zproj/parser.py
- **Status**: Implemented

## REQ-ARCH-019
- **Title**: Model validator
- **Component**: python/zproj/validator.py
- **Status**: Implemented

## REQ-ARCH-020
- **Title**: JSON Schema validation
- **Component**: python/zproj/schema.py
- **Status**: Implemented

## REQ-ARCH-021
- **Title**: Canonical ordering
- **Component**: python/zproj/canonical.py
- **Status**: Implemented

## REQ-ARCH-022
- **Title**: C code emitter
- **Component**: python/zproj/emit_c.py
- **Status**: Implemented

## REQ-ARCH-023
- **Title**: Blob emitter
- **Component**: python/zproj/emit_blob.py
- **Status**: Implemented

## REQ-ARCH-024
- **Title**: Documentation emitter
- **Component**: python/zproj/emit_docs.py
- **Status**: Implemented

## REQ-ARCH-025
- **Title**: Compiler pipeline
- **Component**: python/zproj/compiler.py
- **Status**: Implemented

## REQ-ARCH-026
- **Title**: Diagnostics reporting
- **Component**: python/zproj/diagnostics.py
- **Status**: Implemented

## REQ-ARCH-027
- **Title**: ZRM model format
- **Component**: schema/zrm.schema.json
- **Status**: Implemented
- **Description**: YAML models with facts, rules, modes, actions, and compute expressions.

## REQ-ARCH-028
- **Title**: Reusable include library
- **Component**: lib/zrm/
- **Status**: Implemented

## Safety Requirements

## REQ-SAFETY-001
- **Title**: Deterministic evaluation order
- **Status**: Implemented
- **Description**: Safety guards SHALL be evaluated before all other rule classes.
- **Verification**: Unit test + model validator

## REQ-SAFETY-002
- **Title**: No dynamic allocation
- **Status**: Implemented
- **Description**: The engine SHALL NOT call malloc/realloc/free.
- **Verification**: Code review, static analysis

## REQ-SAFETY-003
- **Title**: Bounded execution
- **Status**: Implemented
- **Description**: All loops bounded by model table sizes. No recursion.
- **Verification**: Code review

## REQ-SAFETY-004
- **Title**: Overflow protection
- **Status**: Implemented
- **Description**: Arithmetic SHALL use 64-bit intermediates.
- **Verification**: Unit tests with boundary values

## REQ-SAFETY-005
- **Title**: Model integrity hash
- **Status**: Implemented
- **Description**: Models carry SHA-256 hash for verification.
- **Verification**: zprojc compile output

## REQ-SAFETY-006
- **Title**: Staleness detection
- **Status**: Implemented
- **Description**: Facts with stale_after_ms SHALL be detectable via stale operator.
- **Verification**: Unit test

## REQ-SAFETY-007
- **Title**: Division by zero handling
- **Status**: Implemented
- **Description**: div/mod by zero SHALL return 0.
- **Verification**: Unit test

## Build Requirements

## REQ-BUILD-001
- **Title**: Zephyr module build
- **Status**: Implemented
- **Description**: Project builds as a Zephyr module via CMake and zephyr/module.yml.

