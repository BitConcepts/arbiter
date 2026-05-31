# Test Specification

## C Runtime Engine Tests

## TEST-001
- **Title**: Engine init and context management
- Covers: REQ-ARCH-001
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-002
- **Title**: Rule evaluation loop
- Covers: REQ-ARCH-002
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-003
- **Title**: Compute expression interpreter
- Covers: REQ-ARCH-003
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-004
- **Title**: Fact store with timestamps
- Covers: REQ-ARCH-004
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-005
- **Title**: Trace subsystem
- Covers: REQ-ARCH-005
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-006
- **Title**: Binary blob loading
- Covers: REQ-ARCH-006
- **File**: tests/unit/test_arbiter_blob/src/main.c
- **Status**: Implemented

## TEST-007
- **Title**: Action dispatcher
- Covers: REQ-ARCH-007
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## API and Header Tests

## TEST-008
- **Title**: Public C API
- Covers: REQ-ARCH-008
- **File**: samples/battery_policy/src/main.c
- **Status**: Implemented

## TEST-009
- **Title**: Model data structures
- Covers: REQ-ARCH-009
- **File**: samples/battery_policy/src/main.c
- **Status**: Implemented

## TEST-010
- **Title**: Evaluation result structure
- Covers: REQ-ARCH-010
- **File**: samples/pid_controller/src/main.c
- **Status**: Implemented

## TEST-011
- **Title**: Trace API header
- Covers: REQ-ARCH-011
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-012
- **Title**: Version constants
- Covers: REQ-ARCH-012
- **File**: include/arbiter/arbiter_version.h
- **Status**: Implemented

## Zephyr Subsystem Tests

## TEST-013
- **Title**: Shell commands
- Covers: REQ-ARCH-013
- **File**: subsys/arbiter/arbiter_shell.c
- **Status**: Implemented

## TEST-014
- **Title**: Runtime evaluation thread
- Covers: REQ-ARCH-014
- **File**: subsys/arbiter/arbiter_runtime.c
- **Status**: Implemented

## TEST-015
- **Title**: Watchdog supervision
- Covers: REQ-ARCH-015
- **File**: subsys/arbiter/arbiter_watchdog.c
- **Status**: Implemented

## TEST-016
- **Title**: Kconfig feature flags
- Covers: REQ-ARCH-016
- **File**: subsys/arbiter/Kconfig
- **Status**: Implemented

## Python Compiler Tests

## TEST-017
- **Title**: arbiterc CLI
- Covers: REQ-ARCH-017
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-018
- **Title**: YAML model parser
- Covers: REQ-ARCH-018
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-019
- **Title**: Model validator
- Covers: REQ-ARCH-019
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-020
- **Title**: JSON Schema validation
- Covers: REQ-ARCH-020
- **File**: tests/python/test_schema.py
- **Status**: Implemented

## TEST-021
- **Title**: Canonical ordering
- Covers: REQ-ARCH-021
- **File**: tests/python/test_canonical.py
- **Status**: Implemented

## TEST-022
- **Title**: C code emitter
- Covers: REQ-ARCH-022
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-023
- **Title**: Blob emitter
- Covers: REQ-ARCH-023
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-024
- **Title**: Documentation emitter
- Covers: REQ-ARCH-024
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-025
- **Title**: Compiler pipeline
- Covers: REQ-ARCH-025
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-026
- **Title**: Diagnostics reporting
- Covers: REQ-ARCH-026
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-027
- **Title**: ARB model format schema
- Covers: REQ-ARCH-027
- **File**: tests/python/test_schema.py
- **Status**: Implemented

## TEST-028
- **Title**: Reusable include library
- Covers: REQ-ARCH-028
- **File**: samples/kalman_filter/models/kalman.arb.yaml
- **Status**: Implemented

## Safety Requirement Tests

## TEST-029
- **Title**: Deterministic evaluation order
- Covers: REQ-SAFETY-001
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-030
- **Title**: No dynamic allocation
- Covers: REQ-SAFETY-002
- **File**: lib/arbiter_engine.c
- **Status**: Implemented

## TEST-031
- **Title**: Bounded execution
- Covers: REQ-SAFETY-003
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-032
- **Title**: Overflow protection
- Covers: REQ-SAFETY-004
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-033
- **Title**: Model integrity hash
- Covers: REQ-SAFETY-005
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-034
- **Title**: Staleness detection
- Covers: REQ-SAFETY-006
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-035
- **Title**: Division by zero handling
- Covers: REQ-SAFETY-007
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## Build Requirement Tests

## TEST-036
- **Title**: Zephyr module build
- Covers: REQ-BUILD-001
- **File**: samples/battery_policy/testcase.yaml
- **Status**: Implemented
