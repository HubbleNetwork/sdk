.. _hubble_release_notes:

Release Notes and Lifecycle
############################

This document describes the Hubble Network SDK release process, versioning
strategy, and support policies. Understanding these policies helps developers
plan their integration efforts and make informed decisions about when to upgrade
to new releases.

Overview
********

The Hubble Network SDK follows a structured release lifecycle designed to ensure
stability, reliability, and backward compatibility. Releases progress through
distinct phases from initial development through release candidates to stable
production releases. The SDK adheres to `Semantic Versioning`_ principles to
clearly communicate the nature and impact of each release.

.. _Semantic Versioning: https://semver.org/

Release Lifecycle
*****************

The Hubble Network SDK release process consists of three primary phases:

Development Phase
=================

All new development work is integrated into the ``main`` branch. During this
phase:

- New features, enhancements, and experimental functionality are added
- API changes may be introduced
- Breaking changes may occur without prior notice
- No stability guarantees are provided

**Important**: Code from the ``main`` branch is not recommended for production
use. When sufficient changes accumulate or significant new features are ready,
the development team schedules a release and transitions to the release
candidate phase.

Release Candidate Phase
=======================

When a release is scheduled, the following process begins:

**Release Candidate Creation**
  - A release candidate tag (e.g., ``v1.4.0-rc1``) is created from the ``main``
    branch
  - The release candidate is published for testing and validation

**Feature Freeze**
  - The ``main`` branch enters a feature freeze period
  - Only the following types of changes are permitted:

    - Critical bug fixes
    - Test fixes and improvements
    - Documentation updates and corrections
    - Security patches

**Issue Tracking**
  - All known issues are documented and categorized
  - Issues are classified as either:

    - **Release blockers**: Must be resolved before the stable release
    - **Known limitations**: Documented and acceptable for the release

Hardening and Validation Phase
===============================

Before a stable release, comprehensive validation is performed across multiple
dimensions:

**Functional Testing**
  - **Unit Tests**: Validate individual components in isolation

    - Cryptographic primitives and signing flows
    - Message encoding and decoding operations
    - BLE advertising payload formatting (including edge cases, maximum length handling, and truncation scenarios)

  - **Integration Tests**: Verify end-to-end functionality

    - Complete signing and verification workflows between the SDK and reference backend systems
    - Cross-platform compatibility validation
    - Interoperability testing with Hubble Network services

**Quality Assurance**
  - Code review and static analysis
  - Memory safety and resource leak detection
  - Performance benchmarking
  - Documentation review and accuracy verification

**Platform Validation**
  - Testing across all supported platforms (Zephyr, FreeRTOS, ESP-IDF)
  - Hardware-in-the-loop testing where applicable
  - Regression testing against previous releases

Release Model
*************

The Hubble Network SDK uses `Semantic Versioning`_ with the format
``MAJOR.MINOR.PATCH`` (e.g., ``1.4.2``).

Version Components
==================

*MAJOR* Version Increment
  Indicates breaking changes that may require modifications to existing code:

  - Changes to public API function signatures
  - Modifications to public data structures that affect binary compatibility
  - Changes to documented protocol behavior
  - Removal or deprecation of previously supported features

  **Example**: Upgrading from ``v1.x.x`` to ``v2.x.x`` may require code
  changes in your application.

*MINOR* Version Increment
  Indicates backward-compatible feature additions:

  - New API functions or methods
  - New configuration options
  - Enhanced functionality that doesn't break existing code
  - Performance improvements

  **Example**: Upgrading from ``v1.4.x`` to ``v1.5.x`` adds new features but
  maintains compatibility with existing code.

*PATCH* Version Increment
  Indicates backward-compatible bug fixes:

  - Bug fixes that don't change API behavior
  - Security patches
  - Documentation corrections
  - Internal improvements with no user-visible changes

  **Example**: Upgrading from ``v1.4.0`` to ``v1.4.1`` fixes issues without
  introducing new features or breaking changes.

Versioning Rules
================

The following guidelines determine version increments:

- **MAJOR bump**: Required when public structs, function signatures, or
  documented behavior change in ways that could break existing user code
- **MINOR bump**: Used when adding new APIs or options without breaking
  existing functionality
- **PATCH bump**: Applied for bug fixes and security updates that maintain
  backward compatibility for documented use cases

Release Types
*************

The Hubble Network SDK maintains two types of releases:

Stable Releases
===============

Stable releases (e.g., ``v1.4.0``, ``v1.4.1``) represent production-ready
versions of the SDK:

- **Fully validated**: Complete testing and quality assurance processes have
  been completed
- **Production approved**: Suitable for use in production environments
- **Documented**: Comprehensive release notes and migration guides are provided
- **Supported**: Receive bug fixes and security updates according to the
  support policy

Long-Term Support (LTS) Releases
================================

LTS releases (e.g., ``v2.0.x``) are designated stable releases with extended
support periods:

- **Extended lifecycle**: Receive security updates and critical bug fixes for
  an extended period (typically 12-24 months)
- **Target audience**: Designed for high-volume deployments and long-lived
  products that require stability over new features
- **Predictable maintenance**: Regular security patches and critical fixes
  without feature additions
- **Migration path**: Clear upgrade paths documented for transitioning to newer
  LTS or stable releases

LTS releases are explicitly marked in release announcements and documentation.

Support Policy
**************

The Hubble Network SDK maintains different support levels depending on release
type:

Stable  Releases
================

The current and previous releases receive:

- **Security updates**: All security patches
- **Critical bug fixes**: High-severity issues affecting production deployments
- **Documentation updates**: Corrections and clarifications as needed
- **No new features**: Feature additions are not backported to stable releases

Long-Term Support Releases
==========================

LTS releases receive:

- **Security updates**: All security patches
- **Critical bug fixes**: High-severity issues affecting production deployments
- **Documentation updates**: Corrections and clarifications as needed
- **No new features**: Feature additions are not backported to LTS releases

Release Schedule
****************

The Hubble Network SDK follows a predictable release schedule:

Regular Releases
================

- **Major releases**: depending on feature development and user needs
- **Minor releases**: depending on feature development and user needs
- **Patch releases**: Released as needed for bug fixes and security updates
- **Release candidates**: Published 2-4 weeks before stable releases for
  community testing

Release Announcements
=====================

All releases are announced through:

- GitHub releases page
- SDK documentation updates
- Release notes with detailed changelogs
- Migration guides for major version upgrades

For the most current release information, visit the `Hubble Network SDK GitHub
repository`_.

.. _Hubble Network SDK GitHub repository: https://github.com/hubblenetwork/sdk

Migration and Upgrade Guidance
******************************

When upgrading between SDK versions:

- **Review release notes**: Understand changes, deprecations, and new features
- **Check migration guides**: Follow platform-specific upgrade instructions
- **Test thoroughly**: Validate your application with the new SDK version
- **Update dependencies**: Ensure compatible versions of platform dependencies

For detailed migration instructions, refer to the release notes for your target
version.
