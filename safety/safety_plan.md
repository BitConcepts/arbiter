# zproj Safety Plan

## 1. Overview

This plan describes the safety lifecycle activities for zproj, a
deterministic reasoning engine for embedded systems.

**Important**: zproj is not a certified safety component.  This plan
provides a framework for teams using zproj in safety-relevant
applications to build their own safety case.

## 2. Safety Lifecycle

### 2.1 Concept Phase
- Define the system's safety context and hazard analysis.
- Identify which ZRM rules are safety-relevant (`safety_relevant: true`
  on facts, `criticality: safety_critical` on rules).

### 2.2 Model Development
- Author ZRM models in YAML.
- Use `zprojc validate` to check schema conformance.
- Review models against safety requirements (SR-001 through SR-010).
- Use `zprojc docs` to generate human-readable rule documentation.

### 2.3 Compilation
- Run `zprojc compile` to generate C code from the model.
- The compiler canonicalizes rules, validates constraints, and embeds
  integrity hashes.
- The compiled output is deterministic: same model → same C code.

### 2.4 Integration
- Integrate generated C code into the Zephyr application.
- Implement action callbacks (`app_*` functions).
- Configure watchdog supervision if required.
- Enable tracing for development and validation builds.

### 2.5 Verification
- Run unit tests (`tests/zephyr/`) covering engine evaluation,
  expression correctness, and boundary conditions.
- Run sample applications on target hardware.
- Execute benchmarks to confirm real-time budget compliance.
- Perform code review of hand-written C (callbacks, main loop).

### 2.6 Validation
- System-level testing with real sensors and actuators.
- Fault injection testing (stale sensors, out-of-range values,
  watchdog timeout).
- Review trace logs to confirm safety guards fire correctly.

## 3. Configuration Management

- ZRM models are version-controlled alongside application code.
- Model hash changes are tracked in commit history.
- The `zprojc inspect` command displays model metadata for audit.

## 4. Roles

- **Model Author**: Writes and maintains ZRM models.
- **Firmware Engineer**: Implements callbacks and system integration.
- **Safety Engineer**: Reviews models, requirements, and test results.
- **Validator**: Executes test plans and records evidence.

## 5. Documentation Artifacts

- `safety/safety_manual.md` — Engine safety properties.
- `safety/requirements.md` — Safety requirements with verification.
- `safety/safety_plan.md` — This document.
- `doc/` — User-facing documentation.
- `tests/benchmarks/README.md` — Performance characterization.

## 6. Tool Qualification

The `zprojc` compiler is a development tool, not a certified tool.
Its output (generated C code) must be reviewed or tested as part of
the application's verification activities.  The compiler itself does
not require tool qualification unless the generated code is used
without independent verification.
