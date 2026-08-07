.. _bare_metal_quick_start:

Bare Metal
##########

Port the Hubble Device SDK to a bare metal environment — one without a
traditional real-time operating system (RTOS) such as FreeRTOS or Zephyr.


Requirements
************

- Device ``key`` (generated when you register a new device to your organization
  through the Hubble Cloud API).
- A **counter source** for EID rotation and key derivation — either device
  uptime (recommended for bare metal, no clock required) or current Unix time.
  See :ref:`bare_metal_counter_source` below.
- Encryption library or custom implementation.


Overview
********

Bare metal implementations run directly on the microcontroller hardware, often
using vendor-provided Hardware Abstraction Layers (HALs) or custom timing
infrastructure. This guide outlines how to compile a basic Bluetooth® Low
Energy (BLE) beacon in a bare metal environment that advertises on the Hubble
Terrestrial Network.

Porting the Hubble Device SDK to a new platform requires

- Including Hubble **source files** and **cryptographic** implementation.
- Choosing a **counter source** (device uptime or Unix time).
- Specific **configuration defines**.
- Implementing two **abstraction layers**: system abstraction and cryptographic
  abstraction.


.. _bare_metal_counter_source:

Counter Source
**************

The SDK derives a rotating **time counter** that drives EID rotation, key
derivation, and BLE address generation. You select where that counter comes
from at compile time. The two sources are mutually exclusive — defining both
produces a build error.

.. list-table::
   :header-rows: 1
   :widths: 20 30 15 35

   * - Counter Source
     - Define
     - Requires a clock?
     - Best for
   * - **Device uptime** *(recommended for bare metal)*
     - ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME``
     - No
     - Devices without a real-time clock (RTC), reliable time source, or a way
       to provision Unix time.
   * - **Unix time**
     - ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME``
     - Yes
     - Devices with an RTC or network time synchronization.

Device Uptime (recommended)
---------------------------

In device uptime mode the counter is derived **purely from elapsed uptime**, so
no time synchronization is required. This is the simplest option for bare metal
hardware that has no RTC and no way to provision the current time. It depends
only on your :ref:`bare_metal_uptime_get` implementation.

The counter advances once per :ref:`EID rotation period
<bare_metal_eid_rotation_period>` (daily by default) and wraps at the EID pool
size of ``128``.

:c:func:`hubble_init` takes the **initial counter value** rather than a
timestamp:

- Pass ``0`` to start at counter epoch 0 (valid).
- Pass a previously saved counter to resume from a known state after a reboot,
  preserving continuity across power cycles.

Use :c:func:`hubble_counter_get` to read the current counter value so your
application can persist it to non-volatile storage and reload it on the next
boot. Its signature is ``int hubble_counter_get(uint32_t *counter)``: it returns
a status code (``0`` on success) and writes the counter value through the
pointer. In device uptime mode the value it returns is the wrapped pool counter
in the range ``0`` to ``127``. That is exactly the form :c:func:`hubble_init`
expects as its initial counter, so saving it and passing it back on the next
boot restores counter continuity across the reboot.

.. code-block:: c

   /* Device uptime mode — start at counter epoch 0 */
   int ret = hubble_init(0, master_key);

   /* Device uptime mode — resume from a saved counter after reboot.
    * hubble_counter_get() writes a uint32_t (the wrapped 0..127 pool
    * value), so store and reload the counter as uint32_t. hubble_init()
    * accepts it as the uint64_t initial counter. */
   uint32_t saved_counter = your_nvs_read_u32("hubble_counter");
   int ret = hubble_init(saved_counter, master_key);

.. tip::

   Because it removes the dependency on time synchronization, device uptime mode
   is the **default recommendation for bare metal**. Choose Unix time only when
   your device already maintains accurate wall-clock time.

Unix Time
---------

In Unix time mode the counter is derived from the current Unix Epoch time, so
the device must know the wall-clock time before it can generate valid
advertisements. :c:func:`hubble_init` takes the **current Unix time in
milliseconds since the Unix epoch**:

- A value of ``0`` is invalid and returns an error.
- Update the time later with :c:func:`hubble_time_set` (for example, after an
  RTC read or a network time sync).

.. code-block:: c

   /* Unix time mode — provide current Unix time in milliseconds */
   uint64_t unix_time_ms = 1705881600000;
   int ret = hubble_init(unix_time_ms, master_key);


Source Files
************

To build the **Hubble Device SDK**, include the following source files in your
project. Ensure your build system includes the SDK's ``include/`` directory in
the header search path.

.. list-table::
   :header-rows: 1
   :widths: 40 60

   * - File
     - Purpose
   * - ``src/hubble.c``
     - Core SDK initialization and counter source (device uptime / Unix time)
       management.
   * - ``src/hubble_ble.c``
     - BLE advertisement packet generation.
   * - ``src/crypto/mbedtls.c`` or ``src/crypto/psa.c``
     - Cryptographic implementation (choose one, or implement and port your
       own).
   * - Your port file(s)
     - Platform-specific implementations of ``sys.h`` and optionally
       ``crypto.h`` (see **Abstraction Layers** below).


.. _bare_metal_configuration_defines:

Configuration Defines
*********************

Hubble Device SDK uses compile-time configuration defines. Create a
configuration header or pass these as compiler flags.

.. _bare_metal_required_defines:

Required Defines
----------------

.. code-block:: c

   /* Select the key size. Define exactly one of the following.
    * This choice selects the AES-128 vs AES-256 cipher. */
   #define CONFIG_HUBBLE_NETWORK_KEY_128       /* 128-bit keys */
   /* #define CONFIG_HUBBLE_NETWORK_KEY_256 */ /* 256-bit keys */

   /* Enable BLE network functionality */
   #define CONFIG_HUBBLE_BLE_NETWORK

Define exactly one of ``CONFIG_HUBBLE_NETWORK_KEY_128`` or
``CONFIG_HUBBLE_NETWORK_KEY_256``. If neither is defined the build fails with a
compile error.

Counter Source Defines
----------------------

Select exactly one counter source (see :ref:`bare_metal_counter_source`).
Defining both produces a build error; defining neither falls back to the SDK
default of Unix time.

.. code-block:: c

   /* Recommended for bare metal: derive the counter from device uptime.
    * No real-time clock or time provisioning required. */
   #define CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME

   /* Alternative: derive the counter from Unix time (requires a clock). */
   /* #define CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME */

.. _bare_metal_eid_rotation_period:

The EID rotation period controls how often the counter advances. Currently only
daily rotation (``86400`` seconds) is supported for backend compatibility.

.. code-block:: c

   /* EID rotation period in seconds (currently only 86400 / daily is supported) */
   #define CONFIG_HUBBLE_EID_ROTATION_PERIOD_SEC 86400

.. _bare_metal_optional_defines:

Optional Defines
----------------

.. code-block:: c

   /* Strict nonce reuse checking. This is ENABLED BY DEFAULT in the SDK
    * (the Kconfig default is y). A bare-metal build has no Kconfig
    * defaults, so define it explicitly to keep the check active.
    * Leaving it out disables the check, which is strongly discouraged
    * outside of testing. */
   #define CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK

   /* Enable custom sequence counter implementation */
   #define CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM


Abstraction Layers
******************

Two abstraction layers must be implemented to port Hubble Device SDK:

#. **System Abstraction** (``sys.h``) — Provides timing, logging, and optional
   sequence counter functionality.
#. **Cryptographic Abstraction** (``crypto.h``) — Provides AES-CTR encryption,
   AES-CMAC authentication, and secure memory handling.


System Abstraction
------------------

``sys.h``

The system abstraction layer is defined in ``include/hubble/port/sys.h``. You
must implement the following functions:

- ``hubble_uptime_get()``
- ``hubble_log()``
- (Optional) ``hubble_sequence_counter_get()``

.. _bare_metal_uptime_get:

hubble_uptime_get
^^^^^^^^^^^^^^^^^

.. code-block:: c

   uint64_t hubble_uptime_get(void);

Returns the system uptime in milliseconds since boot, and must provide
monotonically increasing values. This function underpins **both** counter
sources: in device uptime mode it is the sole input to the time counter, and in
Unix time mode it is used alongside the provisioned timestamp to track Unix time
between syncs.

**Requirements**

- Return value in milliseconds.
- Must handle timer overflow/wraparound correctly.
- Timing precision requirement: ±500 ppm (0.05%) or better.
- The SDK expects this function to be called from a single execution context.
  Thread safety is not required for bare metal implementations.

**Additional Context for Bare Metal Implementation**

Most microcontrollers provide a hardware timer or system tick counter. Your
implementation should:

- Read the hardware timer/counter value.
- Track overflows to extend beyond the native timer width.
- Convert to milliseconds based on the timer frequency.

**Example Implementation Pattern**

.. code-block:: c

   #include <hubble/port/sys.h>

   /* Example using a 32-bit hardware timer at known frequency */
   uint64_t hubble_uptime_get(void)
   {
       static uint64_t overflow_count = 0;
       static uint32_t last_ticks = 0;

       uint32_t current_ticks = your_hardware_timer_get();

       /* Detect overflow (assumes function called frequently enough) */
       if (current_ticks < last_ticks) {
           overflow_count += (1ULL << 32);
       }
       last_ticks = current_ticks;

       uint64_t total_ticks = current_ticks + overflow_count;

       /* Convert to milliseconds */
       return (total_ticks * 1000ULL) / YOUR_TIMER_FREQUENCY_HZ;
   }

.. note::

   For a real-world example using the **Nordic SoftDevice** with ``app_timer``
   see the ``hubble-reference-nordic-softdevice`` implementation on `GitHub
   <https://github.com/HubbleNetwork/hubble-reference-nordic-softdevice/blob/main/README.md>`_.

hubble_log
^^^^^^^^^^

.. code-block:: c

   int hubble_log(enum hubble_log_level level, const char *format, ...);

Logs a formatted message at the specified severity level. This function is
application-specific and should integrate with your project's logging
infrastructure.

**Log Levels**

.. code-block:: c

   enum hubble_log_level {
       HUBBLE_LOG_DEBUG,
       HUBBLE_LOG_INFO,
       HUBBLE_LOG_WARNING,
       HUBBLE_LOG_ERROR,
   };

**Additional Context for Bare Metal Implementation**

- Minimal/No-op: Return 0 without output (suitable for production with size
  constraints).
- UART/Serial: Format and transmit via UART.
- RTT/SWO: Use debug probe logging (J-Link RTT, SWO, etc.).
- Vendor Logging: Integrate with your MCU vendor's logging framework.

**Example: Minimal Implementation**

.. code-block:: c

   int hubble_log(enum hubble_log_level level, const char *format, ...)
   {
       (void)level;
       (void)format;
       return 0;
   }

**Example: UART Implementation**

.. code-block:: c

   #include <hubble/port/sys.h>
   #include <stdarg.h>
   #include <stdio.h>

   int hubble_log(enum hubble_log_level level, const char *format, ...)
   {
       static const char *level_names[] = {"DBG", "INF", "WRN", "ERR"};
       char buffer[128];
       va_list args;

       int offset = snprintf(buffer, sizeof(buffer), "[%s] ", level_names[level]);

       va_start(args, format);
       int len = vsnprintf(buffer + offset, sizeof(buffer) - offset, format, args);
       va_end(args);

       if (len > 0) {
           your_uart_write(buffer, offset + len);
       }

       return 0;
   }

hubble_sequence_counter_get
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   uint16_t hubble_sequence_counter_get(void);

Optional function returns a 10-bit sequence counter (0–1023) used for key
rotation, nonce generation, and BLE address derivation. The counter should
increment with each BLE advertisement and wrap from 1023 back to 0.

**Default Behavior**

If you do not define ``CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM``, the SDK
provides a default implementation that maintains a simple incrementing counter
in RAM. This is suitable for most bare metal applications.

**When to Implement Custom**

Using ``hubble_sequence_counter_get`` is optional. Implement this function if
you need:

- Persistence across device resets (requires non-volatile storage)
- Synchronization with external systems
- Custom counter management logic

**Example: Custom Implementation with Non-Volatile Storage (NVS)**

.. code-block:: c

   #define CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM

   uint16_t hubble_sequence_counter_get(void)
   {
       static uint16_t counter = 0;
       static bool initialized = false;

       if (!initialized) {
           /* Load from non-volatile storage on first call */
           counter = your_nvs_read_u16("hubble_seq");
           initialized = true;
       }

       uint16_t current = counter;
       counter = (counter + 1) & HUBBLE_BLE_MAX_SEQ_COUNTER;  /* 0x3FF = 1023 */

       /* Persist periodically or on significant changes */
       your_nvs_write_u16("hubble_seq", counter);

       return current;
   }


Cryptographic Abstraction
-------------------------

``crypto.h``

The cryptographic abstraction layer is defined in
``include/hubble/port/crypto.h``. The SDK requires AES-CTR encryption and
AES-CMAC authentication.

Using a Library


The Hubble Device SDK includes ready-to-use implementations for common
cryptographic libraries. To use a provided implementation, include the matching
**source file** in your build and link against its underlying crypto library.

.. note::

   In Kconfig-based builds the crypto backend is chosen with the
   ``CONFIG_HUBBLE_NETWORK_CRYPTO_MBEDTLS`` /
   ``CONFIG_HUBBLE_NETWORK_CRYPTO_PSA`` /
   ``CONFIG_HUBBLE_NETWORK_CRYPTO_CUSTOM`` choice
   (``CONFIG_HUBBLE_NETWORK_CRYPTO_PSA`` is the default), and the build system
   compiles the matching file for you. The crypto source files themselves do
   **not** test that symbol, so a bare-metal build makes the same selection
   simply by compiling exactly one of ``src/crypto/mbedtls.c``,
   ``src/crypto/psa.c``, or your own implementation. You do not need to define
   ``CONFIG_HUBBLE_NETWORK_CRYPTO_*`` for the source file to build. Note that
   both ``mbedtls.c`` and ``psa.c`` select the AES cipher from the
   ``CONFIG_HUBBLE_NETWORK_KEY_128`` / ``CONFIG_HUBBLE_NETWORK_KEY_256`` choice,
   so that define must still be set as described under
   :ref:`bare_metal_required_defines`.

.. list-table::
   :header-rows: 1
   :widths: 20 35 45

   * - Library
     - Source File
     - Detail
   * - **Mbed TLS**
     - ``src/crypto/mbedtls.c``
     - Widely supported, good balance of size and features.
   * - **PSA Crypto API**
     - ``src/crypto/psa.c``
     - ARM's standard crypto API, supports hardware acceleration.

For most bare metal projects, **we recommend using Mbed TLS or the PSA Crypto
API.**

- Both are actively maintained and widely audited.
- Support hardware acceleration on many platforms.
- Provide configurable builds to minimize code size.
- Available from most MCU vendors' SDKs.

.. tip::

   We particularly recommend **PSA Crypto API** if your platform supports it, as
   it provides a standardized interface that can leverage hardware crypto
   accelerators when available.

An alternative is **wolfSSL**. This option offers small footprint optimizations
(``--enable-smallstack``, ``--enable-minimal``) and professional support.
However it may require a commercial license for some use cases.

**We do not recommend TinyCrypt.** While some developers have used TinyCrypt due
to its small size, we do not officially support it and recommend against its
use.

- No longer maintained: The project has been abandoned by Intel.
- Limited side-channel protections: TinyCrypt implements only generic
  timing-attack countermeasures, making it potentially vulnerable on platforms
  where side-channel attacks are a concern.
- Known vulnerabilities: Static analysis has identified issues including integer
  overflows (CWE-190) and invalid pointer arithmetic (CWE-823).
- Limited algorithm support: Only supports AES-128, not AES-256.

.. note::

   If code size is a critical constraint, consider using **Mbed TLS** with a
   minimal configuration or the **PSA Crypto API** with hardware acceleration.

Custom Implementation
^^^^^^^^^^^^^^^^^^^^^

If you need to implement the crypto layer yourself (for example, to use hardware
crypto peripherals directly), implement the Hubble Cryptographic Functions API
(see the :ref:`hubble_core_api` reference).


Complete Example
****************

Below is a complete example of a minimal bare metal port:

.. code-block:: c

   /* hubble_port_bare_metal.c */

   #include <hubble/port/sys.h>
   #include <hubble/port/crypto.h>
   #include <stdarg.h>
   #include <string.h>

   /* Include your platform headers */
   #include "your_hardware_timer.h"
   #include "your_uart.h"

   /*
    * System Abstraction
    */

   uint64_t hubble_uptime_get(void)
   {
       static uint64_t overflow_ticks = 0;
       static uint32_t last_ticks = 0;

       uint32_t current = hardware_timer_get_ticks();

       if (current < last_ticks) {
           overflow_ticks += 0x100000000ULL;
       }
       last_ticks = current;

       return ((current + overflow_ticks) * 1000ULL) / TIMER_FREQ_HZ;
   }

   int hubble_log(enum hubble_log_level level, const char *format, ...)
   {
       /* Implement based on your logging needs, or leave as no-op */
       (void)level;
       (void)format;
       return 0;
   }

   /*
    * Note: hubble_sequence_counter_get() uses the SDK's default
    * implementation unless CONFIG_HUBBLE_NETWORK_SEQUENCE_NONCE_CUSTOM
    * is defined.
    */

.. warning::

   For cryptographic functions, link against ``src/crypto/mbedtls.c`` or
   ``src/crypto/psa.c``, or provide your own implementation.


Validation
**********

Test your port by:

#. Verifying ``hubble_uptime_get()`` returns monotonically increasing values.
#. Calling :c:func:`hubble_init` with your key and the appropriate initial value
   for your counter source:

   - **Device uptime mode:** an initial counter value (``0`` to start at epoch
     0, or a saved counter to resume).
   - **Unix time mode:** a known, non-zero Unix timestamp in milliseconds.

#. Generating advertisements with :c:func:`hubble_ble_advertise_get` and
   verifying output length.
#. Checking logs (if implemented) for any error messages.

In your main loop, use :c:func:`hubble_ble_advertise_expiration_get` to decide
when to regenerate the advertisement. It returns the number of milliseconds
until the current advertisement expires, which is the natural signal for a
bare-metal loop to call :c:func:`hubble_ble_advertise_get` again and refresh the
payload in step with the EID rotation period.


Troubleshooting
***************

Linker errors for crypto functions
----------------------------------

- Ensure you've included either ``src/crypto/mbedtls.c``, ``src/crypto/psa.c``,
  or your own crypto implementation.
- Verify the crypto library (Mbed TLS, PSA, etc.) is properly linked.

"CONFIG_HUBBLE_NETWORK_KEY_256 or CONFIG_HUBBLE_NETWORK_KEY_128 must be defined" error
--------------------------------------------------------------------------------------

- Define exactly one of ``CONFIG_HUBBLE_NETWORK_KEY_128`` or
  ``CONFIG_HUBBLE_NETWORK_KEY_256`` in your build or config header.

"Cannot define both ... COUNTER_SOURCE ..." build error
-------------------------------------------------------

- The two counter sources are mutually exclusive. Define only
  ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`` *or*
  ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME``, not both.

"Unix Epoch time not initialized" warning
-----------------------------------------

- This appears in Unix time mode when no valid time has been set. Call
  :c:func:`hubble_init` with a non-zero Unix timestamp, or call
  :c:func:`hubble_time_set` once a time source is available.
- If your device has no clock, use device uptime mode
  (``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME``) instead, which needs no time
  provisioning.

Timer overflow causing time jumps
---------------------------------

- Ensure ``hubble_uptime_get()`` is called frequently enough to detect
  overflows.
- For 32-bit timers at 1 MHz, overflow occurs every ~71 minutes.

Nonce reuse warnings
--------------------

- If you see "Re-using same nonce is insecure!" warnings, ensure your sequence
  counter is incrementing properly.
- These warnings come from
  ``CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK``, which is enabled by
  default. In a bare-metal build without Kconfig defaults, define it explicitly
  to keep the check active (see :ref:`bare_metal_optional_defines`).
