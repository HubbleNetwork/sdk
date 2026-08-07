.. _ti_integration_guide:

TI SimpleLink Integration Guide
################################

This guide walks through integrating the Hubble Network dual-stack Satellite
and BLE application with the `TI SimpleLink Low Power F3 SDK`_.

By the end of this guide you will know how to:

* Integrate the Hubble Satellite and Terrestrial (BLE) network stacks into a
  TI SimpleLink application.
* Obtain ephemeris data and use pass prediction to schedule satellite
  transmissions.
* Transmit data to the Hubble satellite network from a TI device.

Supported Devices and SDK Version
**********************************

The Hubble Device SDK currently supports **SimpleLink Low Power F3 SDK v9.20**.
If you require support for a different version, `contact us <mailto:support@hubble.com>`_.

The following TI SimpleLink device families are supported:

.. list-table::
   :widths: 20 20 60
   :header-rows: 1

   * - Device
     - Family
     - Notes
   * - CC2340R5
     - CC23xx
     - Max 8 dBm; external amplifier required to reach 20 dBm
   * - CC2755P10
     - CC27xx
     - 20 dBm integrated PA

The following toolchain versions are validated against
**SimpleLink Low Power F3 SDK v9.20.00.81**. See the
`SimpleLink Low Power F3 SDK v9.20.00.81 release notes`_ for the full
compatibility matrix. For other patch or build versions, refer to their
respective release notes.

.. list-table::
   :widths: 50 50
   :header-rows: 1

   * - Tool
     - Version
   * - Code Composer Studio
     - 20.4.0
   * - TI ARM Clang Compiler
     - 4.0.4.LTS
   * - SysConfig
     - 1.26.3
   * - UniFlash
     - 9.5.0


.. include:: ../common/prerequisites.rst
   :start-after: hubble-integration-prerequisites

.. include:: ../common/account-setup.rst
   :start-after: hubble-integration-account-setup


SDK Setup
*********

Clone the Hubble Device SDK:

.. code-block:: bash

   git clone https://github.com/HubbleNetwork/hubble-device-sdk.git

Adding the Hubble Device SDK to SysConfig
==========================================

The steps below cover both **Code Composer Studio (CCS)** and a
Makefile-based workflow.

.. tabs::

   .. tab:: CCS

      If you do not have a CCS project yet, refer to the
      `TI CCS Getting Started guide`_ for instructions on creating one.

      #. Right-click your project folder and select **Properties**.
      #. Navigate to **Build** → **Tools** → **SysConfig**.
      #. In the **SysConfig Flags** field, append:

         .. code-block:: none

            -s "/path/to/hubble-device-sdk/.metadata/product.json"

      #. Click the checkmark to confirm, then **Save and Close**.

   .. tab:: Makefile

      Set the following environment variables before building:

      .. code-block:: bash

         export TICLANG_ARMCOMPILER=/path/to/ti-cgt-armllvm
         export SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/path/to/simplelink_lowpower_f3_sdk
         export SYSCONFIG_TOOL=/path/to/sysconfig_cli.sh
         export HUBBLE_NETWORK_SDK=/path/to/hubble-device-sdk

      Pass both product descriptors to the SysConfig CLI:

      .. code-block:: Makefile

         SYSCFG_CMD_STUB = $(SYSCONFIG_TOOL) --compiler ticlang \
             --product $(SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR)/.metadata/product.json \
             -s $(HUBBLE_NETWORK_SDK)/.metadata/product.json

      Generate the configuration files into your build directory:

      .. code-block:: Makefile

         SYSCFG_FILES := $(shell $(SYSCFG_CMD_STUB) \
             --listGeneratedFiles --listReferencedFiles \
             --output $(BUILD_DIR) your-app.syscfg)

         SYSCFG_C_FILES   = $(filter %.c,$(SYSCFG_FILES))
         SYSCFG_H_FILES   = $(filter %.h,$(SYSCFG_FILES))
         SYSCFG_OPT_FILES = $(filter %.opt,$(SYSCFG_FILES))

      Pass the generated ``.opt`` files to the compiler:

      .. code-block:: Makefile

         CFLAGS += $(addprefix @,$(SYSCFG_OPT_FILES))

      Besides this, you will need to pull in the correct header and source files
      from TI SimpleLink. See ``samples/freertos/ti/sat-dual-stack`` for a complete
      reference Makefile for each target device.


.. _ti_sat_project_config:

Project Configuration
*********************

Enable the Hubble dual-stack functionality by adding Hubble configuration
options to your project's ``.syscfg`` file.

.. tabs::

   .. tab:: CCS

      #. Double-click your project's ``.syscfg`` file to open it in the
         SysConfig editor.
      #. In the left pane, scroll to the bottom and locate
         **HUBBLE DEVICE SDK**. Click **+** to add it.
      #. Check **Enable Hubble Satellite Network** and
         **Enable Hubble Terrestrial Network**.
      #. Set **Device time drift retry rate in PPM** to your oscillator's
         PPM rating (check your crystal datasheet). See
         :ref:`hubble_satellite_clock_drift` for details.
      #. **Use FreeRTOS Daemon Hook** is enabled by default to automatically
         set up the BLE and satellite stacks. If you disable it, you must call
         :c:func:`hubble_init` in your application **before** the BLE stack is
         started. More details are in the :ref:`ti_sat_sdk_init` section.
      #. Hover over the **(?)** icon next to each option to learn more, or
         refer to :ref:`hubble_configuration` for the complete options
         reference.
      #. Save the file, then right-click it → **Open With...** → **Text
         Editor** and add the following at the bottom to enable
         both BLE and satellite stacks:

         .. code-block:: javascript

            var hubble_radio = system.getScript("/hubble_radio.js");
            hubble_radio.config_dual_stack("ble");

   .. tab:: Makefile

      Add the following to your ``.syscfg`` file. ``hubble_radio.js`` sets
      up the ``radioconfig`` and the Dynamic Multi-protocol Manager (DMM)
      so the BLE and satellite stacks can share the radio:

      .. code-block:: javascript

         var Hubble = scripting.addModule("/Hubble");
         Hubble.useSatellite   = true;
         Hubble.useTerrestrial = true;
         /* Set to your oscillator's PPM rating (check your crystal datasheet) */
         Hubble.deviceTDR      = 10;

         var hubble_radio = system.getScript("/hubble_radio.js");
         hubble_radio.config_dual_stack("ble");

      Then set the following in your application's ``hubble_config.h``, which
      you need to include along with your source files in the Makefile:

      .. code-block:: c

         #define CONFIG_HUBBLE_SAT_NETWORK              1
         #define CONFIG_HUBBLE_BLE_NETWORK              1
         #define CONFIG_HUBBLE_KEY_SIZE                 16 /* 16 for 128-bit, 32 for 256-bit */
         #define CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME 1
         #define CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR   10 /* Check your oscillator's PPM rating */

      See :ref:`hubble_satellite_clock_drift` for details on TDR and how
      it affects retransmissions.

      Optionally, enable the Hubble FreeRTOS daemon hook to automatically
      set up the BLE and satellite stacks.

      .. code-block:: c

         #define CONFIG_HUBBLE_FREERTOS_DAEMON_HOOK 1

.. note::

   **Crypto:** A custom AES-CMAC and AES-CTR implementation is required.
   Make sure to select the correct crypto options in your ``.syscfg`` file.
   See ``src/hubble_ti_crypto.c`` in the Hubble samples for a reference
   implementation.

.. note::

   Make sure the RCL command symbol in the custom RF settings is generated
   automatically (set **Symbol Name Generation Method** to **Automatic**).
   See ``samples/freertos/ti/sat-dual-stack`` for complete reference
   ``.syscfg`` scripts.

.. include:: ../common/data-requirements.rst
   :start-after: hubble-integration-data-requirements


.. _ti_sat_sdk_init:

.. include:: ../common/sdk-init.rst
   :start-after: hubble-integration-sdk-init

If ``CONFIG_HUBBLE_FREERTOS_DAEMON_HOOK`` is disabled, :c:func:`hubble_init`
must be called **before** ``BLEAppUtil_init``:

.. code-block:: c

   /* Hardware initialization: Board_init(), clocks, peripherals, etc. */

   /*
    * unix_time_ms must be > device uptime in CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME
    * mode. We can use UINT64_MAX as a sentinel value until real Unix time is provisioned.
    */
   hubble_init(UINT64_MAX, master_key);

   /* Update user configuration of the BLE stack */
   user0Cfg.appServiceInfo->timerTickPeriod     = ICall_getTickPeriod();
   user0Cfg.appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();

   BLEAppUtil_init(&criticalErrorHandler, &App_StackInitDoneHandler,
                   &appMainParams, &appMainPeriCentParams);

Once real Unix time and orbital parameters are available (e.g. provisioned
over BLE), call :c:func:`hubble_sat_satellites_set` before starting pass
prediction.


The Pass Prediction Loop
************************

.. include:: ../common/pass-prediction.rst
   :start-after: hubble-integration-pass-prediction

Beacon over BLE While Waiting
==============================

Schedule a timer for the pass window and start BLE advertising while the
device waits. The example below uses ``ClockP``; any timer peripheral that
can schedule a future callback works equally well.

.. code-block:: c

       /*
        * Schedule a ClockP timer for the satellite pass window.
        * Verify configTICK_RATE_HZ: the sleep duration must not exceed
        * the maximum timeout ClockP supports at your tick rate.
         */
       ClockP_setTimeout(&satPassClock,
                         ClockP_usToTicks((pass_info.start - now_ms) * 1000U));
       ClockP_start(&satPassClock);

       /* Get the Hubble beacon payload and start advertising */
       hubble_ble_advertise_get(inputBuf, inputBufLen, outBuf, &outBufLen);
       ble_adv_start();

       /*
        * Block until the pass timer fires. Common approaches are
        * a semaphore, task notification, or event flag.
        */
       SemaphoreP_pend(&satPassSem, SemaphoreP_WAIT_FOREVER);

Transmit to the Satellite
==========================

Stop advertising, then build and send the packet:

.. code-block:: c

       ble_adv_stop();

       err = hubble_sat_packet_get(&packet, NULL, 0);
       if (err != 0) {
           LOG_ERR("Failed to build packet (err %d)", err);
           return err;
       }

       /* Blocking call. Retries are handled internally by the SDK */
       err = hubble_sat_packet_send(&packet, HUBBLE_SAT_RELIABILITY_NORMAL);
       if (err != 0) {
           LOG_ERR("Failed to send packet (err %d)", err);
           return err;
       }
   } /* end while loop, back to compute the next pass, re-enable bluetooth, and beacon */

:c:func:`hubble_sat_packet_send` is blocking. It returns only after the full
transmission sequence completes, including all retries. See
:ref:`hubble_satellite_reliability` for guidance on reliability modes and
their effect on power consumption.

.. note::

   ``BLEAppUtil_advStart`` and ``BLEAppUtil_advStop`` must be called from
   within the BLEAppUtil context. Calling them directly from another thread
   will cause a crash or silently fail.
   Use ``BLEAppUtil_invokeFunction`` to put the call into the correct context:

   .. code-block:: c

      bStatus_t ble_adv_start(void)
      {
          return BLEAppUtil_invokeFunction(
              (InvokeFromBLEAppUtilContext_t)_beacon_start, NULL);
      }

      bStatus_t ble_adv_stop(void)
      {
          return BLEAppUtil_invokeFunction(
              (InvokeFromBLEAppUtilContext_t)_beacon_stop, NULL);
      }

   where ``_beacon_start`` calls
   ``BLEAppUtil_advStart(_beacon_adv_handle, &_hubble_start_adv_set)``
   and ``_beacon_stop`` calls ``BLEAppUtil_advStop(_beacon_adv_handle)``.
   See ``samples/freertos/ti/sat-dual-stack`` for a complete reference
   implementation.


Building and Flashing
*********************

.. tabs::

   .. tab:: CCS

      #. Right-click your project in the **Project Explorer** and select
         **Build Project** (or use **Project** → **Build Project** from the
         menu bar).
      #. Once the build succeeds, right-click the project and select
         **Flash Project** to flash the firmware to the device.

   .. tab:: Makefile

      Build your application by invoking the Makefile:

      .. code-block:: bash

         make -f <your-makefile>

      To flash, use `UniFlash`_. Connect your device and
      load the built ``.hex`` or ``.out`` file.

.. note::

   **CC2755P10 (CC27xx):** If the Hardware Security Module (HSM) has not
   been provisioned on your device yet, BLE may not function correctly.
   Refer to the `TI SimpleLink CC27xx Getting Started guide`_ for
   instructions on loading the HSM before running your application.


Verifying the Application
**************************

Unlike other ports, the TI port does not include a logging backend built in
for Hubble Device SDK log. Verification is done using external tools, or
enabling logging module and calling log functions manually in your application.

Verify BLE
==========

Use the SDK's scan script to confirm BLE advertising is working. Install the
dependencies and run:

.. code-block:: bash

   pip install -r tools/requirements-scan.txt
   python tools/scan.py --key "<your-device-key>"

The script scans for Hubble BLE beacons (UUID 0xFCA6) and prints decoded
packets in real time. Running without ``--key`` prints raw packets without
decryption.

Alternatively, use the **Hubble Connect** mobile app to see nearby Hubble
devices:

* `Hubble Connect on the App Store`_
* `Hubble Connect on Google Play`_

Verify Satellite RF
===================

To verify the satellite RF output before a live pass, use an ADALM-PLUTO SDR
and the ``pyhubblenetwork`` scan tool. See the **RF Verification with an SDR**
section in **Next Steps** below for full instructions.


Troubleshooting
***************

.. include:: ../common/troubleshooting-common.rst
   :start-after: hubble-integration-troubleshooting-common

Transmission fails
==================

**Symptom:** :c:func:`hubble_sat_packet_send` returns an error or BLE
silently fails to send any advertising packets.

**Cause:**

* ``BLEAppUtil_advStop`` or ``BLEAppUtil_advStart`` was not called from
  within the ``BLEAppUtil`` context.

* Stack is too tight: the call stack needs to have enough space
  to handle both BLE and satellite stack operations.

**Fix:**

* Verify the return status of ``BLEAppUtil_invokeFunction``.

* Increase the stack size of the task that calls ``hubble_sat_packet_send``,
  pass prediction API, or BLE advertising start/stop.


.. _ti_sat_next_steps:

.. include:: ../common/next-steps.rst
   :start-after: hubble-integration-next-steps

Further Reading
===============

* :ref:`hubble_satellite_introduction`: satellite protocol details,
  reliability modes, and power trade-offs.
* :ref:`hubble_configuration`: full configuration reference for all
  ``CONFIG_HUBBLE_*`` options.
* :ref:`hubble_timing`: time management best practices for devices
  with and without a real-time clock.
* `TI SimpleLink Low Power F3 SDK`_: TI's SDK documentation, examples, and release notes.


.. _TI SimpleLink Low Power F3 SDK: https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK/
.. _SimpleLink Low Power F3 SDK v9.20.00.81 release notes: https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.20.00.81/exports/release_notes_simplelink_lowpower_f3_sdk_9_20_00_81.html
.. _TI CCS Getting Started guide: https://software-dl.ti.com/ccs/esd/documents/users_guide/ccs_getting-started.html#creating-a-new-ccstudio-ide-project
.. _UniFlash: https://www.ti.com/tool/UNIFLASH
.. _TI SimpleLink CC27xx Getting Started guide: https://dev.ti.com/tirex/content/simplelink_lowpower_f3_sdk_9_20_00_81/docs/simplelink_mcu_sdk/html/quickstart-guide/quickstart-intro-cc23xx.html#build-your-first-program-for-the-device-device
.. _Hubble Connect on the App Store: https://apps.apple.com/us/app/hubble-connect/id6751236191
.. _Hubble Connect on Google Play: https://play.google.com/store/apps/details?id=com.hubble.connect
