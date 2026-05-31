## Description
Brief description of changes and motivation.

Fixes # (issue number)

## Type of Change
- [ ] Bug fix (non-breaking)
- [ ] New feature (non-breaking)
- [ ] Breaking change
- [ ] Documentation update
- [ ] Safety-relevant change

## Testing
Describe tests run and results.

## Checklist
- [ ] SPDX license headers on all new files (`SPDX-License-Identifier: MIT`)
- [ ] C code follows Zephyr coding style (tabs, K&R braces)
- [ ] Python code passes `ruff check python/` and `mypy python/arbiter/`
- [ ] Tests added or updated and passing (`pytest tests/python -v`)
- [ ] Documentation updated (README, doc/, safety/ if applicable)
- [ ] Requirements updated (`docs/REQUIREMENTS.md`) if new functionality
- [ ] Test coverage updated (`docs/TESTS.md`) for new requirements
- [ ] ARB models validate (`arbiterc validate --strict`)
- [ ] No new warnings
- [ ] Commit messages follow conventional commits format
- [ ] Safety artifacts updated if safety-relevant (`safety/`)
