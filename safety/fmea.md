# Arbiter Engine — Failure Mode and Effects Analysis (FMEA)

## Scope
Analysis of all failure modes in the Arbiter runtime engine
(`lib/`, `subsys/arbiter/`) that could lead to incorrect or
unsafe evaluation results.

## Severity Scale
- **S4** (Catastrophic): Loss of life or irreversible damage
- **S3** (Critical): Severe injury or major equipment damage
- **S2** (Major): Minor injury or significant equipment damage
- **S1** (Minor): Nuisance or minor equipment impact

## Detection Scale
- **D1** (Certain): Always detected before effect occurs
- **D2** (High): Detected in most cases
- **D3** (Moderate): May or may not be detected
- **D4** (Low): Unlikely to be detected

## FMEA Table

### FM-001: Fact Value Corruption
- **Component**: arbiter_fact_store.c
- **Failure Mode**: Fact value written outside declared range
- **Cause**: Application writes invalid sensor data
- **Effect**: Incorrect rule evaluation, potentially wrong mode/action
- **Severity**: S3
- **Detection**: D1 — range validation rejects out-of-range writes
- **Mitigation**: `arbiter_set_*()` validates against `range_min`/`range_max`
- **Residual Risk**: None if range is correctly specified in model
- **Test**: TEST-004 (fact store with timestamps)

### FM-002: Stale Sensor Data
- **Component**: arbiter_eval.c (snapshot)
- **Failure Mode**: Fact timestamp exceeds `stale_after_ms`
- **Cause**: Sensor driver stops updating, communication loss
- **Effect**: Decisions based on outdated data
- **Severity**: S3
- **Detection**: D1 — staleness check in snapshot, `stale` operator in rules
- **Mitigation**: Safety guard rules trigger on stale facts (model-level)
- **Residual Risk**: Model must define stale guards; engine cannot enforce this
- **Test**: TEST-034 (staleness detection)

### FM-003: Expression Overflow
- **Component**: arbiter_eval.c (expression evaluator)
- **Failure Mode**: Arithmetic result exceeds int32_t range
- **Cause**: Large operand values combined with multiply/accumulate
- **Effect**: Incorrect computed value (wrapped or saturated)
- **Severity**: S3
- **Detection**: D2 — 64-bit widening prevents silent truncation
- **Mitigation**: All arithmetic uses `int64_t` intermediates; `scale` divides before storing; `accumulate` uses saturation; `clamp` enforces bounds
- **Residual Risk**: If scale factor is wrong in model, result is numerically wrong but not UB
- **Test**: TEST-032 (overflow protection)

### FM-004: Division by Zero
- **Component**: arbiter_eval.c (expression evaluator)
- **Failure Mode**: `div` or `mod` expression with zero divisor
- **Cause**: Fact value is zero and used as divisor in model
- **Effect**: Could cause CPU exception or undefined behavior
- **Severity**: S4
- **Detection**: D1 — explicit zero check returns 0
- **Mitigation**: `if (divisor == 0) return 0;` in eval loop
- **Residual Risk**: None — zero is always a safe default for division
- **Test**: TEST-035 (division by zero handling)

### FM-005: Model Table Corruption
- **Component**: arbiter_model.h (const data)
- **Failure Mode**: Model tables in flash/ROM are corrupted (bit flip)
- **Cause**: Flash degradation, cosmic ray, incorrect programming
- **Effect**: Wrong rules evaluated, wrong conditions, wrong actions
- **Severity**: S4
- **Detection**: D2 — model SHA-256 hash can be verified at init time
- **Mitigation**: Application should verify `model_hash` against known-good value at startup. CRC/ECC on flash provides hardware-level protection.
- **Residual Risk**: If hash check is skipped, corruption is undetected
- **Test**: TEST-033 (model integrity hash)

### FM-006: Safety Guard Not First
- **Component**: arbiter_eval.c (rule loop)
- **Failure Mode**: Safety guard rules evaluated after inference rules
- **Cause**: Compiler bug produces wrong canonical order
- **Effect**: Inference computes before safety guard can override
- **Severity**: S4
- **Detection**: D1 — compiler sorts safety_guard class first; validator checks order
- **Mitigation**: Canonical ordering is enforced at compile time by `arbiterc`; eval loop processes rules in table order (index 0..N)
- **Residual Risk**: Compiler bug could mis-sort; mitigated by `arbiterc validate`
- **Test**: TEST-029 (deterministic evaluation order)

### FM-007: Unbounded Execution
- **Component**: arbiter_eval.c
- **Failure Mode**: Eval loop runs indefinitely
- **Cause**: Bug in loop termination condition
- **Effect**: System hangs, watchdog timeout
- **Severity**: S3
- **Detection**: D1 — all loops bounded by `model->rule_count`, `condition_count`, `expr_count`; watchdog catches hangs
- **Mitigation**: No recursion, no while(true), all for-loops use model table sizes as bounds
- **Residual Risk**: None if code review confirms loop bounds
- **Test**: TEST-031 (bounded execution)

### FM-008: Dynamic Memory Allocation
- **Component**: All engine code
- **Failure Mode**: malloc/realloc called during eval, causing heap fragmentation or OOM
- **Cause**: Code change introduces heap allocation
- **Effect**: Non-deterministic behavior, possible crash
- **Severity**: S4
- **Detection**: D1 — MISRA C Rule 21.3 forbids stdlib allocation; static analysis catches any use
- **Mitigation**: Engine uses only stack variables and const model tables; no heap functions linked
- **Residual Risk**: None if MISRA check is part of CI
- **Test**: TEST-030 (no dynamic allocation)

### FM-009: Action Callback Failure
- **Component**: arbiter_action.c
- **Failure Mode**: Application callback crashes, hangs, or takes too long
- **Cause**: Bug in application-provided callback function
- **Effect**: System hang or incorrect actuator state
- **Severity**: S3
- **Detection**: D3 — engine cannot observe callback internals
- **Mitigation**: `must_complete_within_ms` documents timing constraint; watchdog supervision catches hangs; application is responsible for callback correctness
- **Residual Risk**: Callbacks are outside engine scope — system integrator must verify
- **Test**: N/A (application responsibility)

### FM-010: Concurrent Access
- **Component**: arbiter_ctx (fact values)
- **Failure Mode**: ISR writes fact while eval is reading
- **Cause**: Missing synchronization between sensor ISR and eval thread
- **Effect**: Torn reads, inconsistent snapshot
- **Severity**: S3
- **Detection**: D2 — snapshot copies values atomically at start of eval
- **Mitigation**: `arbiter_snapshot_begin()` copies all facts before eval starts; application should not write facts during snapshot+eval sequence
- **Residual Risk**: If 32-bit writes are not atomic on target, individual fact reads could tear. On ARM Cortex-M this is atomic for aligned 32-bit values.
- **Test**: Documented in safety manual (integration guidance)

## Summary

| FM | Severity | Detection | Mitigated By |
|----|----------|-----------|-------------|
| FM-001 | S3 | D1 | Range validation |
| FM-002 | S3 | D1 | Staleness detection + safety guards |
| FM-003 | S3 | D2 | 64-bit widening + saturation |
| FM-004 | S4 | D1 | Zero check returns 0 |
| FM-005 | S4 | D2 | SHA-256 hash verification |
| FM-006 | S4 | D1 | Canonical ordering + validator |
| FM-007 | S3 | D1 | Bounded loops + watchdog |
| FM-008 | S4 | D1 | MISRA C Rule 21.3 + static analysis |
| FM-009 | S3 | D3 | Watchdog + timing constraint (app responsibility) |
| FM-010 | S3 | D2 | Snapshot isolation |

## Open Items
- FM-005: Hash verification should be mandatory, not optional
- FM-009: Consider adding callback timing enforcement in engine
- FM-010: Consider adding mutex/spinlock option for RTOS targets
