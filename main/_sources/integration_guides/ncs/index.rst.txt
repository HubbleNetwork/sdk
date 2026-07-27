.. _ncs_integration_guide:

Nordic nRF Connect SDK Integration Guide
########################################

This guide walks through integrating the Hubble Network dual-stack Satellite
and BLE application with the `nRF Connect SDK`_ (NCS).
NCS is Nordic Semiconductor's production-ready SDK built on top of the Zephyr
RTOS, and is the recommended platform for Nordic nRF52, nRF53, and nRF54 Series
devices.

By the end of this guide you will know how to:

* Integrate the Hubble Satellite and Terrestrial (BLE) network stacks into an
  NCS application.
* Obtain ephemeris data and use pass prediction to schedule satellite
  transmissions.
* Transmit data to the Hubble satellite network from a Nordic device.

Supported NCS Version and Boards
*********************************

The Hubble Device SDK currently supports **NCS v3.4.0**. If you require
support for a different version, `contact us <mailto:support@hubble.com>`_.

Nordic nRF52, nRF53, and nRF54 Series SoCs are supported. The SDK includes
board API implementations for the following development kits out of the box:

.. list-table::
   :widths: 50 25 25
   :header-rows: 1

   * - Board
     - Target
     - SoC Family
   * - nRF21540 DK
     - ``nrf21540dk/nrf52840``
     - nRF52
   * - Nordic Thingy:53
     - ``thingy53/nrf5340/cpunet``
     - nRF53
   * - nRF54L15 DK
     - ``nrf54l15dk/nrf54l15/cpuapp``
     - nRF54

For custom boards, refer to the :ref:`board bring-up <ncs_sat_board_bringup>`
section.

.. include:: ../common/prerequisites.rst
   :start-after: hubble-integration-prerequisites

.. include:: ../common/account-setup.rst
   :start-after: hubble-integration-account-setup

SDK Setup
*********

Install the NCS Toolchain
=========================

Follow the `NCS installation guide`_ to install the NCS toolchain and
all required dependencies before proceeding.

Add Hubble Network to an Existing NCS Project
=============================================

If you already have an NCS workspace, add the Hubble Device SDK as a West
module by including the following in your project's sub-manifest or
``west.yml``:

.. TODO: Update ``revision`` from ``main`` to the satellite release tag once
   the first tagged satellite release is available.

.. code-block:: yaml

   manifest:
     remotes:
       - name: hubble
         url-base: https://github.com/HubbleNetwork
     projects:
       - name: hubblenetwork-sdk
         repo-path: hubble-device-sdk
         revision: main
         path: modules/lib/hubblenetwork-sdk
         remote: hubble

Then run:

.. code-block:: bash

   west update hubblenetwork-sdk

Initialize a New Workspace
==========================

To start a fresh workspace with Hubble Network as the manifest repository,
use the ``west-ncs.yml`` to import NCS:

.. code-block:: bash

   west init -m git@github.com:HubbleNetwork/hubble-device-sdk.git \
       --mf west-ncs.yml ~/hubblenetwork-workspace
   cd ~/hubblenetwork-workspace
   west update

Fetch the Satellite Radio Blobs
================================

The satellite radio port for Nordic SoCs relies on prebuilt libraries
distributed as West blobs. These are required for the satellite stack to
build and must be fetched explicitly:

.. code-block:: bash

   west blobs fetch hubblenetwork-sdk

The blobs are placed under ``modules/lib/hubblenetwork-sdk/zephyr/blobs/``
and linked automatically when ``CONFIG_HUBBLE_SAT_NETWORK=y`` is set for a
supported Nordic board.

.. note::

   If you skip this step, the build will fail with a missing library error
   when ``CONFIG_HUBBLE_SAT_NETWORK=y`` is enabled.


.. _ncs_sat_board_bringup:

Board Bring-Up
**************

If you are using one of the supported development kits from the
`Supported NCS Version and Boards`_ section, skip ahead to
:ref:`Project Configuration <ncs_sat_project_config>`.

For custom boards, this section walks through implementing the satellite
board API using the nRF21540 DK as a concrete example. If your board does
not yet have a Zephyr board definition, follow the
`Zephyr board porting guide`_ first before proceeding.

The full board API contract and integration checklist are documented in
:ref:`board_support`. This section focuses on the NCS/Zephyr path.

.. note::

   **nRF53 (dual-core) boards:** The nRF5340 has a dedicated network core
   (Cortex-M33) for radio activity and an application core for application
   logic. The satellite and BLE radio stacks run on the network core, while
   your application runs on the application core. Communication between the
   two cores requires IPC.

   When building for an nRF53 target, use ``sysbuild`` to compile both cores'
   firmware in a single build invocation. For a complete dual-core project
   structure with IPC wiring and application/network core split, see the
   `Hubble Thingy:53 reference application`_.

Implementing the Board API
==========================

Create a ``.c`` file for your board and implement the four functions declared
in ``port/zephyr/sat_board.h``. The nRF21540 DK implementation at
``port/zephyr/boards/nordic/nrf52/nrf21540dk.c`` is used as the reference
throughout.

.. note::

   A common FEM implementation uses generic two-pin PA/LNA control, such as
   a Skyworks SKY66112-11 or similar. See
   ``port/zephyr/boards/nordic/nrf52/fem.c`` for a reference on how these
   routines are implemented.

:c:func:`hubble_sat_board_init`
--------------------------------

Called once during SDK initialization. Initialize your board hardware here:
GPIOs, PA/FEM configuration, and any static power defaults:

.. code-block:: c

   int hubble_sat_board_init(void)
   {
       /* Initialize your board hardware here, e.g. PA/FEM GPIOs. */
       hubble_board_fem_setup();
       return 0;
   }

:c:func:`hubble_sat_board_enable`
----------------------------------

Called before each transmission window. Enable your board's RF components,
setting power to the right level (+20 dBm) before handing off to the SoC layer:

.. code-block:: c

   int hubble_sat_board_enable(void)
   {
       /* Enable your board hardware here */
       return hubble_sat_soc_enable();
   }

:c:func:`hubble_sat_board_disable`
-----------------------------------

Called after each transmission window. Disable your RF components and return
the board to its idle state:

.. code-block:: c

   int hubble_sat_board_disable(void)
   {
       /* Disable your board hardware here */
       return hubble_sat_soc_disable();
   }

:c:func:`hubble_sat_board_packet_send`
---------------------------------------

Called by the SDK for each transmission, including retries. Enable the PA/FEM
path before transmission and put it back to sleep/bypass afterwards:

.. code-block:: c

   int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
   {
       int ret;

       /* Enable your PA/FEM before transmitting. */
       board_fem_enable();

       /* 
        * Optional: default power for each SoC is 0 dBm,
        * however, the board may and can set the SoC radio power
        * to a higher level supported by the SoC based on the gain
        * of the PA/FEM to reach +20 dBm output.
        * e.g. nrf_radio_txpower_set(NRF_RADIO, desired_power_level);
        */
       board_fem_set_power_level(desired_power_level);

       /* Call the SoC layer to handle packet transmission */
       ret = hubble_sat_soc_packet_send(packet);
       if (ret != 0) {
           /* Handle error */
       }

       /* Optional: if power level was set, restore previous power level */
       board_fem_set_power_level(previous_power_level);

       /* Return PA/FEM to sleep/bypass after transmission. */
       board_fem_sleep();

       return ret;
   }

Register the Board in the Build System
=======================================

Your board source file, which includes the PA/FEM setup routines and the
board API implementation above, must be registered with the Zephyr build
system. This can be done from within the SDK's ``port/zephyr/boards/``
directory or from within your application's own source tree as an out-of-tree
module.

Conditioned on your board's Kconfig symbol, add the following to the relevant
``CMakeLists.txt``. Using the nRF21540 DK as an example:

.. code-block:: cmake

   if(CONFIG_BOARD_NRF21540DK_NRF52840)
       zephyr_include_directories(
           ${ZEPHYR_BASE}/subsys/bluetooth/controller/ll_sw/nordic/hal/nrf5/radio/
       )
       zephyr_library_sources(fem.c)
       zephyr_library_sources(nrf21540dk.c)
   endif()

Replace ``CONFIG_BOARD_NRF21540DK_NRF52840`` with the Kconfig symbol for your
board (``CONFIG_BOARD_<YOUR_BOARD>``), and list your FEM and board source
files accordingly. The prebuilt satellite radio blob for the SoC family is
linked automatically by the existing SDK SoC ``CMakeLists.txt``.


.. _ncs_sat_project_config:

Project Configuration
*********************

Enable the Hubble dual-stack by adding the following to your ``prj.conf``:

.. code-block:: kconfig

   CONFIG_HUBBLE_BLE_NETWORK=y
   CONFIG_HUBBLE_SAT_NETWORK=y

   # Set to your oscillator's PPM rating (check your crystal datasheet)
   CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR=10

``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` sets the clock drift rate of your
oscillator in parts per million (PPM). Configuring this correctly ensures the
SDK adds the right number of retransmissions to compensate for accumulated
clock drift since the last time sync. An incorrect value reduces satellite
delivery probability. See :ref:`hubble_satellite_clock_drift` for a detailed
explanation.

For the full set of available options, see :ref:`hubble_configuration`.

Other common options for a dual-stack application:

.. code-block:: kconfig

   # Bluetooth
   CONFIG_BT=y
   CONFIG_BT_PERIPHERAL=y

   # Satellite pass windows can be 12+ hours away,
   # so we need a 64-bit timeout to avoid overflow.
   CONFIG_TIMEOUT_64BIT=y

   # Optional: enable FPU if supported
   CONFIG_FPU=y


.. include:: ../common/data-requirements.rst
   :start-after: hubble-integration-data-requirements


.. include:: ../common/sdk-init.rst
   :start-after: hubble-integration-sdk-init


The Pass Prediction Loop
************************

.. include:: ../common/pass-prediction.rst
   :start-after: hubble-integration-pass-prediction

Beacon over BLE While Waiting
==============================

Start a timer for the pass window and begin BLE advertising while the device
waits:

.. code-block:: c

       k_timer_start(&sat_timer, K_MSEC(pass_info.start - now_ms),
                     K_NO_WAIT);

       /*
        * Enable Bluetooth by calling bt_enable(), get the Hubble beacon
        * payload via hubble_ble_advertise_get() and start BLE advertising
        * with bt_le_adv_start().
        */
       start_ble_advertising();

       /*
        * Block and wait until the pass timer fires. Common approaches are
        * a semaphore, task notification, or event flag.
        */
       block_until_timer_expires();

The timer callback signals the waiting thread when the pass window is overhead.

Transmit to the Satellite
==========================

Before transmitting, the BLE stack must be fully stopped to release the radio.
Call ``bt_le_adv_stop()`` to stop advertising, then ``bt_disable()`` to
disable the Bluetooth stack entirely. The satellite radio requires exclusive
access to the radio hardware:

.. code-block:: c

       /* Stop advertising and disable BLE to release the radio. */
       bt_le_adv_stop();
       bt_disable();

       err = hubble_sat_packet_get(&packet, NULL, 0);
       if (err != 0) {
           LOG_ERR("Failed to build packet (err %d)", err);
           return err;
       }

       /* Blocking call, retries are handled internally by the SDK */
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


Building and Flashing
**********************

Build your application with the ``ncs-dual-stack`` snippet. This snippet is
required for all NCS applications using the Hubble dual-stack. It enables
the Multiprotocol Service Layer (MPSL) for BLE and satellite radio coexistence:

.. code-block:: bash

   west build -p -b <your_board> <your_app> -S ncs-dual-stack
   west flash


Verifying the Application
**************************

Enable the Hubble Device SDK logging by adding the following to ``prj.conf``:

.. code-block:: kconfig

   CONFIG_LOG=y
   CONFIG_HUBBLE_LOG_LEVEL_DBG=y

Open a serial terminal to the device at 115200 baud (e.g. ``nrfutil``,
``minicom`` or any serial terminal application).

Expected SDK Log Output
=======================

After a successful :c:func:`hubble_init` call, the SDK logs:

.. code-block:: none

   <inf> hubblenetwork: Hubble Device SDK initialized

At debug level, once pass prediction runs and a transmission is scheduled, you
will see the retry count computed from your TDR setting and the time elapsed
since the last sync:

.. code-block:: none

   <dbg> hubblenetwork: Time drift since last sync: 20000 ms
   <dbg> hubblenetwork: Number of additional retries due TDR: 1
   <dbg> hubblenetwork: Number of retries: 9 - interval: 20 seconds

After :c:func:`hubble_sat_packet_send` completes the full transmission
sequence:

.. code-block:: none

   <inf> hubblenetwork: Hubble Satellite packet sent

If this line appears without any preceding ``<err>`` or ``<wrn>`` from the
``hubblenetwork`` module, the device has successfully transmitted to the
satellite network. For RF-level verification using an SDR, see
:ref:`Next Steps <ncs_sat_next_steps>`.


Troubleshooting
***************

Build fails with missing library
=================================

**Symptom:** Linker error referencing a missing ``hubblenetwork`` or satellite
radio library.

**Cause:** West blobs were not fetched.

**Fix:**

.. code-block:: bash

   west blobs fetch hubblenetwork-sdk

.. include:: ../common/troubleshooting-common.rst
   :start-after: hubble-integration-troubleshooting-common

Transmission fails
==================

**Symptom:** ``<wrn> hubblenetwork: Hubble Satellite packet transmission failed``

**Cause:** The board API returned an error from
:c:func:`hubble_sat_board_packet_send`, or the BLE stack was still active
when :c:func:`hubble_sat_packet_send` was called.

**Fix:** Ensure ``bt_le_adv_stop()`` and ``bt_disable()`` are called before
:c:func:`hubble_sat_packet_send`. If the error comes from the board API,
check your PA/FEM GPIO sequencing in
:c:func:`hubble_sat_board_packet_send`.

---

**Symptom:** ``<err> hubblenetwork: Hubble Satellite Network initialization failed``

**Cause:** :c:func:`hubble_sat_board_init` returned an error during SDK
initialization, common cause is the board init failed.

**Fix:** Check the return value of :c:func:`hubble_sat_board_init` in your
board implementation.


.. _ncs_sat_next_steps:

.. include:: ../common/next-steps.rst
   :start-after: hubble-integration-next-steps

Further Reading
===============

* :ref:`hubble_satellite_introduction`: satellite protocol details,
  reliability modes, and power trade-offs.
* :ref:`hubble_pass_prediction_best_practices`: choosing pass options,
  interpreting pass results, and computing multiple upcoming passes.
* :ref:`hubble_configuration`: full Kconfig reference for all
  ``CONFIG_HUBBLE_*`` options.
* :ref:`hubble_timing`: time management best practices for devices
  with and without a real-time clock.
* :ref:`board_support`: full board API contract and integration checklist.


.. External links

.. _NCS installation guide: https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/installation/install_ncs.html
.. _nRF Connect SDK: https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html
.. _Zephyr board porting guide: https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html
.. _Hubble Thingy\:53 reference application: https://github.com/HubbleNetwork/hubble-reference-thingy53
