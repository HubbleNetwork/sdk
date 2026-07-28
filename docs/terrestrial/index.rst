.. _hubble_terrestrial_introduction:

Terrestrial Network Overview
############################


Introduction
************

The Hubble Terrestrial Network is the name Hubble uses for its network built on
Bluetooth® Low Energy (BLE). It is a framework designed to provide secure and
efficient communication within a Bluetooth Low Energy (BLE) environment.
Advanced encryption techniques and Bluetooth technology protect data
integrity and privacy across distributed systems. The Hubble Terrestrial API, defined
in the **hubble/ble.h** header file, provides a comprehensive set of functions
for initializing, configuring, and managing network operations.

.. note::

   While this network is called the *Hubble Terrestrial Network*, the SDK code
   and APIs use the ``ble`` namespace for it. Terrestrial functions, types, and
   configuration options carry a ``ble`` marker — for example the
   :c:func:`hubble_ble_advertise_get` function, the ``hubble/ble.h`` header, and
   the ``CONFIG_HUBBLE_BLE_NETWORK`` option — reflecting the Bluetooth Low
   Energy technology the network is built on.

Key Features
============

Secure Communication
--------------------

The Hubble Terrestrial Network uses a 128 or 256-bit encryption key to safeguard all
transmitted data, minimizing the risk of unauthorized access and data
breaches. See :ref:`hubble_ble_security`

Time Synchronization
--------------------

Functions are available to initialize the network with the current time and
update it as needed. This ensures that all nodes remain synchronized, which
facilitates coordinated operations and data consistency. See :ref:`hubble_timing`
for best practices on provisioning time and accounting for clock drift.

Advertisement Management
------------------------

The Hubble Terrestrial Network API processes input data to generate Bluetooth advertisements.
These advertisements are essential for device discovery and communication
within the BLE network. The API returns platform-specific pointers to the
generated advertisements, allowing for seamless integration with various
hardware and software platforms.

API Overview
************

Initialization
==============

Initialize the network with the current Unix time before calling other Hubble
Terrestrial API functions. The ``hubble_init`` function sets up the required
configurations and prepares the network for operation:

.. code-block:: c

    int hubble_init(uint64_t initial_time, const void *key);

Time Management
===============

The API provides functions to set and update the Unix time (milliseconds since the
Unix epoch). The `hubble_time_set` function enables precise time
synchronization throughout the network:

.. code-block:: c

   int hubble_time_set(uint64_t unix_time);

Encryption Key Management
=========================

An encryption key secures all network communication. The
`hubble_key_set` function configures this key for ongoing network
operations:

.. code-block:: c

   int hubble_key_set(const void *key);

Advertisement Retrieval
=======================

The `hubble_ble_advertise_get` function processes input data to create
Bluetooth advertisements, a critical step in enabling device discovery and
interaction within the network:

.. code-block:: c

   int hubble_ble_advertise_get(const uint8_t *input, size_t input_len, uint8_t *out, size_t *out_len);

The full API is documented in the :ref:`hubble_ble_api` reference.
