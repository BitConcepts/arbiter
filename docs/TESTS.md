# Test Specification

## Python Compiler Tests

## TEST-001
- **Title**: Canonical ordering
- **File**: tests/python/test_canonical.py
- **Covers**: REQ-ARCH-021
- **Status**: Implemented

## TEST-002
- **Title**: Compiler pipeline
- **File**: tests/python/test_compiler.py
- **Covers**: REQ-ARCH-025, REQ-ARCH-022, REQ-ARCH-023, REQ-ARCH-024
- **Status**: Implemented

## TEST-003
- **Title**: Golden vector regression
- **File**: tests/python/test_golden_vectors.py
- **Covers**: REQ-ARCH-025
- **Status**: Implemented

## TEST-004
- **Title**: JSON Schema validation
- **File**: tests/python/test_schema.py
- **Covers**: REQ-ARCH-020, REQ-ARCH-027
- **Status**: Implemented

## C Runtime Tests

## TEST-005
- **Title**: Blob loading test
- **File**: tests/unit/test_ARBITER_blob/src/main.c
- **Covers**: REQ-ARCH-006
- **Status**: Implemented
- **Harness**: ztest (native_sim)

## TEST-006
- **Title**: Evaluation engine test
- **File**: tests/unit/test_ARBITER_eval/src/main.c
- **Covers**: REQ-ARCH-001, REQ-ARCH-002, REQ-ARCH-003, REQ-ARCH-004, REQ-ARCH-007, REQ-SAFETY-001, REQ-SAFETY-003, REQ-SAFETY-004, REQ-SAFETY-006, REQ-SAFETY-007
- **Status**: Implemented
- **Harness**: ztest (native_sim)

## Benchmark Tests

## TEST-007
- **Title**: PID benchmark — engine vs hand-coded
- **File**: tests/benchmarks/pid_benchmark/src/main.c
- **Covers**: REQ-ARCH-002, REQ-ARCH-003
- **Status**: Implemented
- **Harness**: console (native_sim)

## TEST-008
- **Title**: Kalman benchmark — engine vs hand-coded
- **File**: tests/benchmarks/kalman_benchmark/src/main.c
- **Covers**: REQ-ARCH-002, REQ-ARCH-003
- **Status**: Implemented
- **Harness**: console (native_sim)

## Sample Build Tests

## TEST-009
- **Title**: Sample build validation (17 samples)
- **File**: samples/*/testcase.yaml
- **Covers**: REQ-BUILD-001, REQ-ARCH-008, REQ-ARCH-009, REQ-ARCH-010, REQ-ARCH-027
- **Status**: Implemented
- **Harness**: Twister build_only

## Integration Tests

## TEST-010
- **Title**: Python package install and pytest
- **Type**: integration
- **Covers**: REQ-BUILD-001, REQ-ARCH-017, REQ-ARCH-018, REQ-ARCH-019
- **Status**: Implemented

## Zephyr Subsystem Tests

## TEST-011
- **Title**: Shell command inspection
- **File**: tests/unit/test_ARBITER_eval/src/main.c
- **Covers**: REQ-ARCH-013, REQ-ARCH-016
- **Status**: Implemented
- **Note**: Shell tested indirectly via eval test; Kconfig validated by build

## TEST-012
- **Title**: Runtime thread and watchdog
- **File**: subsys/arbiter/ARBITER_runtime.c
- **Covers**: REQ-ARCH-014, REQ-ARCH-015
- **Status**: Implemented
- **Note**: Verified via sample builds with CONFIG_ARBITER=y

## API and Header Tests

## TEST-013
- **Title**: Trace API validation
- **File**: tests/unit/test_ARBITER_eval/src/main.c
- **Covers**: REQ-ARCH-005, REQ-ARCH-011, REQ-SAFETY-002
- **Status**: Implemented

## TEST-014
- **Title**: Version header and model integrity
- **File**: include/arbiter/ARBITER_version.h
- **Covers**: REQ-ARCH-012, REQ-SAFETY-005
- **Status**: Implemented
- **Note**: Version constants compile-time verified; hash in generated model

## Coverage Matrix

- REQ-ARCH-001..007: TEST-006 (eval engine)
- REQ-ARCH-008..010: TEST-009 (sample builds)
- REQ-ARCH-011..012: TEST-013, TEST-014
- REQ-ARCH-013..016: TEST-011, TEST-012
- REQ-ARCH-017..026: TEST-001..004, TEST-010 (Python tests)
- REQ-ARCH-027..028: TEST-004, TEST-009
- REQ-SAFETY-001..007: TEST-006, TEST-013, TEST-014
- REQ-BUILD-001: TEST-009, TEST-010

