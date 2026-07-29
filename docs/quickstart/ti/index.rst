.. _ti_quick_start:

TI SDK (SysConfig)
==================

This guide explains how to integrate the Hubble Device SDK into a
`SimpleLink Low Power F3 SDK <https://www.ti.com/tool/SIMPLELINK-LOWPOWER-F3-SDK>`_
project using `SysConfig <https://www.ti.com/tool/SYSCONFIG>`_. SysConfig
generates driver and peripheral initialization code from a ``.syscfg`` script,
and the Hubble Device SDK provides its own SysConfig product so its module
can be configured alongside the TI SDK drivers in the same script.

.. note::

   SysConfig is the recommended integration path for TI SDK projects. If you
   prefer to manage Hubble Device SDK build manually or are working in an
   environment without SysConfig, follow the generic
   :ref:`freertos_quick_start` instead — it describes how to include the SDK
   sources and flags directly via the provided Makefile fragment.

.. seealso::

   This page covers TI SDK setup and SysConfig integration. For a complete,
   end-to-end **satellite dual-stack** application on TI hardware — pass
   prediction, BLE beaconing, and RF verification — see the
   :ref:`ti_integration_guide`.

Prerequisites
*************

- `SimpleLink Low Power F3 SDK <https://www.ti.com/tool/SIMPLELINK-LOWPOWER-F3-SDK>`_ installed.
- `TI ARM LLVM Compiler <https://www.ti.com/tool/CCSTUDIO>`_ installed.
- SysConfig CLI tool (``sysconfig_cli.sh`` / ``sysconfig_cli.bat``) available.
- The Hubble Device SDK cloned or added as a submodule.

Set the following environment variables before building:

.. code-block:: bash

   export TICLANG_ARMCOMPILER=/path/to/ti-cgt-armllvm
   export SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/path/to/simplelink_lowpower_f3_sdk
   export SYSCONFIG_TOOL=/path/to/sysconfig_cli.sh
   export HUBBLE_NETWORK_SDK=/path/to/hubble-device-sdk


Adding the Hubble Device SDK SysConfig Product
***********************************************

The Hubble Device SDK ships a SysConfig product descriptor at
``.metadata/product.json``. Pass it to the SysConfig CLI alongside the TI SDK
product so both products are available in the same ``.syscfg`` script:

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


Configuring the Hubble Module in a ``.syscfg`` Script
******************************************************

Load the Hubble module in your ``.syscfg`` script with:

.. code-block:: javascript

   var hubble = scripting.addModule("/Hubble");

This is all that is required. The module registers the Hubble Device SDK
sources and include paths with the SysConfig framework so the generated
``.opt`` files carry the right compiler flags.

A minimal ``.syscfg`` script for a BLE application on CC23xx looks like:

.. code-block:: javascript

   // @cliArgs --board /ti/boards/LP_EM_CC2340R5 --rtos freertos

   /* TI drivers */
   var AESCCM = scripting.addModule("/ti/drivers/AESCCM");
   AESCCM.addInstance().$name = "CONFIG_AESCCM0";

   var RCL   = scripting.addModule("/ti/drivers/RCL");
   var Power = scripting.addModule("/ti/drivers/Power");

   /* Hubble Device SDK */
   var hubble = scripting.addModule("/Hubble");

   /* BLE stack and FreeRTOS */
   var ble      = scripting.addModule("/ti/ble/ble");
   ble.basicBLE = true;

   const FreeRTOS = scripting.addModule("/freertos/FreeRTOS");
   FreeRTOS.heapSize = 0x00004D50;

The ``/Hubble`` module exposes the network it should build for through two
options:

- ``useTerrestrial`` — enable the Hubble Terrestrial (BLE) Network.
- ``useSatellite`` — enable the Hubble Satellite Network.

Based on these options and the selected device (CC23xx or CC27xx), the module
automatically registers the correct radio sources and build flags, so the same
script works for terrestrial-only, satellite-only, and dual-stack
applications.


Configuring the Radio for Satellite
***********************************

Satellite transmission uses a custom RF configuration. The SDK provides a
``hubble_radio.js`` helper that sets up the ``radioconfig`` (and, for
dual-stack, the DMM) modules and excludes the stock ``ti_radio_config.c`` in
favor of the SDK-provided one. Load it with ``system.getScript`` and call the
function that matches your use case.

For a **satellite-only** application, enable satellite, disable terrestrial,
and call ``config()``:

.. code-block:: javascript

   var Hubble = scripting.addModule("/Hubble");
   Hubble.useSatellite   = true;
   Hubble.useTerrestrial = false;

   var hubble_radio = system.getScript("/hubble_radio.js");
   hubble_radio.config();

For a **dual-stack** application (satellite + terrestrial running
concurrently), enable both networks and call ``config_dual_stack(role)``,
passing the BLE stack role (for example ``"ble"``). This sets up the Dynamic
Multi-protocol Manager (DMM) so the BLE and satellite stacks can share the
radio:

.. code-block:: javascript

   var Hubble = scripting.addModule("/Hubble");
   Hubble.useSatellite   = true;
   Hubble.useTerrestrial = true;

   var hubble_radio = system.getScript("/hubble_radio.js");
   hubble_radio.config_dual_stack("ble");

When both networks are enabled, the module additionally pulls in the DMM
sources and include paths and defines ``-DCC23X0`` (plus ``-DCC27`` on CC27xx)
and the ``-DUSE_DMM*`` flags.

.. important::

   In a dual-stack application, ``hubble_init`` sets up both the BLE (BT) and
   the custom RF stacks for satellite use. It must be called **before** the BLE
   stack is started **only when** ``CONFIG_HUBBLE_FREERTOS_DAEMON_HOOK``
   (``configUSE_DAEMON_TASK_STARTUP_HOOK`` when using SysConfig) is disabled.
   When the daemon task startup hook is enabled, :c:func:`hubble_init` has to
   be invoked only before using Hubble SDK APIs.

.. note::

   Make sure the RCL command symbol in the custom RF settings is generated
   automatically (set **Symbol Name Generation Method** to **Automatic**) so
   the SDK can reference it. See the ``samples/freertos/ti/sat-continuous``
   sample for complete satellite-only and dual-stack ``.syscfg`` scripts.


Using Code Composer Studio (CCS)
*********************************

If you are using `Code Composer Studio <https://www.ti.com/tool/CCSTUDIO>`_
and prefer not to write a Makefile manually, you can add the Hubble Device SDK
SysConfig product directly from the IDE:

1. Right-click the project folder in the **Project Explorer** and select
   **Properties**.
2. Navigate to **General** → **SysConfig**.
3. In the **SysConfig Flags** field, append the following flag:

   .. code-block:: none

      -s "/path/to/hubble-device-sdk/.metadata/product.json"

   Replace ``/path/to/hubble-device-sdk`` with the actual path to your Hubble
   Device SDK checkout.

4. Click **Apply and Close**. CCS will pass this flag to the SysConfig CLI
   during the build, making the ``/Hubble`` module available in your
   ``.syscfg`` script just as described in the section above. If Satellite Network
   is enabled the additional step must be added to your ``.syscfg`` script:

   .. code-block:: none

      var hubble_radio = system.getScript("/hubble_radio.js");
      hubble_radio.config_dual_stack("ble"); // or hubble_radio.config()

Building the Application
************************

Run SysConfig to generate the configuration files and then build:

.. code-block:: bash

   make -f Makefile

The ``Makefile`` must pass the generated ``.opt`` files to the
compiler using the ``@`` prefix so that SysConfig-generated flags (include
paths, preprocessor definitions) are applied:

.. code-block:: Makefile

   CFLAGS += $(addprefix @,$(SYSCFG_OPT_FILES))


See the ``samples/freertos/ti/ble_beacon`` sample for a complete working example.
