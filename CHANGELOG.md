# Changelog

## [0.1.0] - 2026-05-31

### Added
- Core C runtime engine with deterministic rule evaluation
- Compute expression engine with 15 fixed-point opcodes
- Zephyr subsystem: shell commands, runtime thread, watchdog
- Python `arbiterc` compiler: parse, validate, compile, emit docs
- ARB model format (`.arb.yaml`) with JSON schema validation
- 17 sample applications:
  - battery_policy, pid_controller, hydraulic_press, valve_sequencer
  - mesh_router, firewall_policy, access_control, firmware_guard
  - tmr_voter, hvac_controller, cobot_safety, power_mppt
  - kalman_filter, task_scheduler, power_budget, fault_diagnosis, motion_planner
- Reusable ARB include library (`lib/arb/`)
- Benchmarks: PID and Kalman engine vs hand-coded C
- Safety artifacts: safety manual, requirements, safety plan
- Full test suites: ztest (C), pytest (Python), Twister (samples)
- CI: GitHub Actions with ruff, mypy, pytest, SPDX checks
- Specsmith governance: full AEE lifecycle (inception → release)
