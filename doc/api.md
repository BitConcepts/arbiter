# Arbiter C API Reference

## Headers

```c
#include <arbiter/arbiter.h>        // Main API
#include <arbiter/arbiter_model.h>  // Model data structures
#include <arbiter/arbiter_result.h> // Evaluation results
#include <arbiter/arbiter_trace.h>  // Trace API
```

## Initialization

```c
int arbiter_init(struct arbiter_ctx *ctx, const struct arbiter_model *model);
```

Initialize the engine context with a compiled model. Sets all facts to defaults. Returns `ARBITER_OK` (0) on success.

## Setting Facts

```c
int arbiter_set_bool(struct arbiter_ctx *ctx, uint16_t fact_id, bool value);
int arbiter_set_i32(struct arbiter_ctx *ctx, uint16_t fact_id, int32_t value);
int arbiter_set_u32(struct arbiter_ctx *ctx, uint16_t fact_id, uint32_t value);
int arbiter_set_timestamp(struct arbiter_ctx *ctx, uint16_t fact_id, uint32_t ts_ms);
```

Write a value to a fact. Range validation is applied if the fact has a declared range.

## Evaluation

```c
void arbiter_snapshot_begin(struct arbiter_ctx *ctx, struct arbiter_snapshot *snap);
int arbiter_eval(const struct arbiter_model *model,
                 struct arbiter_snapshot *snap,
                 struct arbiter_result *result,
                 struct arbiter_trace *trace);
```

1. `arbiter_snapshot_begin()` — captures fact values into an immutable snapshot.
2. `arbiter_eval()` — evaluates all rules against the snapshot. Deterministic: same snapshot always produces the same result.

## Reading Results

```c
int arbiter_get_mode(const struct arbiter_result *result, uint16_t *mode_id);
```

## Model Structure

The compiled model is a `const struct arbiter_model` with:
- `facts[]` — fact definitions (type, range, default, name)
- `rules[]` — rule definitions (class, conditions, expressions)
- `conditions[]` — condition definitions (fact, op, value)
- `expressions[]` — compute expressions (op, operands, scale)
- `actions[]` — action definitions (type, callback, timing)
- `mode_names[]` — string names for modes
- `model_hash[32]` — SHA-256 of canonical model
- `schema_hash[32]` — SHA-256 of schema version

## Kconfig Options

```
CONFIG_ARBITER=y              # Enable Arbiter engine
CONFIG_ARBITER_TRACE=y        # Enable rule-firing trace
CONFIG_ARBITER_SHELL=y        # Enable shell inspection commands
CONFIG_ARBITER_WATCHDOG=y     # Enable watchdog supervision
CONFIG_ARBITER_MAX_FACTS=64   # Maximum facts per model
```

## Error Codes

- `ARBITER_OK` (0) — Success
- `ARBITER_ERR_INVALID` — Invalid parameter
- `ARBITER_ERR_RANGE` — Value out of declared range
- `ARBITER_ERR_STALE` — Fact timestamp is stale
