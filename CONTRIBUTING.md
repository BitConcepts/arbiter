# Contributing to arbiter

Thank you for your interest in contributing to arbiter! This document provides
guidelines and information for contributors.

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md).
By participating, you are expected to uphold this code.

## How to Contribute

### Reporting Issues

- Use the [GitHub issue tracker](https://github.com/BitConcepts/arbiter/issues).
- Check for existing issues before creating a new one.
- Use the provided issue templates (bug report or feature request).
- Include as much context as possible: Zephyr version, board, compiler, steps
  to reproduce.

### Pull Requests

1. Fork the repository and create a feature branch from `main`.
2. Make your changes following the coding style guidelines below.
3. Add or update tests for your changes.
4. Ensure all tests pass (`pytest tests/python` and `west twister -T tests`).
5. Add SPDX license headers to all new source files.
6. Submit a pull request with a clear description.

## Coding Style

### C Code

- Follow [Zephyr coding style](https://docs.zephyrproject.org/latest/contribute/guidelines.html#coding-style).
- Use fixed-width integer types (`uint32_t`, `int32_t`, etc.).
- No recursion, no unbounded loops, no hidden dynamic allocation.
- Explicit overflow handling.
- `const`-correct model tables.
- Every file must include an SPDX header:
  ```c
  /* SPDX-License-Identifier: MIT */
  ```

### Python Code

- Follow PEP 8 style, enforced by `ruff`.
- Type annotations on all public functions.
- Every file must include an SPDX header:
  ```python
  # SPDX-License-Identifier: MIT
  ```

## Testing

### Python Tests

```sh
python -m pytest tests/python
```

### Zephyr Tests

```sh
west twister -T tests
west twister -T samples
```

## Commit Messages

- Use clear, descriptive commit messages.
- Reference issue numbers where applicable (`Fixes #123`).

## License

By contributing, you agree that your contributions will be licensed under the
MIT License.
