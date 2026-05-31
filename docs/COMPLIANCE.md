# Compliance Report — arbiter

**Generated:** 2026-05-31

## Project Summary

- **Name**: arbiter
- **Type**: Embedded / hardware (C/C++)
- **Language**: c
- **VCS Platform**: github
- **Spec Version**: 0.11.8

## Verification Tools

- **Lint**: clang-tidy, cppcheck
- **Typecheck**: cppcheck
- **Test**: west twister, ctest, unity
- **Security**: flawfinder
- **Build**: west build, cmake, make
- **Format**: clang-format
- **Compliance**: misra-c

## Requirements Coverage Matrix

**Coverage**: 0/36 (0%)

- ✗ REQ-ARCH-001
- ✗ REQ-ARCH-002
- ✗ REQ-ARCH-003
- ✗ REQ-ARCH-004
- ✗ REQ-ARCH-005
- ✗ REQ-ARCH-006
- ✗ REQ-ARCH-007
- ✗ REQ-ARCH-008
- ✗ REQ-ARCH-009
- ✗ REQ-ARCH-010
- ✗ REQ-ARCH-011
- ✗ REQ-ARCH-012
- ✗ REQ-ARCH-013
- ✗ REQ-ARCH-014
- ✗ REQ-ARCH-015
- ✗ REQ-ARCH-016
- ✗ REQ-ARCH-017
- ✗ REQ-ARCH-018
- ✗ REQ-ARCH-019
- ✗ REQ-ARCH-020
- ✗ REQ-ARCH-021
- ✗ REQ-ARCH-022
- ✗ REQ-ARCH-023
- ✗ REQ-ARCH-024
- ✗ REQ-ARCH-025
- ✗ REQ-ARCH-026
- ✗ REQ-ARCH-027
- ✗ REQ-ARCH-028
- ✗ REQ-BUILD-001
- ✗ REQ-SAFETY-001
- ✗ REQ-SAFETY-002
- ✗ REQ-SAFETY-003
- ✗ REQ-SAFETY-004
- ✗ REQ-SAFETY-005
- ✗ REQ-SAFETY-006
- ✗ REQ-SAFETY-007

## Audit Summary

- **Passed**: 29
- **Failed**: 1
- **Fixable**: 0
- **Status**: Issues found

- ✓ Required file AGENTS.md exists
- ✓ Required file LEDGER.md exists
- ✓ Governance file docs/governance/RULES.md exists
- ✓ Governance file docs/governance/SESSION-PROTOCOL.md exists
- ✓ Governance file docs/governance/LIFECYCLE.md exists
- ✓ Governance file docs/governance/ROLES.md exists
- ✓ Governance file docs/governance/CONTEXT-BUDGET.md exists
- ✓ Governance file docs/governance/VERIFICATION.md exists
- ✓ Governance file docs/governance/DRIFT-METRICS.md exists
- ✓ Recommended file docs/REQUIREMENTS.md exists
- ✓ Recommended file docs/TESTS.md exists
- ✓ Recommended file docs/ARCHITECTURE.md exists
- ✓ Recommended file docs/SPECSMITH.yml exists
- ✓ Recommended file CONTRIBUTING.md exists
- ✓ Recommended file LICENSE exists
- ✗ 36 REQ(s) without test coverage: REQ-ARCH-001, REQ-ARCH-002, REQ-ARCH-003, REQ-ARCH-004, REQ-ARCH-005, REQ-ARCH-006, REQ-ARCH-007, REQ-ARCH-008, REQ-ARCH-009, REQ-ARCH-010, REQ-ARCH-011, REQ-ARCH-012, REQ-ARCH-013, REQ-ARCH-014, REQ-ARCH-015, REQ-ARCH-016, REQ-ARCH-017, REQ-ARCH-018, REQ-ARCH-019, REQ-ARCH-020, REQ-ARCH-021, REQ-ARCH-022, REQ-ARCH-023, REQ-ARCH-024, REQ-ARCH-025, REQ-ARCH-026, REQ-ARCH-027, REQ-ARCH-028, REQ-BUILD-001, REQ-SAFETY-001, REQ-SAFETY-002, REQ-SAFETY-003, REQ-SAFETY-004, REQ-SAFETY-005, REQ-SAFETY-006, REQ-SAFETY-007
- ✓ LEDGER.md has 14 lines (within 2000 threshold)
- ✓ 0 open, 0 closed TODOs
- ✓ AGENTS.md: 47 lines
- ✓ docs/governance/RULES.md: 5 lines
- ✓ docs/governance/SESSION-PROTOCOL.md: 7 lines
- ✓ docs/governance/LIFECYCLE.md: 4 lines
- ✓ docs/governance/ROLES.md: 4 lines
- ✓ docs/governance/CONTEXT-BUDGET.md: 3 lines
- ✓ docs/governance/VERIFICATION.md: 3 lines
- ✓ docs/governance/DRIFT-METRICS.md: 3 lines
- ✓ CI config missing lint tool (lint:clang-tidy); test tool is present. Consider adding clang-tidy.
- ✓ Project type embedded-hardware matches detected structure
- ✓ Trace vault intact (3 seals)
- ✓ Phase 🚀 Release: 50% ready (2 check(s) remaining: CHANGELOG.md has version entry, CHANGELOG.md exists)

## Recent Activity

- `4cd58f2 feat: complete arbiter ARB module implementation`
- `0411420 Initial commit`

**Contributors:**
- 2	Tristen Pierson

## AI System Inventory (REG-010)

### Agent Capabilities
- **run_shell**: Execute a shell command. Safety-checked; destructive commands are blocked.
  *Epistemic claims:* EXEC-001: no python -c for non-trivial code
- **read_file**: Read a text file from the repository.
  *Epistemic claims:* read-only: does not modify files
- **write_file**: Write content to a file (creates or overwrites).
  *Epistemic claims:* modifies filesystem: logged in audit chain
- **patch_file**: Apply a unified diff patch to a file.
  *Epistemic claims:* modifies filesystem: logged in audit chain
- **list_files**: List files matching a glob pattern in a directory.
  *Epistemic claims:* read-only: does not modify files
- **grep**: Search for a pattern in files.
  *Epistemic claims:* read-only: does not modify files
- **git_diff**: Show the git diff for the working tree.
  *Epistemic claims:* read-only: does not modify files
- **git_status**: Show git status for the working tree.
  *Epistemic claims:* read-only: does not modify files
- **run_tests**: Run the project test suite.
  *Epistemic claims:* may modify test artifacts but not source
- **open_url**: Fetch text content from a URL.
  *Epistemic claims:* network: reads external resources
- **search_docs**: Search documentation files in the repo.
  *Epistemic claims:* read-only: does not modify files
- **remember_project_fact**: Store a named fact in the local project index (.repo-index/facts.json).
  *Epistemic claims:* modifies .repo-index/facts.json only
- **run_gcc**: Compile or build with GCC / G++. Pass compiler flags verbatim via *args*. Use *compiler* to select g++, gcc-12, etc.
  *Epistemic claims:* invokes compiler process; may produce build artifacts
- **run_arm_gcc**: Cross-compile for ARM bare-metal (arm-none-eabi-gcc / g++). Set *compiler* to 'arm-none-eabi-g++' for C++.
  *Epistemic claims:* invokes cross-compiler; produces .elf/.bin artifacts
- **run_aarch64_gcc**: Cross-compile for AArch64 Linux (aarch64-linux-gnu-gcc / g++).
  *Epistemic claims:* invokes cross-compiler; produces shared/static libraries
- **run_iar_compiler**: Build an IAR Embedded Workbench project via IarBuild command-line. Provide the .ewp *project_file* path.
  *Epistemic claims:* requires IAR Embedded Workbench installed; produces .out artifacts
- **run_intel_compiler**: Compile with Intel oneAPI (icx/icpx) or classic (icc/icpc) compilers. Use *compiler* to select the binary.
  *Epistemic claims:* requires Intel oneAPI or classic compiler installed
- **run_clang_format**: Run clang-format on source files. Use *in_place=True* to apply changes, or leave False to print the diff only.
  *Epistemic claims:* modifies source files in-place when in_place=True
- **run_clang_tidy**: Run clang-tidy static analysis on source files. Pass *checks* to filter specific lint rules.
  *Epistemic claims:* read-only analysis unless --fix is passed
- **run_vsg**: Run VSG (VHDL Style Guide) on .vhd/.vhdl files or directories. Use *fix=True* to apply automatic style corrections in place.
  *Epistemic claims:* modifies VHDL source files in-place when fix=True
- **specsmith_run**: Run any specsmith CLI command. Accepts slash-command form ('/specsmith save'), single-word verb shortcuts ('save', 'push', 'pull', 'load', 'sync', 'audit', 'status', 'watch', 'commit', 'validate', 'doctor', 'run'), or the full 'specsmith <args>' form. Use this for all specsmith governance operations.
  *Epistemic claims:* invokes specsmith CLI; may write to .specsmith/ and .chronomemory/; save/push/commit modify git history; load/pull may overwrite local governance state

### Risk Classification
- **EU AI Act tier**: GPAI (general-purpose; systemic risk assessment required if >10^25 FLOP)
- **NIST AI RMF**: GOVERN + MAP + MEASURE + MANAGE controls applied
- **Use-case scope**: software development governance; not Annex III high-risk

### Human Oversight Controls
- Preflight gate: all governed actions require human-language approval
- Kill-switch: `specsmith kill-session` halts all active agent sessions
- Escalation: `specsmith preflight --escalate-threshold <float>` gates low-confidence actions
- Retry budget: `agents_max_iterations` in docs/SPECSMITH.yml bounds self-improvement loops

## Governance File Inventory

- ✓ `AGENTS.md`
- ✓ `LEDGER.md`
- ✗ `docs/SPECSMITH.yml`
- ✓ `scaffold.yml`
- ✓ `docs/REQUIREMENTS.md`
- ✓ `docs/TESTS.md`
- ✓ `docs/ARCHITECTURE.md`
- ✓ `docs/governance/RULES.md`
- ✓ `docs/governance/SESSION-PROTOCOL.md`
- ✓ `docs/governance/LIFECYCLE.md`
- ✓ `docs/governance/ROLES.md`
- ✓ `docs/governance/VERIFICATION.md`
