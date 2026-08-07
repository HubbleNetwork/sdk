.. _esp_idf_integration_guide:

ESP-IDF Integration Guide
#########################

This guide walks through integrating the Hubble Network dual-stack Satellite
and BLE application with `Espressif's IoT Development Framework`_ (ESP-IDF).

By the end of this guide you will know how to:

* Integrate the Hubble Satellite and Terrestrial (BLE) network stacks into an
  ESP-IDF application.
* Obtain ephemeris data and use pass prediction to schedule satellite
  transmissions.
* Transmit data to the Hubble satellite network from an ESP32 device.

Supported Devices and SDK Version
**********************************

The Hubble Device SDK currently supports **ESP-IDF v6.0**. If you require
support for a different version, `contact us <mailto:support@hubble.com>`_.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - SoC
     - Notes
   * - ESP32-C6
     - RISC-V, 20 dBm integrated PA


.. include:: ../common/prerequisites.rst
   :start-after: hubble-integration-prerequisites

.. include:: ../common/account-setup.rst
   :start-after: hubble-integration-account-setup


SDK Setup
*********

Install ESP-IDF
===============

Follow the `ESP-IDF Getting Started guide`_ to install ESP-IDF v6.0 and all
required dependencies. Once installed, source the export script to set up the
environment:

.. code-block:: bash

   . $IDF_PATH/export.sh

Clone the Hubble Device SDK
===========================

Clone the SDK alongside your application:

.. code-block:: bash

   git clone https://github.com/HubbleNetwork/hubble-device-sdk.git

Add the Hubble Port as an ESP-IDF Component
============================================

Register the ESP-IDF port as a component by setting ``EXTRA_COMPONENT_DIRS``
in your application's ``CMakeLists.txt``:

.. code-block:: cmake

   set(EXTRA_COMPONENT_DIRS /path/to/hubble-device-sdk/port/esp-idf/)

   include($ENV{IDF_PATH}/tools/cmake/project.cmake)
   project(your-app LANGUAGES C)

See ``samples/esp-idf/sat-dual-stack/CMakeLists.txt`` for a complete reference.

.. _esp_idf_sat_phy_blob:

Fetch the Satellite PHY Blob (ESP32-C6)
========================================

.. important::

   The Satellite Network module requires a PHY library blob from Espressif
   that is currently in Early Access (EA). The ``libphy`` shipped with
   ESP-IDF does **not** include this API yet and must be swapped in manually
   before building.

#. Download the Espressif PHY blob:

   `libphy_C6_20260317_c83212e.zip`_

#. Unzip and copy the extracted ``*.a`` files into your ESP-IDF installation:

   .. code-block:: bash

      unzip "libphy_C6_20260317_c83212e.zip"
      cp *.a $IDF_PATH/components/esp_phy/lib/esp32c6/

This step is temporary. Once Espressif ships the API upstream, the blob swap
will no longer be needed.


.. _esp_idf_sat_project_config:

Project Configuration
*********************

sdkconfig.defaults
==================

Enable the Hubble dual-stack by adding the following to your
``sdkconfig.defaults``:

.. code-block:: kconfig

   # Hubble Network
   CONFIG_HUBBLE_BLE_NETWORK=y
   CONFIG_HUBBLE_SAT_NETWORK=y

   # Set to your oscillator's PPM rating (check your crystal datasheet)
   CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR=10

``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` sets the clock drift rate in parts
per million (PPM). See :ref:`hubble_satellite_clock_drift` for details.

For the full set of available options, see :ref:`hubble_configuration`.

Other common options for a dual-stack application:

.. code-block:: kconfig

   # Bluetooth
   CONFIG_BT_ENABLED=y
   CONFIG_BT_NIMBLE_ENABLED=y

   # Hubble uses legacy advertising, not extended advertising
   CONFIG_BT_NIMBLE_50_FEATURE_SUPPORT=n

   # Optional: enable logging
   CONFIG_LOG_VERSION_2=y

Register Application Components
================================

In your ``main/CMakeLists.txt``, declare the required component dependencies:

.. code-block:: cmake

   idf_component_register(SRCS ${YOUR_APP_SOURCES}
                          PRIV_REQUIRES bt nvs_flash esp_timer hubblenetwork-sdk
                          INCLUDE_DIRS ".")

``bt`` and ``hubblenetwork-sdk`` are required for the dual-stack. ``nvs_flash``
is required by NimBLE. ``esp_timer`` is used in the reference sample for pass
scheduling, however, any timer peripheral works.


.. include:: ../common/data-requirements.rst
   :start-after: hubble-integration-data-requirements


.. include:: ../common/sdk-init.rst
   :start-after: hubble-integration-sdk-init

On ESP-IDF, ``nvs_flash_init()`` must be called before initializing the NimBLE
stack and registering GATT characteristics and services:

.. code-block:: c

   /* NVS flash init, dependency of NimBLE stack */
   ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
       ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       ESP_ERROR_CHECK(nvs_flash_erase());
       ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

   ret = nimble_port_init();
   if (ret != ESP_OK) {
       ESP_LOGE(BLE_TAG, "Failed to init NimBLE (rc=%d)", ret);
       return ret;
   }

   /*
    * Call your BLE setup function here. For example:
    * Register GATT services, configs, callbacks, NimBLE host task, etc.
    */


The Pass Prediction Loop
************************

.. include:: ../common/pass-prediction.rst
   :start-after: hubble-integration-pass-prediction

Beacon over BLE While Waiting
==============================

Schedule a one-shot timer for the pass window and start BLE advertising while
the device waits. The example below uses ``esp_timer``; any timer peripheral
that can schedule a future callback works equally well.

.. code-block:: c

       sat_wait_us = (pass_info.start - now_ms) * US_PER_MS;
       esp_timer_start_once(_sat_timer, sat_wait_us);

       /*
        * Get the Hubble beacon payload using hubble_ble_advertise_get()
        * and start BLE advertising with ble_gap_adv_start().
        */
       ble_adv_start();

       /*
        * Block and wait until the pass timer fires. Common approaches are
        * a semaphore, task notification, or event flag.
        */
       block_until_timer_expires();

The timer callback signals the waiting task when the pass window is overhead.

Transmit to the Satellite
==========================

Stop advertising, then build and send the packet:

.. code-block:: c

       /* Stop advertising to release the radio. */
       ble_gap_adv_stop();

       err = hubble_sat_packet_get(&packet, NULL, 0);
       if (err != 0) {
           ESP_LOGE(APP_TAG, "Failed to build packet (err %d)", err);
           return;
       }

       /* Blocking call. Retries are handled internally by the SDK */
       err = hubble_sat_packet_send(&packet, HUBBLE_SAT_RELIABILITY_NORMAL);
       if (err != 0) {
           ESP_LOGE(APP_TAG, "Failed to send packet (err %d)", err);
           return;
       }
   } /* end while loop, back to compute the next pass, re-enable bluetooth, and beacon */

:c:func:`hubble_sat_packet_send` is blocking. It returns only after the full
transmission sequence completes, including all retries. See
:ref:`hubble_satellite_reliability` for guidance on reliability modes and
their effect on power consumption.


Building and Flashing
*********************

Set the target, build, flash, and open the serial monitor:

.. code-block:: bash

   idf.py set-target esp32c6
   idf.py build flash monitor


Verifying the Application
**************************

Expected Log Output
===================

Enable logging by adding the following to your ``sdkconfig.defaults``:

.. code-block:: kconfig

   CONFIG_LOG_VERSION_2=y

After a successful :c:func:`hubble_init` call, the SDK logs:

.. code-block:: none

   I (xxx) hubblenetwork: Hubble Device SDK initialized (HDCV:1.0/E:256/CS:UT/RP:S86400/N:TS/TV:0/SV:0)

.. note::

   If using ESP-IDF Log v1 (``CONFIG_LOG_VERSION_1=y``), the log level prefix
   and timestamp are omitted and only the plain message appears, e.g.:

   .. code-block:: none

      Hubble Device SDK initialized (HDCV:1.0/E:256/CS:UT/RP:S86400/N:TS/TV:0/SV:0)

At debug level, once pass prediction runs and a transmission is scheduled:

.. code-block:: none

   D (xxx) hubblenetwork: Time drift since last sync: 20000 ms
   D (xxx) hubblenetwork: Number of additional retries due TDR: 1
   D (xxx) hubblenetwork: Number of retries: 9 - interval: 20 seconds

After :c:func:`hubble_sat_packet_send` completes:

.. code-block:: none

   I (xxx) hubblenetwork: Hubble Satellite packet sent

If this line appears without any preceding error from the ``hubblenetwork``
tag, the device has successfully transmitted to the satellite network.

Verify BLE
==========

Use the SDK's scan script to confirm BLE advertising is working:

.. code-block:: bash

   pip install -r tools/requirements-scan.txt
   python tools/scan.py --key "<your-device-key>"

Verify Satellite RF
===================

To verify the satellite RF output before a live pass, use an ADALM-PLUTO SDR
and the ``pyhubblenetwork`` scan tool. See the **RF Verification with an SDR**
section in **Next Steps** below for full instructions.


Troubleshooting
***************

.. include:: ../common/troubleshooting-common.rst
   :start-after: hubble-integration-troubleshooting-common

Build fails with missing PHY symbols
=====================================

**Symptom:** Linker error referencing undefined symbols.

**Cause:** The EA PHY blob was not swapped into the ESP-IDF installation.

**Fix:** Follow the :ref:`esp_idf_sat_phy_blob` steps in the SDK Setup section.

NimBLE fails to initialize
===========================

**Symptom:** ``nimble_port_init()`` returns an error.

**Cause:** ``nvs_flash_init()`` was not called before initializing the NimBLE
stack.

**Fix:** Ensure ``nvs_flash_init()`` is called before ``nimble_port_init()``.


.. _esp_idf_sat_next_steps:

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
* `Espressif's IoT Development Framework`_: ESP-IDF documentation,
  examples, and API reference.


.. _Espressif's IoT Development Framework: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/index.html
.. _ESP-IDF Getting Started guide: https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/index.html
.. _libphy_C6_20260317_c83212e.zip: https://dl.espressif.com/AE/libphy_C6_20260317_c83212e%20(2).zip
