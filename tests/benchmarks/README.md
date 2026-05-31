# zproj Benchmarks

Comparative benchmarks measuring the zproj reasoning engine against
equivalent hand-coded C implementations.

## Available Benchmarks

| Benchmark | zproj Model | Hand-coded Baseline | Key Metric |
|-----------|-------------|---------------------|------------|
| `pid_benchmark` | PID controller (16 facts, 8 rules, 25 exprs) | 6-field struct, ~30 lines | ns/tick |
| `kalman_benchmark` | 1-D Kalman filter (14 facts, 5 rules, 12 exprs) | 5-field struct, ~20 lines | ns/tick |

## Building and Running

```sh
# PID benchmark
west build -b native_sim tests/benchmarks/pid_benchmark
west build -t run

# Kalman benchmark
west build -b native_sim tests/benchmarks/kalman_benchmark
west build -t run
```

## What Is Measured

### Execution Time (CPU cycles)
- Uses Zephyr's `timing_*` API for cycle-accurate measurement.
- 100-iteration warmup to stabilize caches and branch predictors.
- 1000-iteration measured window, averaged to ns/tick.
- Both implementations run the **same algorithm** with the same inputs
  and produce the same outputs — the only difference is whether the
  logic is expressed as hand-coded C or as a ZRM model evaluated by
  the zproj engine.

### RAM
- **Hand-coded**: `sizeof(struct hand_pid)` or `sizeof(struct hand_kf)` —
  just the bare state variables (24–28 bytes typical).
- **zproj engine**: `sizeof(struct zproj_ctx)` — includes the fact value
  array, timestamps, mode state, and evaluation bookkeeping.
- The benchmark reports both at runtime.

### ROM (Flash)
- ROM cannot be measured at runtime.  After building, compare `.elf`
  sizes:

  ```sh
  # Build hand-coded-only version (strip zproj from prj.conf)
  arm-none-eabi-size build/zephyr/zephyr.elf

  # vs. full zproj engine version
  arm-none-eabi-size build/zephyr/zephyr.elf
  ```

- The delta includes:
  - zproj engine code (`zproj_eval`, `zproj_snapshot_begin`, expression
    evaluator, condition evaluator)
  - Compiled model tables (facts, rules, conditions, expressions —
    const data in `.rodata`)
  - Strings (fact names, rule names, explanations)

## Expected Results and Interpretation

### Where hand-coded C wins

- **Execution speed**: Expect 2–5× faster per tick.  The hand-coded PID
  is ~6 arithmetic operations.  The zproj engine must iterate rule tables,
  evaluate condition chains, dispatch expression opcodes, and manage
  snapshot state — all generic indirection that a compiler cannot
  specialize away.

- **RAM**: Dramatically smaller.  A hand-coded PID is 24 bytes of state.
  The zproj context carries per-fact metadata (timestamps, ranges),
  mode tracking, and result buffers — hundreds of bytes even for small
  models.

- **ROM**: Smaller by several KB.  The hand-coded version compiles to
  a tight loop.  zproj includes the generic engine, expression
  interpreter, and const model tables.

### Where zproj wins

- **Deterministic safety semantics**: Every rule has a `class`
  (safety_guard, inference, obligation), `criticality`, and
  `explanation`.  The engine enforces evaluation order guarantees
  that hand-coded if/else chains do not.

- **Traceability**: The model carries a SHA-256 hash.  Every rule
  firing is traceable.  Changing a gain or threshold is a model change
  with a new hash — not an anonymous code edit buried in version control.

- **Auditability for certification**: ZRM models can be validated
  against a JSON schema, inspected via `zprojc inspect`, and
  referenced in safety cases.  Hand-coded C requires separate
  requirement tracing.

- **Runtime inspection**: With `CONFIG_ZPROJ_SHELL=y`, an operator
  can inspect live fact values, mode state, and rule firing from the
  Zephyr shell — impossible with a bare struct.

- **Separation of concerns**: Domain engineers author `.zrm.yaml`
  models; firmware engineers write the C harness.  Changes to control
  logic do not require touching C code.

- **Watchdog integration**: The engine can be wrapped in a watchdog-
  supervised thread with bounded execution time — a common SIL
  requirement that hand-coded PID loops must implement ad hoc.

- **Correctness by construction**: Compute expressions use 64-bit
  widening, saturation, and explicit scale factors.  Hand-coded
  fixed-point math is a common source of silent overflow bugs.

### Bottom line

> zproj is not faster or smaller than hand-coded C.  It is
> **safer, more traceable, and more maintainable**.
>
> The overhead is the cost of declarative safety semantics.  For
> applications where functional safety, auditability, or runtime
> inspection matter, that cost is well justified.  For bare-metal
> hot loops where every nanosecond counts and safety certification
> is not required, hand-coded C is the right choice.

## Adding New Benchmarks

1. Create `tests/benchmarks/<name>/` with the standard layout
   (CMakeLists.txt, prj.conf, testcase.yaml, src/main.c).
2. Include a hand-coded baseline implementing the **same algorithm**
   as the zproj model under test.
3. Use `timing_init()` / `timing_start()` / `timing_counter_get()`
   for measurement.
4. Report both implementations' results and the percentage overhead.
5. Include `build_only: true` in testcase.yaml — benchmarks need
   real hardware or native_sim for meaningful timing.
