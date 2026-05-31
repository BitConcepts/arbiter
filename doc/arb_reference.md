# ARB Model Reference

## File Format

ARB models are YAML files with the `.arb.yaml` extension.

```yaml
arb_version: 0.1
model: <model_name>

target:
  rtos: zephyr
  profile: static_c
  safety_profile: <optional>

facts: [...]
modes: [...]
rules: [...]
actions: [...]
```

## Top-Level Keys

- **arb_version** (required): Must be `0.1`.
- **model** (required): Model name, used in generated code identifiers.
- **target** (required): Target platform and profile.
- **system** (optional): System/item context for safety traceability.
- **includes** (optional): List of reusable ARB fragment files to import.
- **facts** (required): List of fact definitions.
- **modes** (optional): List of mode definitions.
- **rules** (required): List of rule definitions.
- **actions** (optional): List of action definitions.

## Facts

Each fact represents a typed value in the reasoning context.

```yaml
facts:
  - id: sensor.temperature
    type: int32          # bool | int32 | uint32 | enum
    unit: millideg       # optional, documentation only
    range: [-40000, 125000]  # optional, enforced at write time
    default: 25000       # optional, initial value
    source: sensor       # optional, documentation
    stale_after_ms: 100  # optional, staleness threshold
    safety_relevant: true  # optional, marks for safety analysis
```

**Types**: `bool`, `int32`, `uint32`, `enum`

## Modes

Modes represent the system's state machine states.

```yaml
modes:
  - id: mode.idle
  - id: mode.running
  - id: mode.fault
```

## Rules

Rules are the core reasoning elements. Each rule has conditions and consequences.

```yaml
rules:
  - id: "01_safety_guard"
    class: safety_guard    # safety_guard | mode_guard | inference |
                           # constraint | obligation | advisory
    when:
      all:                 # all | any
        - fact: sensor.valid
          op: "=="         # == != < <= > >= in not_in stale
          value: false     #   not_stale changed delta_gt delta_lt
    then:
      set_mode: mode.fault
      action: emergency_stop
      compute:             # optional compute expressions
        - target: output.value
          op: assign       # add sub mul div mod abs negate
          left_literal: 0  #   min max clamp shift_r shift_l
                           #   scale assign accumulate
      explanation: "Sensor fault detected."
      criticality: safety_critical
```

### Rule Classes (evaluation order)
1. `safety_guard` — Evaluated first; effects take precedence
2. `mode_guard` — State machine transitions
3. `inference` — Computational rules
4. `constraint` — Invariant enforcement
5. `obligation` — Required actions
6. `advisory` — Non-critical informational

### Condition Operators
`==`, `!=`, `<`, `<=`, `>`, `>=`, `in`, `not_in`, `stale`, `not_stale`, `changed`, `delta_gt`, `delta_lt`

### Compute Expression Operators
`add`, `sub`, `mul`, `div`, `mod`, `abs`, `negate`, `min`, `max`, `clamp`, `shift_r`, `shift_l`, `scale`, `assign`, `accumulate`

- `scale`: `target = (left * right) / scale` — fixed-point multiply
- `accumulate`: `target += (left * right) / scale` — running sum
- `clamp`: `target = clamp(left, right_literal=lo, scale=hi)`
- All arithmetic uses 64-bit widening; division by zero returns 0.

## Actions

```yaml
actions:
  - id: emergency_stop
    type: callback         # callback | log | notify | set_fact |
                           #   set_mode | raise_fault | clear_fault
    symbol: app_emergency_stop  # C function name (callback type)
    must_complete_within_ms: 5  # optional timing constraint
    safe_state_action: true     # marks as safe-state action
```

## Includes

Reuse common fact/rule fragments from the include library:

```yaml
includes:
  - lib/arb/sensor_health.arb.yaml
  - lib/arb/estop.arb.yaml
```

## Schema Validation

Models can be validated against the JSON schema:

```sh
arbiterc validate model.arb.yaml --strict
```

The schema is at `schema/arb.schema.json`.
