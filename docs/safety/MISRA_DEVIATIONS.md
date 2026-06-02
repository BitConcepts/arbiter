# MISRA C 2012 — Known Deviations

This document records intentional deviations from MISRA C:2012 rules in
the arbiter engine source code (`lib/` and `include/`).  Each deviation
is tracked here with a rationale and scope so that safety audits can
reference this file as the single source of truth.

## Deviation Table

| Rule | Category | Rationale | Location |
|------|----------|-----------|----------|
| 1.1 — C11 required | Required | Zephyr RTOS requires C11 (e.g. `_Static_assert`, `_Atomic`, `<stdnoreturn.h>`). The arbiter engine targets Zephyr exclusively and cannot compile as C90/C99. | All translation units |
| 20.9 — `<stdio.h>` input/output | Required | Zephyr system headers (`<zephyr/kernel.h>`, `<zephyr/logging/log.h>`) are included via angle-bracket syntax and resolved by the Zephyr build system. These are not standard-library I/O headers. | All `#include <zephyr/*.h>` |
| 21.6 — Standard I/O functions | Required | The `LOG_INF`, `LOG_ERR`, `LOG_DBG` macros expand to `printf`-style formatting internally. The arbiter engine never calls `printf`/`fprintf` directly; all logging goes through Zephyr's `LOG_*` API, which may be compiled out via `CONFIG_LOG=n`. | `lib/arbiter_engine.c`, `lib/arbiter_eval.c`, `lib/arbiter_trace.c` |
| 11.3 — Cast between pointer to object and pointer to different object type | Required | The blob loader casts `const uint8_t *` to `const struct arbiter_model *` when deserialising a compiled model blob. The cast is guarded by alignment checks and a SHA-256 hash verification before any field access. | `lib/arbiter_blob.c` — `ARBITER_blob_load()` |
| 14.3 — Controlling expression is always true/false | Required | The `unlikely()` and `likely()` branch-hint macros (from `<zephyr/toolchain.h>`) wrap conditions in `__builtin_expect()`. Static analysers may report the inner expression as invariant. These are intentional performance annotations, not dead code. | `lib/arbiter_eval.c`, `lib/arbiter_fact_store.c` |

## Process

- New deviations must be added to this table **before** the corresponding
  code is merged.
- Each entry must include the MISRA rule number, its category
  (Required / Advisory / Mandatory), a rationale, and the affected
  file(s).
- The CI pipeline runs `cppcheck --addon=misra` with `--suppress` flags
  matching the rules listed above.  Adding a new suppression in CI
  without a corresponding entry here is a policy violation.
- This file is referenced by `docs/COMPLIANCE.md` and the safety case
  documentation in `safety/`.
