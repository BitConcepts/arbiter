# arbiter Safety Manual

## 1. Purpose

This document describes the safety-relevant properties of the arbiter
reasoning engine and how they support integration into safety-critical
embedded systems.

**This document does not constitute a safety certification claim.**
arbiter provides architectural support for functional safety workflows
but has not been independently certified to any SIL or ASIL level.

## 2. Scope

Covers the arbiter C runtime (`lib/`, `subsys/arbiter/`), the ARB model
format, and the `arbiterc` compiler toolchain.

## 3. Safety Architecture

### 3.1 Deterministic Evaluation

The engine evaluates rules in a fixed, canonical order defined at
compile time.  Given the same fact snapshot, the engine always produces
the same result.  There is no dynamic memory allocation, no recursion,
and no unbounded loops during evaluation.

### 3.2 Rule Classification

Every rule carries a `class` attribute:

- `safety_guard` — Evaluated first.  If any safety guard fires, its
  effects (mode change, output zeroing) take precedence over all
  other rules.  This ensures fail-safe behavior.
- `mode_guard` — State machine transitions.
- `inference` — Computational rules (including compute expressions).
- `constraint` — Invariant enforcement.
- `obligation` — Required output actions.
- `advisory` — Non-critical informational rules.

### 3.3 Compute Expression Safety

- All arithmetic uses 64-bit widening to prevent silent overflow.
- `scale` and `accumulate` operations use explicit divisors.
- `clamp` enforces output bounds declaratively.
- Division by zero returns 0 (safe default).

### 3.4 Model Integrity

- Each compiled model carries a SHA-256 hash of its canonical form.
- The schema hash validates that the model was compiled against the
  expected ARB schema version.
- At init time, the application can verify these hashes against
  expected values.

### 3.5 Staleness Detection

Facts can declare `stale_after_ms`.  The engine checks timestamps
during snapshot evaluation and can trigger safety guards when sensor
data is stale.

### 3.6 Traceability

- Every rule has a unique `id` and optional `explanation`.
- The trace subsystem (`CONFIG_ARBITER_TRACE=y`) records which rules
  fired, in what order, with what mode transitions.
- Facts are named and can be inspected at runtime via the shell
  subsystem.

## 4. Integration Guidance

### 4.1 Watchdog Supervision

Enable `CONFIG_ARBITER_WATCHDOG=y` to wrap the evaluation thread in a
hardware watchdog.  If evaluation exceeds the configured timeout, the
watchdog resets the system — preventing stuck-engine scenarios.

### 4.2 Fact Range Validation

Facts declare `range: [min, max]`.  The engine validates written
values against these ranges.  Out-of-range writes are rejected,
preventing garbage propagation.

### 4.3 Safe State Actions

Actions can be marked `safe_state_action: true`.  These are the
actions triggered by safety_guard rules and represent the system's
defined safe state (e.g., zero actuator output, open contactor).

## 5. Limitations

- arbiter is **not certified** to IEC 61508, ISO 26262, or any other
  functional safety standard.
- The engine does not implement redundant execution or voting
  internally (see the `tmr_voter` sample for how to model TMR
  externally).
- The expression evaluator does not support floating-point — all
  math is fixed-point integer.
- Staleness detection depends on the application providing accurate
  timestamps.

## 6. Recommended Verification

- Run `arbiterc validate` on every model to check schema conformance
  and rule consistency.
- Use the provided test suites (`tests/`) as a starting point for
  application-specific validation.
- Review the benchmark results (`tests/benchmarks/`) to confirm that
  engine execution time fits within your system's real-time budget.
- Perform FMEA on the ARB model to identify failure modes in rule
  interactions.
