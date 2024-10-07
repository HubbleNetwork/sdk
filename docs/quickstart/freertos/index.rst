.. _freertos_quick_start:

FreeRTOS Quick Start
======================

This guide provides a quick overview of how to integrate the
Hubble Network SDK with FreeRTOS. Due to differences between various
FreeRTOS vendors, the SDK provides mechanisms to integrate it into the
application build system. However, this integration requires some work
from the user to adapt it to their specific environment.


Prerequisites
*************

- FreeRTOS installed and configured for your platform.
- A working build system (e.g., Makefile-based).


Adding Hubble Network to FreeRTOS
*********************************

* Include Hubble Network SDK in your application

     .. code-block bash

     git submodule add  https://github.com/HubbleNetwork/sdk .


* Customize the configuration header

   * The header file at ``port/freertos/config.h`` contains macros that
     can be customized for your application. For example, it can change
     the encryption key size.

   * All symbols defined in this file will be available at build time
     for all objects compiled using ``HUBBLENETWORK_SDK_BLE_FLAGS``.

* Since there is no standard cryptographic API, the application has to implement
  the API :c:func:`hubble_ble_api_get`. This method returns a :ref:`data struct <hubble_ble_port>`
  that implements cryptographic primitives needed by Hubble Network SDK.

* Include the Makefile fragment

   * Add the provided Makefile fragment
     ``port/freertos/hubblenetwork-sdk.mk`` to your application's build
     system.

   * This fragment does not build any object files directly but provides:
     - The list of source files to be built: ``HUBBLENETWORK_SDK_BLE_SOURCES``.
     - The flags to be used during compilation: ``HUBBLENETWORK_SDK_BLE_FLAGS``.

Example of including the Makefile fragment:

.. code-block:: Makefile

   # Hubble Network SDK

   include path/to/hubblenetwork-sdk/port/freertos/hubblenetwork-sdk.mk

   define HUBBLE_RULE
   $(basename $(notdir $(1))).obj: $(1)
        $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_BLE_FLAGS) -c $$< -o $$@
   endef

   $(foreach source_file,$(HUBBLENETWORK_SDK_BLE_SOURCES),$(eval $(call HUBBLE_RULE,$(source_file))))

   # Hubble Network SDK END


Build the application
---------------------


.. code-block:: bash

   make -C /path/to/your/application/Makefile
