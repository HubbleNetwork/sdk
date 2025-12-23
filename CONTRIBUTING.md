# Contributing to Hubble Network SDK

Thank you for your interest in contributing to the Hubble Network SDK.

This document outlines the contribution guidelines and processes for the Hubble Network
SDK. Following these guidelines helps ensure consistency, quality, and maintainability
across the codebase while facilitating collaboration among contributors.

## Code of Conduct

This project and everyone participating in it is governed by our
[Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to
uphold this code. Please report unacceptable behavior to support@hubble.com.

## I Have a Question

Before asking a question:

- Check the [Hubble Network SDK Documentation](https://docs.hubble.com/docs/device-sdk/intro)
- Search existing [Issues](https://github.com/hubblenetwork/hubble-device-sdk/issues)
- Review the codebase and examples in the `samples/` directory

If you still have questions, please start a discussion in [GitHub Discussions](https://github.com/hubblenetwork/hubble-device-sdk/discussions).
When asking a question, please provide:
- Context about what you're trying to accomplish
- Platform and version information (target board, SDK version, RTOS version)
- Relevant code snippets or error messages

## I Want To Contribute

### Legal Notice

When contributing to this project, you must agree that you have authored 100% of the
content, that you have the necessary rights to the content, and that the content you
contribute may be provided under the project license (Apache 2.0).

### Developer Certificate of Origin (DCO)

This project follows the Developer Certificate of Origin (DCO) process to ensure
developers are following licensing criteria for their contributions. The DCO is a
legally binding statement asserting that you are the creator of your contribution,
or that you otherwise have the authority to distribute the contribution, and that
you are intentionally making the contribution available under the Apache 2.0 license.

You agree to the DCO by including a `Signed-off-by` line at the end of your commit
message. If you are willing to agree to the DCO terms, add this line to every git
commit message:

```
Signed-off-by: Your Name <your.email@example.com>
```

You can sign your commit automatically with `git commit -s` if you have configured
your `user.name` and `user.email` in git.

### License

The Hubble Network SDK uses the Apache 2.0 license. All contributions must be
compatible with this license. Each source file must include the SPDX license
identifier:

```c
/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
```

## Reporting Bugs

### Before Submitting a Bug Report

- Make sure you are using the latest supported version
- Check if there is already a bug report for your issue in the
  [bug tracker](https://github.com/hubblenetwork/hubble-device-sdk/issues?q=label%3Abug)
- Collect information about the bug:
  - Stack trace or error messages
  - Platform and version (Zephyr, ESP-IDF, FreeRTOS, board, SDK version)
  - Steps to reproduce the issue
  - Expected vs actual behavior
  - Relevant code snippets or configuration

> **Security Issues**: We take security very seriously. Please **DO NOT** publicly
> report security-related issues, vulnerabilities, or bugs. Instead, send your
> findings by email to **security@hubble.com**. We appreciate your research and will
> work with you to resolve the situation ASAP.

### How Do I Submit a Good Bug Report?

- Open a [Bug Report](https://github.com/hubblenetwork/hubble-device-sdk/issues/new/choose)
- Complete the Bug Report form with all relevant information
- Provide clear reproduction steps that someone else can follow
- Include a minimal test case if possible
- Tag the issue with appropriate labels (platform, component, etc.)

## Suggesting Enhancements

Enhancement suggestions include new features and improvements to existing functionality.

### Before Submitting an Enhancement

- Make sure you are using the latest version
- Read the [documentation](https://docs.hubble.com/docs/device-sdk/intro) carefully
- Search existing [Issues](https://github.com/hubblenetwork/hubble-device-sdk/issues) to see
  if the enhancement has already been suggested
- Consider whether your idea fits with the scope and aims of the project
- Think about whether the enhancement would be useful to the majority of users

### How Do I Submit a Good Enhancement Suggestion?

- Open an [Enhancement Request](https://github.com/hubblenetwork/hubble-device-sdk/issues/new/choose)
- Use a clear and descriptive title
- Provide a step-by-step description of the suggested enhancement
- Describe the current behavior and explain the expected behavior
- Explain why this enhancement would be useful to SDK users
- Consider cross-platform compatibility (Zephyr, ESP-IDF, FreeRTOS)

## Commit Guide

Each commit message should follow this format:

* A short and descriptive subject line (max 72 characters) with a prefix that
  identifies the subsystem being changed, followed by a colon
* A blank line
* A body which explains the reason for the change ("why" the code is changed)
* A blank line
* A `Signed-off-by: <name> <email>` line (automatic with `git commit -s`)

### Commit Message Format

```
<subsystem>: <component>: <brief description>

<Detailed explanation of why this change was made, what problem it solves,
and any relevant context. This should explain the "why" not just the "what".>

Signed-off-by: Your Name <your.email@example.com>
```

### Subsystem Prefixes

Common subsystem prefixes used in this project:
- `port:` - Platform-specific port implementations (zephyr, esp-idf, freertos)
- `src:` - Core SDK source code
- `include:` - Public API headers
- `docs:` - Documentation changes
- `samples:` - Sample applications
- `tests:` - Test code
- `ci:` - Continuous integration changes
- `tools:` - Development tools

### Examples

```
port: sys: Add timer API

Add support for periodic timer in the sys interface. This enables
applications to schedule periodic callbacks without requiring
platform-specific timer APIs, improving portability across ESP-IDF,
Zephyr, and FreeRTOS platforms.

Signed-off-by: Flavio Ceolin <flavio@hubble.com>
```

```
src: ble: Fix sequence counter overflow handling

The sequence counter was not properly wrapping when reaching the
maximum value, causing encryption failures. This fix ensures proper
modulo arithmetic to maintain the 10-bit counter range.

Fixes #123

Signed-off-by: John Contributor <john@example.com>
```

### Commit Message Rules

- Subject line should not exceed 72 characters
- Subject line should not end with a period
- Use imperative mood ("Add feature" not "Added feature")
- Body lines should wrap at 100 characters
- Do not include words like "WIP", "test", or "temp" in the subject
- If the change addresses an issue, include `Fixes #<issue_number>` in the body

## Code Style

### Formatting

This project uses `clang-format` for code formatting. The formatting rules are
defined in [`.clang-format`](.clang-format). All code must be formatted according
to these rules before submission.

To check formatting:
```bash
clang-format --dry-run --Werror <file>
```

To format code:
```bash
clang-format -i <file>
```

Or format all changed files:
```bash
git clang-format
```

### Naming Conventions

- Use `lower_snake_case` for functions, variables, and file names
- Use `UPPER_SNAKE_CASE` for constants, enum values, and macros
- Use descriptive names; avoid cryptic abbreviations
- Prefix static functions with `_` to distinguish from public APIs
- Append units to variable names when relevant (e.g., `timeout_ms`, `size_bytes`)

### Code Style Guidelines

- **Indentation**: Use tabs for indentation (as configured in `.clang-format`)
- **Line Length**: Maximum 100 characters
- **Functions**: Should generally be less than 80 lines
- **Comments**: Use `/* */` style for block comments, explain "why" not "what"
- **Pointers**: Pointers should hug the variable: `char *ptr` not `char* ptr`
- **Braces**: Use consistent brace style (as configured in `.clang-format`)

### Platform-Specific Code

When adding platform-specific code:

- Place implementations in the appropriate `port/` subdirectory:
  - `port/zephyr/` for Zephyr RTOS
  - `port/esp-idf/` for ESP-IDF
  - `port/freertos/` for FreeRTOS
- Ensure all platforms implement the same API interface
- Test changes on at least one platform before submitting
- Document any platform-specific limitations or requirements

### Header Files

- Use `#ifndef` guards
- Include system headers with `<>` and local headers with `""`
- Include headers in this order:
  1. System headers (`<stdio.h>`, `<stdint.h>`, etc.)
  2. Platform headers (`<zephyr/kernel.h>`, etc.)
  3. SDK headers (`<hubble/port/sys.h>`, etc.)
  4. Local headers (`"port_types.h"`, etc.)

### Error Handling

- Check pointers for NULL before dereferencing
- Return appropriate error codes (negative errno values)
- Use consistent error handling patterns across the codebase
- Document error conditions in function documentation

### Documentation

- Use Doxygen-style comments for public APIs
- Document function parameters, return values, and error conditions
- Include usage examples for complex APIs
- Keep documentation up-to-date with code changes

## Testing

- Add tests for new functionality when possible
- Ensure existing tests pass before submitting
- Test on multiple platforms when applicable
- Include test cases in `tests/` directory
- Update sample code if API changes affect usage

## Pull Request Process

1. **Fork the repository** and create a branch from `main`
2. **Make your changes** following the code style guidelines
3. **Test your changes** on at least one platform
4. **Format your code** using `clang-format`
5. **Write clear commit messages** following the commit guide
6. **Sign your commits** with `git commit -s`
7. **Push to your fork** and open a Pull Request
8. **Ensure CI passes** - all checks must pass before merging
9. **Respond to feedback** from reviewers promptly

### Pull Request Checklist

- [ ] Code follows the style guidelines
- [ ] Code is formatted with `clang-format`
- [ ] Commit messages follow the commit guide
- [ ] All commits are signed-off (`Signed-off-by`)
- [ ] Tests pass (if applicable)
- [ ] Documentation is updated (if applicable)
- [ ] Changes are tested on at least one platform
- [ ] PR description explains the change and why it's needed

## Continuous Integration

The project uses GitHub Actions for continuous integration. All pull requests must
pass CI checks before merging. CI checks include:

- Code formatting (`clang-format`)
- Commit message linting (`gitlint`)
- Build verification on multiple platforms
- Documentation builds
- Static analysis (where applicable)

## Improving Documentation

- Documentation source is in the `docs/` directory
- Use reStructuredText (`.rst`) format for Sphinx documentation
- Keep API documentation synchronized with code changes
- Include code examples in documentation when helpful
- Update `README.md` if project structure or setup changes

## Questions?

If you have questions about contributing, please:

- Check existing [GitHub Discussions](https://github.com/hubblenetwork/hubble-device-sdk/discussions)
- Review the [documentation](https://docs.hubble.com/docs/device-sdk/intro)
- Start a new discussion in [GitHub Discussions](https://github.com/hubblenetwork/hubble-device-sdk/discussions)

Thank you for contributing to the Hubble Network SDK! 🚀
