# Change Ledger

## 2026-05-31T23:28 — Fix Zephyr Kconfig root symbol
- **Author**: Oz
- **Type**: fix
- **REQs affected**: REQ-BUILD-001, REQ-ARCH-016
- **Summary**: Changed the root module symbol in `subsys/arbiter/Kconfig`
  from lowercase `menuconfig arbiter` / `if arbiter` to uppercase
  `menuconfig ARBITER` / `if ARBITER`, matching all `CONFIG_ARBITER=y`
  test/sample configurations and Zephyr Kconfig conventions. This fixes
  Twister build failures where `CONFIG_ARBITER` was treated as undefined.
- **Status**: complete

## 2026-05-31T22:36 — Benchmarks: build_only → execute in CI
- **Author**: Oz
- **Type**: ci
- **REQs affected**: REQ-BUILD-001
- **Summary**: Removed `build_only: true` from pid_benchmark and kalman_benchmark
  testcase.yaml. Added `harness_config.regex` anchors (final log line of each
  benchmark) so Twister gates pass/fail on full execution. Timing numbers,
  RAM overhead, and engine overhead % are now captured in the uploaded
  twister-out/benchmarks/ artifact on every CI run.
- **Status**: complete

## 2026-05-31T22:29 — Enable full Zephyr Twister CI job
- **Author**: Oz
- **Type**: ci
- **REQs affected**: REQ-BUILD-001
- **Summary**: Replaced commented-out Zephyr stub with a fully working `zephyr` CI job.
  Checkout to `app/` (matching `self.path` in west.yml), `west init -l app`,
  `west update --narrow -o=--depth=1` (Zephyr core only, no HALs, shallow clone),
  and three Twister steps: unit tests (executed on native_sim), benchmarks
  (build_only), and all 17 samples (build_only). West workspace cached by
  `west.yml` hash to keep runner times short. Twister results uploaded as
  artifact on every run. No Zephyr SDK/cross-compiler needed — all tests
  are `platform_allow: native_sim`.
- **Status**: complete

## 2026-05-31 — specsmith import
- Imported project: arbiter
- Detected type: embedded-hardware
- Language: c
- Build system: pyproject

## 2026-05-31T14:21 — Initial implementation: C runtime, Python compiler, Zephyr subsystem, 17 samples, benchmarks, safety artifacts
- **Author**: Oz
- **Type**: feature
- **REQs affected**: REQ-ARCH-001,REQ-ARCH-002,REQ-ARCH-003,REQ-ARCH-008,REQ-SAFETY-001,REQ-BUILD-001
- **Status**: complete
- **Chain hash**: `cab3c661cbde99ad...`

## 2026-05-31T17:03 — test-ran TEST-001: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-001
- **Status**: complete
- **Chain hash**: `260e95483887839d...`

## 2026-05-31T17:03 — test-ran TEST-002: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-002
- **Status**: complete
- **Chain hash**: `a27fae6cd64b978f...`

## 2026-05-31T17:03 — test-ran TEST-003: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-003
- **Status**: complete
- **Chain hash**: `39404b8a4f9cde98...`

## 2026-05-31T17:03 — test-ran TEST-004: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-004
- **Status**: complete
- **Chain hash**: `fce860d44fd7bd02...`

## 2026-05-31T17:03 — test-ran TEST-005: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-005
- **Status**: complete
- **Chain hash**: `dd62eea5b2d38185...`

## 2026-05-31T17:03 — test-ran TEST-006: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-006
- **Status**: complete
- **Chain hash**: `a601c4b4b9bd8472...`

## 2026-05-31T17:03 — test-ran TEST-007: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-007
- **Status**: complete
- **Chain hash**: `33d9e9d768c8a637...`

## 2026-05-31T17:03 — test-ran TEST-008: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-008
- **Status**: complete
- **Chain hash**: `24d083fa23cea00a...`

## 2026-05-31T17:03 — test-ran TEST-009: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-009
- **Status**: complete
- **Chain hash**: `8287c9b697961325...`

## 2026-05-31T17:03 — test-ran TEST-010: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-010
- **Status**: complete
- **Chain hash**: `25222819bd3fd6ef...`

## 2026-05-31T17:03 — test-ran TEST-011: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-011
- **Status**: complete
- **Chain hash**: `71d98b8db889375f...`

## 2026-05-31T17:03 — test-ran TEST-012: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-012
- **Status**: complete
- **Chain hash**: `262167e2e173ddee...`

## 2026-05-31T17:04 — test-ran TEST-013: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-013
- **Status**: complete
- **Chain hash**: `4abb7e56fb2bfa6e...`

## 2026-05-31T17:04 — test-ran TEST-014: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-014
- **Status**: complete
- **Chain hash**: `5f3cc7fbbb7e4973...`

## 2026-05-31T17:04 — test-ran TEST-015: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-015
- **Status**: complete
- **Chain hash**: `f951fa37286e7615...`

## 2026-05-31T17:04 — test-ran TEST-016: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-016
- **Status**: complete
- **Chain hash**: `81cccc74a98acb21...`

## 2026-05-31T17:04 — test-ran TEST-017: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-017
- **Status**: complete
- **Chain hash**: `6f1fd22b998c2795...`

## 2026-05-31T17:04 — test-ran TEST-018: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-018
- **Status**: complete
- **Chain hash**: `1c87f8259586eb1b...`

## 2026-05-31T17:04 — test-ran TEST-019: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-019
- **Status**: complete
- **Chain hash**: `182e67f00ddd3d2e...`

## 2026-05-31T17:04 — test-ran TEST-020: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-020
- **Status**: complete
- **Chain hash**: `702ce2473b15d946...`

## 2026-05-31T17:04 — test-ran TEST-021: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-021
- **Status**: complete
- **Chain hash**: `5a24bb25dde3e4b3...`

## 2026-05-31T17:04 — test-ran TEST-022: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-022
- **Status**: complete
- **Chain hash**: `303ec58f619cc79e...`

## 2026-05-31T17:04 — test-ran TEST-023: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-023
- **Status**: complete
- **Chain hash**: `e57a57ed5f05dd80...`

## 2026-05-31T17:04 — test-ran TEST-024: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-024
- **Status**: complete
- **Chain hash**: `b7a49043ed2cf71f...`

## 2026-05-31T17:04 — test-ran TEST-025: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-025
- **Status**: complete
- **Chain hash**: `15a8138daa0496b4...`

## 2026-05-31T17:04 — test-ran TEST-026: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-026
- **Status**: complete
- **Chain hash**: `ee2bb6718e730373...`

## 2026-05-31T17:04 — test-ran TEST-027: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-027
- **Status**: complete
- **Chain hash**: `44f9933c9d387a0d...`

## 2026-05-31T17:04 — test-ran TEST-028: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-028
- **Status**: complete
- **Chain hash**: `a2c7514ffce939e6...`

## 2026-05-31T17:04 — test-ran TEST-029: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-029
- **Status**: complete
- **Chain hash**: `5724209a893af5fa...`

## 2026-05-31T17:04 — test-ran TEST-030: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-030
- **Status**: complete
- **Chain hash**: `7876be9a6bd9261b...`

## 2026-05-31T17:04 — test-ran TEST-031: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-031
- **Status**: complete
- **Chain hash**: `6f891faca2a4d03d...`

## 2026-05-31T17:04 — test-ran TEST-032: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-032
- **Status**: complete
- **Chain hash**: `36972e9fde28ffac...`

## 2026-05-31T17:04 — test-ran TEST-033: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-033
- **Status**: complete
- **Chain hash**: `0ecad47a937f93f6...`

## 2026-05-31T17:04 — test-ran TEST-034: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-034
- **Status**: complete
- **Chain hash**: `ca42124cb56eade5...`

## 2026-05-31T17:04 — test-ran TEST-035: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-035
- **Status**: complete
- **Chain hash**: `15ec9c182dac2256...`

## 2026-05-31T17:04 — test-ran TEST-036: passed (status: pending → implemented)
- **Author**: specsmith
- **Type**: test-ran
- **REQs affected**: TEST-036
- **Status**: complete
- **Chain hash**: `93b3525850de7bfd...`
