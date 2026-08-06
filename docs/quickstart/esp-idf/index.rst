.. _esp_idf_quick_start:

ESP-IDF
=======

This guide explains how to integrate the Hubble Device SDK into an
`ESP-IDF <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html>`_
project. The SDK ships as an ESP-IDF component that can be pulled into any
ESP-IDF application via ``EXTRA_COMPONENT_DIRS``.

.. seealso::

   This page covers ESP-IDF SDK setup and component integration. For a complete,
   end-to-end **satellite dual-stack** application on ESP32 hardware — pass
   prediction, BLE beaconing, and RF verification — see the
   :ref:`esp_idf_integration_guide`.

Prerequisites
*************

- `ESP-IDF <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html>`_
  installed and exported in the current shell (e.g. ``source ~/esp/esp-idf/export.sh``).
- A supported ESP32 target. The BLE Network module works on any ESP32 chip
  with a Bluetooth® Low Energy controller supported by ESP-IDF. The Satellite
  Network module currently targets the **ESP32-C6**.
- The Hubble Device SDK cloned or added to your project as a Git submodule.
- Device ``key`` (generated when you register a new device to your organization
  through the Hubble Cloud API).


Adding Hubble Network to an ESP-IDF Project
*******************************************

#. Add the Hubble Device SDK to your application, for example as a submodule:

   .. code-block:: bash

      git submodule add https://github.com/HubbleNetwork/hubble-device-sdk

#. Point your top-level ``CMakeLists.txt`` at the SDK's ESP-IDF component
   directory using ``EXTRA_COMPONENT_DIRS``, and add ``hubblenetwork-sdk`` to
   the component list:

   .. code-block:: cmake

      cmake_minimum_required(VERSION 3.13.1)

      set(EXTRA_COMPONENT_DIRS path/to/hubble-device-sdk/port/esp-idf/)

      set(COMPONENTS
              "main"
              "bt"
              "hubblenetwork-sdk"
      )

      include($ENV{IDF_PATH}/tools/cmake/project.cmake)

      project(my-app LANGUAGES C)

#. Enable the desired Hubble modules. Either run ``idf.py menuconfig`` and
   toggle them under *Component config* → *Hubble*, or set them in your
   project's ``sdkconfig.defaults``:

   .. code-block:: kconfig

      # Terrestrial (BLE) Network
      CONFIG_HUBBLE_BLE_NETWORK=y

      # Satellite Network
      CONFIG_HUBBLE_SAT_NETWORK=y

   When enabling the BLE Network module on ESP32 chips, you will also want the
   standard BLE controller bits:

   .. code-block:: kconfig

      CONFIG_BT_ENABLED=y
      CONFIG_BT_BLE_50_FEATURES_SUPPORTED=n
      CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y


Satellite Network on ESP32-C6: Required PHY Blob
************************************************

.. important::

   The Satellite Network module requires a PHY library blob from Espressif
   that is currently in Early Access (EA). The ``libphy`` shipped with ESP-IDF
   does **not** include this API yet, so the blob must be swapped in manually
   before building any Satellite Network application on the ESP32-C6.

#. Download the Espressif PHY blob:

   `libphy_C6_20260317_c83212e.zip <https://dl.espressif.com/AE/libphy_C6_20260317_c83212e%20(2).zip>`_

#. Unzip the archive and copy the extracted ``*.a`` files over the matching
   files in your ESP-IDF installation:

   .. code-block:: bash

      unzip "libphy_C6_20260317_c83212e.zip"
      cp *.a $IDF_PATH/components/esp_phy/lib/esp32c6/

This step is temporary. Once Espressif ships the API upstream, the SDK
will consume it directly and the blob swap will no longer be needed.
