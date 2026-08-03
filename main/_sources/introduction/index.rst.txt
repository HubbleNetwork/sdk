.. _hubble_introduction:

Introduction
############

The Hubble Device SDK connects resource-constrained embedded devices to the
Hubble Network. It lets firmware transmit encrypted data to Hubble's
infrastructure over two complementary link types — a **Terrestrial Network**
built on Bluetooth® Low Energy (BLE) and a **Satellite Network** — without a
cellular modem, gateway, or persistent internet connection on the device
itself.

The SDK is designed with flexibility and scalability in mind. Its two primary
services — satellite communication and terrestrial (BLE) communication — are
built around a modular architecture that separates the network logic from
platform-specific details. This makes it straightforward to extend the SDK or
adapt it to different environments, including several real-time operating
systems (RTOSes) and bare metal targets.

The Networks
************

The SDK exposes two independent networks. An application can use either one on
its own or combine them in a dual-stack configuration — for example, beaconing
over BLE while waiting for a satellite pass.

Terrestrial Network (BLE)
=========================

The :ref:`Hubble Terrestrial Network <hubble_terrestrial_introduction>` is
built on Bluetooth Low Energy. The SDK generates encrypted BLE advertisement
packets that nearby Hubble-aware receivers pick up and forward to the network.
This module uses the standard Bluetooth protocol and does not take ownership of
any hardware — the application keeps control of its Bluetooth stack and simply
transmits the advertisements the SDK produces.

Satellite Network
=================

The :ref:`Hubble Satellite Network <hubble_satellite_introduction>` lets devices
send short messages directly to Hubble's satellites, extending connectivity
beyond terrestrial coverage. The application decides *when* to transmit —
continuously when power is not a constraint, or only during a predicted
satellite pass to conserve energy. Because it drives a radio, the satellite
module assumes ownership of the target hardware while transmitting.

Key Characteristics
*******************

Modular architecture
   The :ref:`service modules <hubble_architecture>` (satellite and BLE) sit on
   top of a thin port layer, so the same network logic runs across different
   platforms with only the port implementation changing.

Portable
   First-class support for Zephyr, with additional integrations for FreeRTOS,
   TI SimpleLink/SysConfig, ESP-IDF, and bare metal targets. The port layer
   makes it possible to bring the SDK to environments that are not natively
   supported.

Secure by design
   All transmitted data is protected with 128- or 256-bit encryption, and
   packet identifiers rotate over time to protect privacy. See
   :ref:`security_section` for the security model.

Lightweight
   The SDK is written in C for resource-constrained microcontrollers and depends
   only on a cryptographic backend (such as Mbed TLS or the PSA Crypto API) that
   you select at build time.

Platform Support
****************

.. list-table::
   :widths: 30 20 50
   :header-rows: 1

   * - Platform
     - Quick Start
     - Notes
   * - Zephyr
     - :ref:`zephyr_quick_start`
     - Best-in-class support and reference implementation.
   * - FreeRTOS
     - :ref:`freertos_quick_start`
     - FreeRTOS support with user-managed build integration.
   * - TI SimpleLink / SysConfig
     - :ref:`ti_quick_start`
     - TI SimpleLink SDK + SysConfig integration.
   * - ESP-IDF
     - :ref:`esp_idf_quick_start`
     - ESP-IDF support with component-based integration.
   * - Bare metal
     - :ref:`bare_metal_quick_start`
     - No RTOS; you implement the system and crypto port layers.

Where to go next
****************

* :ref:`hubble_architecture` — how the SDK is structured, its service modules,
  the port layer, and the source-tree code organization.
* :ref:`hubble_quickstart` — build and run a sample application on your platform:
  :ref:`zephyr_quick_start`, :ref:`freertos_quick_start`,
  :ref:`ti_quick_start`, :ref:`esp_idf_quick_start`, or
  :ref:`bare_metal_quick_start`.
* :ref:`hubble_terrestrial_introduction` and
  :ref:`hubble_satellite_introduction` — the two networks in depth, with their
  APIs and workflows.
* :ref:`hubble_integration_guides` — end-to-end guides for integrating the SDK
  into a full application.
* :ref:`hubble_best_practices` — guidance on time management and other
  operational concerns.
* :ref:`hubble_configuration` — the full set of tunable ``CONFIG_HUBBLE_*``
  options.
* :ref:`hubble_api_reference` — the complete API reference.
