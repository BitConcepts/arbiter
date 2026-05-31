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


---
## Governance commands (specsmith_run / /specsmith)

All specsmith governance operations should be invoked through the
``specsmith_run`` agent tool or the ``/specsmith`` REPL slash command.

**In the Nexus REPL:**

```
/specsmith save               # backup + commit + push governance state
/specsmith load               # pull + restore governance state
/specsmith audit --strict     # strict governance audit
/specsmith status             # show governance status
/specsmith push               # git push governance changes
/specsmith pull               # git pull governance changes
/specsmith sync               # full two-way sync
/specsmith watch              # watch CI and block until green
```

**Verb shortcuts** (single word, no prefix needed in tool calls):
``save``, ``load``, ``push``, ``pull``, ``sync``, ``audit``, ``status``,
``watch``, ``commit``, ``validate``, ``doctor``, ``run``.

These are all equivalent: ``specsmith_run("save")``,
``specsmith_run("/specsmith save")``, ``specsmith_run("specsmith save")``.


---
## Session Governance Protocol

This section is non-negotiable. Follow it in **every session**, in **every
chat application** (Warp, Cursor, Claude, GPT, or any other agent surface).

### Session start (run once, output result verbatim)

```bash
specsmith kill-session 2>/dev/null || true   # kill orphaned processes
specsmith audit --project-dir .              # verify governance health
specsmith sync --project-dir .              # confirm machine state
specsmith checkpoint --project-dir .        # emit GOVERNANCE ANCHOR
```

**Output the `specsmith checkpoint` block verbatim as your first response.**

### Before every code change

```bash
specsmith preflight "<describe the change>" --json
```

- `decision == "accepted"` → proceed; note the `work_item_id`.
- `decision == "needs_clarification"` → surface the `instruction` first.
- **Never make a code change without an accepted preflight.**

### Governance heartbeat (every 8–10 turns, or when context feels compressed)

```bash
specsmith checkpoint --project-dir .
```

Output the GOVERNANCE ANCHOR block verbatim in your response, tagged:

```
⎠ GOVERNANCE ANCHOR:
<paste checkpoint output here>
```

### When producing any context summary

1. Run `specsmith checkpoint` first.
2. Place the GOVERNANCE ANCHOR at the **top** of the summary.
3. Never omit phase, work items, or health status from a summary.

### Drift detection — if you cannot answer these from memory, you have drifted

- What is the current AEE phase?
- What work item is active?
- What was the last preflight decision?
- Is the audit currently healthy?

If any answer is unknown: **run `specsmith checkpoint` and re-anchor immediately.**

### Session end

```bash
specsmith save --project-dir .   # ESDB backup + commit + push
specsmith kill-session           # stop governance-serve and tracked processes
```

Never end a session with uncommitted governance changes.

### Quick reference

| When | Command |
|---|---|
| Session start | `specsmith audit && specsmith sync && specsmith checkpoint` |
| Before any code change | `specsmith preflight "<intent>" --json` |
| Every 8–10 turns | `specsmith checkpoint` (output verbatim) |
| Context summary | Checkpoint output at top |
| Session end | `specsmith save && specsmith kill-session` |
| Drift detected | `specsmith checkpoint` immediately |
