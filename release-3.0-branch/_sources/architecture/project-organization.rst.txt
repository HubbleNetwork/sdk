.. _project_organization:

Project Organization
####################

The Hubble Device SDK is organized around a small common core, public headers,
and platform-specific ports. The common code implements the SDK protocols and
state management. The port code adapts those protocols to a specific RTOS,
crypto provider, radio driver, and board.

This page describes how the source tree is laid out, how the layers depend on
one another, the naming conventions used throughout, and where new code should
go.

Layering Model
**************

Everything in the SDK follows one rule: **dependencies flow downward through the
layers**. A layer may call the layer below it, never the layer above.

.. code-block:: text

   application
      |
      v
   include/hubble          public APIs
      |
      v
   src                     common SDK implementation
      |
      v
   include/hubble/port     port contracts
      |
      v
   port/<rtos>             RTOS / framework glue
      |
      v
   port/<rtos>/boards      board and hardware implementation

Applications call the public ``hubble_*`` APIs. Common SDK code implements the
protocol and calls *port contracts* when it needs a platform service. Port and
board files implement those contracts using RTOS, vendor SDK, or hardware
facilities. When common code needs a new platform capability, the fix is to add
a port contract — not to scatter RTOS-specific conditionals through ``src``.

Keeping this direction intact is what lets the protocol logic be tested and
reused across every supported platform.

Directory Layout
****************

Each directory maps to one layer of the model above and owns a specific
responsibility.

Public APIs -- ``include/hubble``
=================================

Application-facing headers. Applications and ports include SDK interfaces from
here instead of reaching into ``src``. These declarations are the stable
integration surface, use the ``hubble_*`` namespace, and must not leak RTOS or
board-specific types into their signatures.

Key headers:

* ``hubble.h``: top-level SDK initialization, time, key, and counter APIs.
* ``ble.h``: BLE Network advertisement APIs.
* ``sat.h``: Satellite Network packet transmission API.
* ``version.h``: SDK version information.

Port Contracts -- ``include/hubble/port``
=========================================

The interfaces a platform integration must implement. These headers are public
in the sense that ports implement them, but they are not application-facing
APIs. Common SDK code calls them; applications normally do not.

The current port contracts are:

* ``sys.h``: system services such as uptime, logging, sequence
  counters, and random bytes.
* ``crypto.h``: cryptographic provider interface used by the common SDK code.
* ``sat_radio.h``: satellite radio operations implemented by each satellite
  platform.

Common Implementation -- ``src``
================================

Behavior that should be identical on every platform: input validation, BLE
advertisement formatting, satellite packet formatting and send policy, key
derivation, nonce-safety checks, encryption and authentication flow,
pass-prediction math, and the feature logic guarded by ``CONFIG_HUBBLE_*``
options.

Common code may include public SDK headers, private ``src`` headers, and port
contract headers. It must **not** include RTOS or board headers directly.

Important files:

* ``hubble.c``: SDK initialization and common time/key entry points.
* ``hubble_ble.c``: BLE advertisement construction.
* ``hubble_crypto.c``: common key derivation, nonce validation, encryption, and
  authentication flow.
* ``hubble_sat.c``: common satellite send and DTM flow.
* ``hubble_sat_packet.c``: satellite packet encoding.
* ``hubble_sat_pass_prediction.c``: pass prediction logic.
* ``crypto/``: concrete crypto provider adapters, currently mbedTLS and PSA.
* ``utils/`` and ``reed_solomon_encoder.*``: internal helpers used by common
  protocol code.
* ``hubble_priv.h``: private declarations shared only by common implementation
  files.

RTOS and framework glue -- ``port/<rtos>``
==========================================

Adapts the SDK to an RTOS or framework. Each port selects the common sources it
needs, provides configuration integration, and implements the port contracts
required by the enabled SDK features. Typical responsibilities are build-system
integration, the ``sys.h`` implementation, crypto provider selection and
initialization, satellite radio implementation when satellite support is
enabled, and include-path and configuration injection.

Current ports:

* ``port/zephyr``: Zephyr module integration, Kconfig, common Zephyr system
  glue, Zephyr satellite radio glue, and board/SOC-specific radio support. The
  Zephyr port selects sources from ``port/zephyr/CMakeLists.txt`` based on
  Kconfig options.
* ``port/freertos``: FreeRTOS makefile integration (``hubblenetwork-sdk.mk``),
  default configuration header flow, system glue, and satellite radio glue.
* ``port/esp-idf``: ESP-IDF component-style wrapper and system glue for ESP-IDF
  applications.

Board and Hardware Implementation -- ``port/<rtos>/boards``
===========================================================

The lowest layer. Board files bind the generic satellite radio contract to
concrete SOC peripherals. For example, Zephyr satellite support has board
families under ``port/zephyr/boards`` for Nordic and Silicon Labs targets.

Use board-specific files for the details that cannot be expressed as common SDK
logic:

* vendor radio driver setup;
* RF channel and power mapping;
* front-end module (FEM) control;
* vendor binary library integration;
* SOC-family differences, timing quirks, and board selection logic.

A board implementation should answer *"how this hardware transmits"*, while
``src`` answers *"what the SDK needs to transmit"*. Keep protocol behavior out
of board files.

Samples and Tests
=================

Samples show application integration for each supported environment
(``samples/<rtos>``). Tests exercise SDK behavior and platform builds. The
common code should be structured so protocol behavior can be tested without
duplicating RTOS-specific glue.

Naming and Namespace Conventions
********************************

The prefix of a symbol tells you which layer owns it.

Public and Common Symbols -- ``hubble_*``
=========================================

Names without a more specific internal or port marker are public or common SDK
symbols. If a symbol is declared in ``include/hubble`` and documented on an API
page, it is application-facing; if it is defined in ``src`` without a public
declaration, treat it carefully, since C symbols have global linkage unless
declared ``static``.

Use this namespace for stable application APIs and SDK-wide constants. Examples:

* ``hubble_init()``, ``hubble_time_set()``, ``hubble_time_get()``, and
  ``hubble_key_set()`` from ``hubble.h``;
* ``hubble_ble_advertise_get()`` and
  ``hubble_ble_advertise_expiration_get()`` from ``ble.h``;
* ``hubble_sat_packet_send()`` from ``sat.h``;
* satellite packet, pass prediction, and DTM helpers under ``include/hubble/sat``.

When adding a public API, place its declaration in ``include/hubble``, add
Doxygen comments, keep the name under the ``hubble_*`` namespace, and avoid
leaking RTOS or board-specific types into the signature.

Private Helpers -- ``hubble_internal_*``
========================================

``hubble_internal_*`` symbols are SDK-private helpers declared in
``src/hubble_priv.h``. They connect common implementation files to each other
without exposing those details to applications or ports.

Applications and port implementations should not call ``hubble_internal_*``
functions. If a port needs a capability currently hidden behind one, add an
explicit port contract or public API rather than including ``src/hubble_priv.h``.

Port-Contract Symbols -- ``hubble_*_port_*``
============================================

Symbols with ``_port_`` in the name are implemented by a port or board
layer and called by common SDK code — for example
``hubble_sat_port_init()`` and ``hubble_sat_port_packet_send()``. The
``_port_`` suffix implies there is an equivalent common function that
shares code across all ports, for example ``hubble_sat_packet_send()``.

Not every port contract carries ``_port_`` in its
name. ``hubble_uptime_get()``, ``hubble_log()``, and
``hubble_crypto_*()`` are declared in ``include/hubble/port`` and are
still port-provider APIs by contract. These APIs are implemented
completely in the port layer and share no common code.

When adding port APIs, place declarations under ``include/hubble/port``,
document who calls the function, and keep RTOS-specific types out of the common
contract unless the contract is intentionally limited to one port.

Configuration Conventions
*************************

Feature and provider selection use ``CONFIG_HUBBLE_*`` macros. Zephyr defines
these through Kconfig; FreeRTOS injects a configuration header through the
makefile integration. Other ports should provide equivalent configuration values
before compiling the common sources.

Keep configuration checks close to the feature boundary:

* build files select source files for major features and providers;
* headers expose APIs conditionally only when the feature requires it;
* common implementation code uses ``CONFIG_HUBBLE_*`` for small behavioral
  decisions within an already-selected feature.
