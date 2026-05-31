# arbiter — Deterministic Reasoning & Safety-Policy Engine for Zephyr RTOS

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Zephyr Module](https://img.shields.io/badge/Zephyr-Module-brightgreen)](https://docs.zephyrproject.org/latest/develop/modules.html)

**Arbiter** is a deterministic reasoning and safety-policy engine for
[Zephyr RTOS](https://www.zephyrproject.org/). It lets you express safety
policies, mode-transition logic, and system-level reasoning rules in a
declarative YAML model format called **ARB**, then compile those models into
bounded, deterministic C representations that run efficiently on
resource-constrained microcontrollers.

## What arbiter Is

- A **YAML model format** (`.arb.yaml`) for expressing facts, rules, modes,
  hazards, safety goals, and actions.
- A **Python compiler** (`arbiterc`) that validates models and emits generated C
  source/headers or compact binary blobs (`.arbb`).
- A **Zephyr runtime** (`libarbiter`) that evaluates compiled models
  deterministically — same model + same input = same output.
- A **Zephyr shell interface** for runtime inspection and debugging.
- A **safety-evidence package** supporting functional-safety certification
  workflows (IEC 61508 / SIL readiness roadmap).

## What arbiter Is NOT

- A SAT/SMT solver or probabilistic inference engine.
- A hardware timing fabric, FPGA constraint fabric, or ASIC logic.
- A packet/DMA/GPU scheduler or global tick distribution system.
- A runtime YAML parser in firmware.
- Certified to any functional-safety standard (yet).

## Quickstart

### Add to Your West Manifest

```yaml
manifest:
  remotes:
    - name: zephyrproject-rtos
      url-base: https://github.com/zephyrproject-rtos
    - name: bitconcepts
      url-base: https://github.com/BitConcepts
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: main
      import: true
    - name: arbiter
      remote: bitconcepts
      revision: main
      path: modules/lib/arbiter
  self:
    path: app
```

### Build and Run a Sample

```sh
west init -m <your-manifest-repo>
west update
west build -b native_sim modules/lib/arbiter/samples/battery_policy
west build -t run
```

### Write an ARB Model

```yaml
arb_version: 0.1
model: motor_safety_policy

target:
  rtos: zephyr
  profile: static_c
  safety_profile: zrm_safety_strict_v0

facts:
  - id: motor.speed_rpm
    type: uint32
    unit: rpm
    range: [0, 12000]
    source: sensor
    stale_after_ms: 20
    safety_relevant: true

  - id: estop.active
    type: bool
    source: gpio
    safety_relevant: true

modes:
  - id: mode.normal
  - id: mode.degraded
  - id: mode.safe_shutdown

rules:
  - id: guard_estop_shutdown
    class: safety_guard
    when:
      all:
        - fact: estop.active
          op: "=="
          value: true
    then:
      set_mode: mode.safe_shutdown
      action: disable_motor_pwm
      explanation: E-stop is active, forcing safe shutdown.
      criticality: safety_critical

actions:
  - id: disable_motor_pwm
    type: callback
    symbol: app_disable_motor_pwm
    must_complete_within_ms: 5
    safe_state_action: true
```

### Compile a Model

```sh
# Validate
arbiterc validate model.arb.yaml --strict

# Compile to C tables (primary safety path)
arbiterc compile model.arb.yaml --out-c ARBITER_model.c --out-h ARBITER_model.h

# Compile to binary blob (optional)
arbiterc compile model.arb.yaml --out-blob model.zrmb

# Generate documentation
arbiterc emit-docs model.arb.yaml --out model.md
```

### Enable in Your Application

```
# prj.conf
CONFIG_ARBITER=y
CONFIG_ARBITER_TRACE=y
```

## Design Principles

- **Deterministic**: Same model + same input snapshot + same runtime version =
  same output and trace, always.
- **Bounded**: All memory, execution time, and resource usage are statically
  bounded and computed at compile time.
- **Generated C tables are the primary safety path.** Filesystem blobs are
  optional and require additional safety justification.
- **No runtime YAML parsing** in firmware — models are compiled ahead of time.
- **Safety-oriented**: Strict safety profile forbids heap after init, floating
  point, recursion, unbounded loops, and nondeterministic constructs.

## Safety Disclaimer

> arbiter is designed to support deterministic, bounded, explainable reasoning in
> Zephyr-based systems and to produce evidence useful for functional-safety
> certification workflows. arbiter has an ASIL D / SIL 4 readiness roadmap.
>
> **arbiter is NOT certified to any functional-safety standard.** It does not
> guarantee functional safety on its own. Certification is the responsibility of
> the system integrator.

## Documentation

- [Getting Started](doc/getting_started.rst)
- [ARB Model Reference](doc/arb_reference.md)
- [C API Reference](doc/api.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Safety Manual](safety/safety_manual.md)
- [Safety Requirements](safety/requirements.md)
- [Determinism & Safety Design](doc/determinism.md)
- [Benchmarks](tests/benchmarks/README.md)

## License

This project is licensed under the [MIT License](LICENSE).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
