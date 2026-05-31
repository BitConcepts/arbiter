# arbiter — Agent Governance

## Project Summary
arbiter is a deterministic reasoning and safety-policy engine for Zephyr RTOS.
It provides a YAML model language (ARB), a Python compiler (`arbiterc`), a C
runtime engine, and Zephyr subsystem integration.

- **Languages**: C (runtime engine), Python (compiler toolchain), YAML (models)
- **Build system**: CMake/Zephyr (C), pyproject.toml (Python)
- **Test frameworks**: ztest (C), pytest (Python), Twister (samples)
- **CI**: GitHub Actions
- **License**: MIT

## Repository Layout
- `include/arbiter/` — Public C API headers
- `lib/` — C runtime library sources
- `lib/arb/` — Reusable ARB include fragments
- `subsys/arbiter/` — Zephyr subsystem (shell, runtime thread, watchdog)
- `python/arbiter/` — Python compiler package
- `schema/` — ARB JSON Schema
- `samples/` — 17 sample applications (PID, Kalman, power budget, etc.)
- `tests/zephyr/` — C unit tests (ztest)
- `tests/python/` — Python tests (pytest)
- `tests/benchmarks/` — Engine vs hand-coded C benchmarks
- `safety/` — Safety manual, requirements, plan
- `docs/` — Governance and user documentation

## Workflow Rules
1. Read AGENTS.md fully before starting any task.
2. Log all changes in LEDGER.md.
3. Map changes to requirements in docs/REQUIREMENTS.md.
4. Verify against docs/TESTS.md.
5. All C code uses Zephyr coding style (tabs, K&R braces).
6. All files carry `SPDX-License-Identifier: MIT` header.
7. Safety-relevant changes must update `safety/` artifacts.
8. ARB model changes must pass `arbiterc validate`.
9. New samples require: model YAML, main.c, CMakeLists.txt, prj.conf, sample.yaml, testcase.yaml.
10. Commits include `Co-Authored-By: Oz <oz-agent@warp.dev>` when AI-assisted.

## Architecture Reference
See `docs/ARCHITECTURE.md` for component breakdown, data flow, and requirement IDs.

## Safety Constraints
- Engine uses no dynamic allocation (no malloc/free).
- All arithmetic uses 64-bit widening to prevent overflow.
- Safety guards always evaluated before other rule classes.
- Model integrity verified via SHA-256 hash.
