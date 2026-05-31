# Arbiter Safety Case (GSN Outline)

## Goal Structuring Notation

This document presents the structured safety argument for the Arbiter
engine using Goal Structuring Notation (GSN) concepts. It links
top-level safety goals to strategies, sub-goals, and evidence.

## Top-Level Goal

**G1: The Arbiter engine is acceptably safe for use as a reasoning
component in SIL 4 / ASIL D systems, within the defined assumptions
of use.**

Context:
- C1: Arbiter is a SEooC (Safety Element out of Context)
- C2: System-level certification is the integrator's responsibility
- C3: Arbiter provides deterministic, bounded rule evaluation

## Strategy Decomposition

```
G1 (Engine is safe)
├── S1: Argument over deterministic behavior
│   ├── G1.1: Same inputs always produce same outputs
│   │   ├── E1.1a: Determinism proof test (10,000 iterations)
│   │   ├── E1.1b: Cross-seed consistency test (100×100 scenarios)
│   │   ├── E1.1c: Fact write order independence test
│   │   └── E1.1d: CBMC bounded model check (planned)
│   └── G1.2: Evaluation terminates in bounded time
│       ├── E1.2a: All loops bounded by model table sizes (code review)
│       ├── E1.2b: No recursion (MISRA Rule 17.2)
│       └── E1.2c: Watchdog supervision (CONFIG_ARBITER_WATCHDOG)
│
├── S2: Argument over absence of undefined behavior
│   ├── G2.1: No dynamic memory allocation
│   │   ├── E2.1a: Code review (no malloc/realloc/free)
│   │   └── E2.1b: MISRA C Rule 21.3 compliance (static analysis)
│   ├── G2.2: No integer overflow
│   │   ├── E2.2a: 64-bit widening in expressions (code review)
│   │   └── E2.2b: Overflow protection unit tests
│   ├── G2.3: No division by zero
│   │   ├── E2.3a: Zero-check in eval loop (code review)
│   │   └── E2.3b: Division by zero unit test
│   └── G2.4: MISRA C:2012 compliance
│       ├── E2.4a: MISRA compliance report (cppcheck/Polyspace)
│       └── E2.4b: Deviation records with rationale
│
├── S3: Argument over failure detection and safe state
│   ├── G3.1: Safety guards always evaluated first
│   │   ├── E3.1a: Canonical ordering in compiler
│   │   ├── E3.1b: Validator checks rule class order
│   │   └── E3.1c: Determinism test verifies guard priority
│   ├── G3.2: Stale sensor data is detected
│   │   ├── E3.2a: Staleness check in snapshot
│   │   └── E3.2b: Staleness detection unit test
│   ├── G3.3: Corrupted model data is detected
│   │   ├── E3.3a: SHA-256 model hash
│   │   └── E3.3b: Hash verification test
│   └── G3.4: Fault injection testing
│       ├── E3.4a: Out-of-range fact injection test
│       ├── E3.4b: Stale timestamp injection test
│       └── E3.4c: Truncated model table test (planned)
│
└── S4: Argument over verification completeness
    ├── G4.1: MC/DC coverage ≥ 100% on safety paths
    │   └── E4.1a: Coverage report (planned)
    ├── G4.2: All safety requirements have tests
    │   └── E4.2a: Specsmith audit: 36/36 REQs covered
    └── G4.3: FMEA completed with mitigations
        └── E4.3a: safety/fmea.md (10 failure modes analyzed)
```

## Evidence Status

| Evidence | Status | Confidence |
|----------|--------|------------|
| E1.1a: Determinism repeat test | ✓ Implemented | 0.90 |
| E1.1b: Cross-seed test | ✓ Implemented | 0.90 |
| E1.1c: Order independence test | ✓ Implemented | 0.90 |
| E1.1d: CBMC model check | ✗ Planned | — |
| E1.2a: Bounded loops review | ✓ Done | 0.85 |
| E1.2b: No recursion (MISRA) | ✓ By design | 0.95 |
| E1.2c: Watchdog | ✓ Implemented | 0.90 |
| E2.1a: No malloc review | ✓ Done | 0.95 |
| E2.1b: MISRA 21.3 | ✗ Needs tool run | — |
| E2.2a: 64-bit widening review | ✓ Done | 0.90 |
| E2.2b: Overflow tests | ✓ Implemented | 0.85 |
| E2.3a: Zero-check review | ✓ Done | 0.95 |
| E2.3b: Div-by-zero test | ✓ Implemented | 0.95 |
| E2.4a: MISRA report | ✗ Not started | — |
| E2.4b: Deviation records | ✗ Not started | — |
| E3.1a: Canonical ordering | ✓ Implemented | 0.90 |
| E3.1b: Validator | ✓ Implemented | 0.90 |
| E3.2a: Staleness check | ✓ Implemented | 0.90 |
| E3.2b: Staleness test | ✓ Implemented | 0.85 |
| E3.3a: SHA-256 hash | ✓ Implemented | 0.90 |
| E3.3b: Hash test | ✓ Implemented | 0.85 |
| E3.4a: Range injection | ✓ Implemented | 0.85 |
| E3.4c: Truncated model | ✗ Planned | — |
| E4.1a: MC/DC report | ✗ Not started | — |
| E4.2a: REQ coverage | ✓ 36/36 | 0.95 |
| E4.3a: FMEA | ✓ 10 FMs analyzed | 0.85 |

## Overall Confidence

Current weighted confidence across all evidence: **~0.72**

To reach ≥0.90 (recommended for SIL 4 readiness):
- Complete CBMC bounded model checking (+0.08)
- Complete MISRA C compliance report (+0.06)
- Complete MC/DC coverage report (+0.05)
- Complete truncated model fault injection (+0.02)

## Assumptions of Use

1. The application correctly implements action callbacks
2. The application verifies model hash at startup
3. Fact values are written from a single thread (or with appropriate synchronization)
4. The target platform provides atomic 32-bit aligned writes
5. Flash/ROM integrity is maintained by hardware ECC
6. The system integrator performs system-level HARA
