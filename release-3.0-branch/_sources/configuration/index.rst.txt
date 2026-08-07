.. _hubble_configuration:

Configuration Options
#####################

The configuration options below are derived from the Zephyr Kconfig definitions
but represent the full set of tunable parameters for the Hubble Device SDK.
The same options apply to all supported ports — Zephyr, FreeRTOS, ESP-IDF,
bare-metal, and other RTOSes — though the mechanism for setting them varies by
platform.

On Zephyr, set these in your application's ``prj.conf`` file. On other
platforms, define the equivalent ``CONFIG_`` macros in your build system or
project header.

.. kconfig-autodoc:: ../port/zephyr/Kconfig
