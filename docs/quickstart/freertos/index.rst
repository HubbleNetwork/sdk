.. _freertos_quick_start:

FreeRTOS
========

This guide provides a quick overview of how to integrate the
Hubble Device SDK with FreeRTOS. Due to differences between various
FreeRTOS vendors, the SDK provides mechanisms to integrate it into the
application build system. However, this integration requires some work
from the user to adapt it to their specific environment.


Prerequisites
*************

- FreeRTOS installed and configured for your platform.
- A working build system (e.g., Makefile-based).
- Device ``key`` (generated when you register a new device to your organization
  through the Hubble Cloud API).


Adding Hubble Network to FreeRTOS
*********************************

* Include Hubble Device SDK in your application

  .. code-block:: bash

     git submodule add https://github.com/HubbleNetwork/hubble-device-sdk


* Provide a configuration header

   * Create a configuration header in your application with the
     ``CONFIG_HUBBLE_*`` macros you want to customize. For example:

     .. code-block:: c

        #define CONFIG_HUBBLE_BLE_NETWORK  0
        #define CONFIG_HUBBLE_SAT_NETWORK  1
        #define CONFIG_HUBBLE_NETWORK_KEY_256

   * Point the build system to your config file by setting
     ``HUBBLENETWORK_SDK_CONFIG`` before including the Makefile fragment:

     .. code-block:: Makefile

        HUBBLENETWORK_SDK_CONFIG = $(MY_APP_DIR)/hubble_config.h

   * If ``HUBBLENETWORK_SDK_CONFIG`` is not set, the SDK defaults to
     ``port/freertos/config.h``, which serves as a reference listing all
     available options with their default values.

   * All symbols defined in the config file will be available at build time
     for all objects compiled using ``HUBBLENETWORK_SDK_FLAGS``.

* Since there is no standard cryptographic API, the application has to use one of
  the implementations available or implement the following APIs:

  * :c:func:`hubble_crypto_init`.
  * :c:func:`hubble_crypto_cmac`.
  * :c:func:`hubble_crypto_aes_ctr`.
  * :c:func:`hubble_crypto_zeroize`.

* Include the Makefile fragment

   * Add the provided Makefile fragment
     ``port/freertos/hubblenetwork-sdk.mk`` to your application's build
     system.

   * This fragment does not build any object files directly but provides:
     - The list of source files to be built: ``HUBBLENETWORK_SDK_SOURCES``.
     - The flags to be used during compilation: ``HUBBLENETWORK_SDK_FLAGS``.

Example of including the Makefile fragment:

.. code-block:: Makefile

   # Hubble Device SDK

   HUBBLENETWORK_SDK_CONFIG = $(MY_APP_DIR)/hubble_config.h
   include path/to/hubblenetwork-sdk/port/freertos/hubblenetwork-sdk.mk

   define HUBBLE_RULE
   $(basename $(notdir $(1))).obj: $(1)
        $(CC) $(CFLAGS) $(HUBBLENETWORK_SDK_FLAGS) -c $$< -o $$@
   endef

   $(foreach source_file,$(HUBBLENETWORK_SDK_SOURCES),$(eval $(call HUBBLE_RULE,$(source_file))))

   # Hubble Device SDK END


Build the application
---------------------


.. code-block:: bash

   make -C /path/to/your/application
