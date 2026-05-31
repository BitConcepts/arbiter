# zproj Safety Requirements

## SR-001: Deterministic Evaluation Order
The engine SHALL evaluate rules in a fixed order determined at compile
time.  The evaluation order SHALL NOT depend on runtime state.

**Verification**: Unit test `tests/zephyr/src/test_eval.c` confirms
identical results across repeated evaluations with the same inputs.

## SR-002: Safety Guard Priority
Rules with `class: safety_guard` SHALL be evaluated before all other
rule classes.  If a safety guard fires, its effects SHALL take
precedence.

**Verification**: Model validator (`zprojc validate`) checks that
safety_guard rules precede other classes in canonical order.

## SR-003: No Dynamic Allocation
The engine SHALL NOT call `malloc`, `realloc`, `free`, or any dynamic
memory allocator during initialization or evaluation.

**Verification**: Code review and static analysis of `lib/` and
`subsys/zproj/` sources.

## SR-004: Bounded Execution
The number of rules, conditions, and expressions evaluated per
`zproj_eval` call is bounded by the compiled model's table sizes.
There SHALL be no unbounded loops or recursion.

**Verification**: Code review confirms loop bounds are
`model->rule_count`, `model->condition_count`, `model->expr_count`.

## SR-005: Overflow Protection
Arithmetic in compute expressions SHALL use 64-bit intermediate
values.  Results SHALL be truncated to int32_t with defined behavior
(saturation for accumulate, truncation for others).

**Verification**: Unit tests with boundary values in
`tests/zephyr/src/test_eval.c`.

## SR-006: Division by Zero Handling
The `div` and `mod` expression operators SHALL return 0 when the
divisor is 0.  No exception or undefined behavior SHALL occur.

**Verification**: Unit test with zero divisor.

## SR-007: Fact Range Enforcement
When a fact declares `range: [min, max]`, the engine SHALL reject
writes outside this range and return an error code.

**Verification**: Unit test writing out-of-range values.

## SR-008: Staleness Detection
When a fact declares `stale_after_ms: N`, the engine SHALL mark the
fact as stale if its timestamp is older than N ms relative to the
snapshot timestamp.  Stale facts SHALL be detectable via the `stale`
condition operator.

**Verification**: Unit test with simulated stale timestamps.

## SR-009: Model Integrity Hash
Each compiled model SHALL carry a SHA-256 hash of its canonical
representation.  The application SHALL be able to verify this hash
at initialization.

**Verification**: `zprojc compile` produces hash; init code can
compare.

## SR-010: Trace Completeness
When tracing is enabled (`CONFIG_ZPROJ_TRACE=y`), the engine SHALL
record every rule that fires, including its id, class, and mode
transition (if any).

**Verification**: Trace output review in integration tests.
