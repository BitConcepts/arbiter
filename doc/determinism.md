# Determinism & Safety Design

## Deterministic Evaluation

Arbiter guarantees deterministic evaluation: given the same compiled model
and the same fact snapshot, `arbiter_eval()` always produces the same
result — same fired rules, same mode, same computed values, same actions.

### How Determinism Is Achieved

1. **Canonical rule ordering**: Rules are sorted at compile time into a
   fixed evaluation order (safety_guard → mode_guard → inference →
   constraint → obligation → advisory). Within each class, rules are
   ordered alphabetically by ID.

2. **Snapshot isolation**: Facts are copied into an immutable snapshot
   before evaluation begins. No external mutation can affect the
   evaluation mid-cycle.

3. **No dynamic allocation**: The engine uses no `malloc`, `realloc`, or
   `free`. All memory is statically sized based on the compiled model's
   fact/rule counts.

4. **Bounded execution**: Every loop in the evaluator is bounded by
   the model's table sizes (`rule_count`, `condition_count`,
   `expr_count`). There is no recursion.

5. **No floating-point**: All arithmetic is fixed-point integer with
   explicit scale factors. This avoids platform-dependent rounding.

6. **Model hashing**: Each compiled model carries a SHA-256 hash of its
   canonical representation. Two models with identical content always
   produce the same hash, regardless of YAML key order or whitespace.

## Safety Architecture

### Rule Priority

Safety guards are always evaluated first. If a safety guard fires,
its effects (mode change, output zeroing, safe-state actions) take
precedence over all other rules. This ensures fail-safe behavior
regardless of what other rules would have computed.

### Overflow Protection

All compute expressions use 64-bit intermediate values:

```
int64_t result = (int64_t)left * (int64_t)right;
output = (int32_t)(result / scale);
```

This prevents silent overflow for any 32-bit operand combination.
The `accumulate` operator uses saturation to prevent runaway
integrator windup.

### Division by Zero

The `div` and `mod` operators return 0 when the divisor is 0.
No exception, trap, or undefined behavior occurs.

### Staleness Detection

Facts can declare `stale_after_ms`. During snapshot creation, the
engine checks each fact's timestamp against the current time. The
`stale` condition operator allows rules to detect and react to
stale sensor data.

### Watchdog Integration

With `CONFIG_ARBITER_WATCHDOG=y`, the evaluation thread is supervised
by a hardware watchdog. If evaluation exceeds the configured timeout,
the watchdog resets the system — preventing stuck-engine scenarios
that could leave actuators in unsafe states.

## Traceability

- Every rule has a unique `id` and optional `explanation`.
- The trace subsystem records which rules fired, in what order.
- The model hash provides a cryptographic link between the YAML
  source and the running firmware.
- Changing any fact, rule, or expression produces a new hash.

## What Arbiter Does NOT Guarantee

- Arbiter does not guarantee functional safety on its own.
- Arbiter does not implement redundant execution or voting internally.
- Arbiter does not verify that action callbacks execute correctly.
- Certification is the responsibility of the system integrator.

See also: [Safety Manual](../safety/safety_manual.md),
[Safety Requirements](../safety/requirements.md).
