# Arbiter Certification Roadmap

## Scope

This document describes the path to qualifying the Arbiter engine as a
**Safety Element out of Context (SEooC)** per IEC 61508-2, suitable for
integration into SIL 4 (IEC 61508) and ASIL D (ISO 26262) systems.

Arbiter itself is not a complete safety system. Certification of the
final system is the responsibility of the system integrator. Arbiter
provides a pre-qualified evidence package that integrators can reference
in their safety case.

## Current State

| Area | Status | Gap |
|------|--------|-----|
| Deterministic evaluation | ✓ Implemented | Needs formal proof / CBMC |
| No dynamic allocation | ✓ Implemented | Needs MISRA C verification |
| Bounded execution | ✓ Implemented | Needs CBMC proof |
| 64-bit overflow protection | ✓ Implemented | Needs MC/DC coverage |
| Division-by-zero handling | ✓ Implemented | Needs unit test + CBMC |
| Model integrity hash | ✓ Implemented | Needs hash verification test |
| Safety manual | ✓ Draft | Needs assessor review |
| Safety requirements | ✓ 7 defined (SR-001..SR-007) | Needs traceability matrix |
| MISRA C compliance | ✗ Not started | Major gap |
| MC/DC coverage | ✗ Not started | Major gap |
| Static analysis | ✗ Not started | Major gap |
| FMEA / FTA | ✗ Draft | Needs completion |
| Safety case (GSN) | ✗ Draft | Needs completion |
| Formal verification | ✗ Not started | Stretch goal |

## Phase 1: Code Quality & MISRA C (Months 1-6)

### 1.1 MISRA C:2012 Compliance
SIL 4 requires MISRA C:2012 compliance for all safety-relevant C code.

**Scope**: `lib/*.c`, `subsys/arbiter/*.c`, `include/arbiter/*.h`

**Actions**:
- Configure cppcheck with MISRA C:2012 addon (open-source starting point)
- Run Polyspace Bug Finder or PC-lint Plus for full MISRA coverage
- Document deviations with rationale per MISRA Compliance:2020

**Key MISRA rules for Arbiter**:
- Rule 21.3: No stdlib memory allocation (`malloc`, `free`) — already compliant
- Rule 17.2: No recursion — already compliant
- Rule 10.x: Integer type rules — needs review (64-bit widening casts)
- Rule 14.x: Control flow rules — needs review (switch/case completeness)
- Dir 4.1: Runtime error minimization — needs static analysis proof

### 1.2 Coding Standard Documentation
- Document the Arbiter coding standard (subset of Zephyr + MISRA)
- Create deviation records for any justified MISRA violations
- Generate MISRA compliance report

### 1.3 Static Analysis
- Run cppcheck, clang-tidy, and/or Polyspace on all engine code
- Prove absence of: null dereference, buffer overflow, integer overflow,
  division by zero, uninitialized variables, unreachable code
- Zero defects required in safety-relevant paths

## Phase 2: Verification & Coverage (Months 4-9)

### 2.1 Structural Coverage (MC/DC)
SIL 4 requires Modified Condition/Decision Coverage on all
safety-relevant code.

**Target**: 100% MC/DC on `arbiter_eval()`, `arbiter_snapshot_begin()`,
expression evaluator, condition evaluator.

**Tools**: gcov + BullseyeCoverage, or LDRA TBrun, or VectorCAST

**Actions**:
- Instrument engine code for MC/DC measurement
- Write additional test vectors to cover all condition combinations
- Generate coverage report showing 100% MC/DC on safety paths
- Document any exclusions with justification

### 2.2 Determinism Proof
Formal or semi-formal proof that `arbiter_eval(M, S)` is deterministic.

**Level 1 — Empirical** (implemented):
- Unit test running eval 10,000 times with identical inputs
- Assert byte-identical results on every iteration
- Property-based testing with randomized models/inputs

**Level 2 — Bounded Model Checking** (recommended):
- Use CBMC to verify `arbiter_eval()` for all models up to N facts/rules
- Prove: no undefined behavior, same output for same input, termination
- Prove: loop bounds ≤ model table sizes

**Level 3 — Formal Proof** (stretch goal):
- Lean 4 or Coq formalization of the eval algorithm
- Prove determinism, termination, and memory safety as theorems

### 2.3 Fault Injection Testing
- Inject corrupted fact values → verify range check catches them
- Inject stale timestamps → verify staleness detection fires
- Inject truncated model tables → verify engine rejects gracefully
- Inject stack overflow conditions → verify watchdog catches them

## Phase 3: Safety Analysis (Months 6-12)

### 3.1 FMEA (Failure Mode and Effects Analysis)
Systematic analysis of every engine component:
- What can fail?
- What is the effect on system safety?
- What is the detection mechanism?
- What is the mitigation?

See `safety/fmea.md` for the detailed analysis.

### 3.2 FTA (Fault Tree Analysis)
Top-down analysis starting from the top-level hazard:
"Engine produces incorrect output leading to unsafe actuator state"

Decompose into contributing faults, assign probabilities, verify
that combined probability meets SIL 4 target (< 10⁻⁸ per hour).

### 3.3 Safety Case (GSN)
Goal Structuring Notation argument:
- **G1**: Arbiter engine is safe for use in SIL 4 systems
  - **S1**: Deterministic evaluation (evidence: proof + tests)
  - **S2**: Bounded execution (evidence: CBMC + coverage)
  - **S3**: No undefined behavior (evidence: MISRA + static analysis)
  - **S4**: Failure detection (evidence: FMEA + fault injection tests)

See `safety/safety_case.md` for the detailed argument.

## Phase 4: Documentation & Evidence Package (Months 10-14)

### 4.1 Safety Manual (Final)
- Update draft safety manual with all evidence references
- Include assumptions of use, safety constraints, integration guidance
- Per IEC 61508-2 Annex D requirements

### 4.2 Traceability Matrix
Complete bidirectional traceability:
```
Safety Goal → Safety Requirement → Design → Implementation → Test → Evidence
```

### 4.3 Evidence Package Contents
Deliverable to system integrators / assessors:
1. Safety Manual (final)
2. Safety Requirements Specification
3. Architecture Description
4. MISRA C Compliance Report + Deviation Records
5. MC/DC Coverage Report
6. Static Analysis Report (zero defects)
7. CBMC/Formal Verification Report
8. FMEA Report
9. FTA Report
10. Safety Case (GSN)
11. Test Reports (unit, integration, fault injection)
12. Determinism Proof
13. Configuration Management Records

## Phase 5: Assessment (Optional — Integrator Responsibility)

Third-party functional safety assessment by an accredited body
(TÜV, SGS, Exida, Bureau Veritas). This phase is documented here
for completeness but is the responsibility of the system integrator
or can be undertaken by BitConcepts if market demand warrants it.

**Estimated cost**: $150K-$400K depending on scope and assessor
**Estimated duration**: 6-12 months including remediation cycles

## Tools Required

| Tool | Purpose | License | Phase |
|------|---------|---------|-------|
| cppcheck + MISRA addon | MISRA C:2012 checking | Open source | 1 |
| clang-tidy | Static analysis | Open source | 1 |
| CBMC | Bounded model checking | Open source | 2 |
| gcov | Line/branch coverage | Open source | 2 |
| BullseyeCoverage or LDRA | MC/DC coverage | Commercial | 2 |
| Polyspace (optional) | Formal static analysis | Commercial | 1-2 |
| Lean 4 (optional) | Formal proof | Open source | 2 |

## Risk Assessment

**High confidence areas** (low risk to certification):
- Deterministic evaluation (simple, bounded, testable)
- No dynamic allocation (trivially provable)
- Division-by-zero handling (single code path)

**Medium confidence areas** (moderate effort):
- MISRA C compliance (many rules, some may need refactoring)
- MC/DC coverage (needs comprehensive test vectors)
- FMEA completeness (systematic but labor-intensive)

**Lower confidence areas** (significant effort):
- Full formal proof of determinism (Lean 4 — high skill requirement)
- FTA probability calculations (need reliability data)
- Assessor acceptance of SEooC scope definition
