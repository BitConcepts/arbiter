# Test Specification

## C Runtime Engine Tests

## TEST-001
- title: Engine init and context management
- Covers: REQ-ARCH-001
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-002
- title: Rule evaluation loop
- Covers: REQ-ARCH-002
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-003
- title: Compute expression interpreter
- Covers: REQ-ARCH-003
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-004
- title: Fact store with timestamps
- Covers: REQ-ARCH-004
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-005
- title: Trace subsystem
- Covers: REQ-ARCH-005
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-006
- title: Binary blob loading
- Covers: REQ-ARCH-006
- **File**: tests/unit/test_arbiter_blob/src/main.c
- **Status**: Implemented

## TEST-007
- title: Action dispatcher
- Covers: REQ-ARCH-007
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## API and Header Tests

## TEST-008
- title: Public C API
- Covers: REQ-ARCH-008
- **File**: samples/battery_policy/src/main.c
- **Status**: Implemented

## TEST-009
- title: Model data structures
- Covers: REQ-ARCH-009
- **File**: samples/battery_policy/src/main.c
- **Status**: Implemented

## TEST-010
- title: Evaluation result structure
- Covers: REQ-ARCH-010
- **File**: samples/pid_controller/src/main.c
- **Status**: Implemented

## TEST-011
- title: Trace API header
- Covers: REQ-ARCH-011
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-012
- title: Version constants
- Covers: REQ-ARCH-012
- **File**: include/arbiter/arbiter_version.h
- **Status**: Implemented

## Zephyr Subsystem Tests

## TEST-013
- title: Shell commands
- Covers: REQ-ARCH-013
- **File**: subsys/arbiter/arbiter_shell.c
- **Status**: Implemented

## TEST-014
- title: Runtime evaluation thread
- Covers: REQ-ARCH-014
- **File**: subsys/arbiter/arbiter_runtime.c
- **Status**: Implemented

## TEST-015
- title: Watchdog supervision
- Covers: REQ-ARCH-015
- **File**: subsys/arbiter/arbiter_watchdog.c
- **Status**: Implemented

## TEST-016
- title: Kconfig feature flags
- Covers: REQ-ARCH-016
- **File**: subsys/arbiter/Kconfig
- **Status**: Implemented

## Python Compiler Tests

## TEST-017
- title: arbiterc CLI
- Covers: REQ-ARCH-017
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-018
- title: YAML model parser
- Covers: REQ-ARCH-018
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-019
- title: Model validator
- Covers: REQ-ARCH-019
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-020
- title: JSON Schema validation
- Covers: REQ-ARCH-020
- **File**: tests/python/test_schema.py
- **Status**: Implemented

## TEST-021
- title: Canonical ordering
- Covers: REQ-ARCH-021
- **File**: tests/python/test_canonical.py
- **Status**: Implemented

## TEST-022
- title: C code emitter
- Covers: REQ-ARCH-022
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-023
- title: Blob emitter
- Covers: REQ-ARCH-023
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-024
- title: Documentation emitter
- Covers: REQ-ARCH-024
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-025
- title: Compiler pipeline
- Covers: REQ-ARCH-025
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-026
- title: Diagnostics reporting
- Covers: REQ-ARCH-026
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-027
- title: ARB model format schema
- Covers: REQ-ARCH-027
- **File**: tests/python/test_schema.py
- **Status**: Implemented

## TEST-028
- title: Reusable include library
- Covers: REQ-ARCH-028
- **File**: samples/kalman_filter/models/kalman.arb.yaml
- **Status**: Implemented

## Safety Requirement Tests

## TEST-029
- title: Deterministic evaluation order
- Covers: REQ-SAFETY-001
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-030
- title: No dynamic allocation
- Covers: REQ-SAFETY-002
- **File**: lib/arbiter_engine.c
- **Status**: Implemented

## TEST-031
- title: Bounded execution
- Covers: REQ-SAFETY-003
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-032
- title: Overflow protection
- Covers: REQ-SAFETY-004
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-033
- title: Model integrity hash
- Covers: REQ-SAFETY-005
- **File**: tests/python/test_compiler.py
- **Status**: Implemented

## TEST-034
- title: Staleness detection
- Covers: REQ-SAFETY-006
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## TEST-035
- title: Division by zero handling
- Covers: REQ-SAFETY-007
- **File**: tests/unit/test_arbiter_eval/src/main.c
- **Status**: Implemented

## Build Requirement Tests

## TEST-036
- title: Zephyr module build
- Covers: REQ-BUILD-001
- **File**: samples/battery_policy/testcase.yaml
- **Status**: Implemented

## Engine Scaling & Acceleration Tests

## TEST-037
- title: clang-tidy CI gate
- Covers: REQ-BUILD-002
- **File**: .github/workflows/ci.yml (clang-tidy job)
- **Status**: Implemented

## TEST-038
- title: Engine scaling profiles with auto-detection
- Covers: REQ-ARCH-029
- **File**: subsys/arbiter/Kconfig, tests/python/test_emit_c.py
- **Status**: Implemented

## TEST-039
- title: HW-accelerated expression evaluation
- Covers: REQ-ARCH-030
- **File**: lib/arbiter_accel.c, tests/benchmarks/
- **Status**: Implemented

## TEST-040
- title: Optional assembly hot paths
- Covers: REQ-ARCH-031
- **File**: lib/arch/ (future — benchmark-gated)
- **Status**: Pending

## TEST-041
- title: FPGA offload interface
- Covers: REQ-ARCH-032
- **File**: include/arbiter/arbiter_offload.h
- **Status**: Implemented

## TEST-042
- title: Model complexity analysis
- Covers: REQ-ARCH-033
- **File**: tests/python/test_emit_c.py
- **Status**: Implemented

