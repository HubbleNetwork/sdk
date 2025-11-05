.. _hubble_ble_introduction:

BLE Network Overview
####################


Introduction
************

The Hubble Bluetooth® Low Energy (BLE) Network is a framework designed to provide secure
and efficient communication within a Bluetooth Low Energy (BLE) environment.
Advanced encryption techniques and Bluetooth technology protect data
integrity and privacy across distributed systems. The Hubble BLE API, defined
in the **hubble/ble.h** header file, provides a comprehensive set of functions
for initializing, configuring, and managing network operations.

Key Features
============

Secure Communication
--------------------

The Hubble BLE Network uses a 256-bit encryption key to safeguard all
transmitted data, minimizing the risk of unauthorized access and data
breaches. See :ref:`hubble_ble_security`

Time Synchronization
--------------------

Functions are available to initialize the network with the current time and
update it as needed. This ensures that all nodes remain synchronized, which
facilitates coordinated operations and data consistency.

Advertisement Management
------------------------

The Hubble BLE API processes input data to generate Bluetooth advertisements.
These advertisements are essential for device discovery and communication
within the BLE network. The API returns platform-specific pointers to the
generated advertisements, allowing for seamless integration with various
hardware and software platforms.

API Overview
************

Initialization
==============

Initialize the network with the current UTC time before calling other Hubble
BLE API functions. The ``hubble_ble_init`` function sets up the required
configurations and prepares the network for operation:

.. code-block:: c

    int hubble_ble_init(uint64_t utc_time);

Time Management
===============

The API provides functions to set and update the UTC time (seconds since the
Unix epoch). The `hubble_ble_utc_set` function enables precise time
synchronization throughout the network:

.. code-block:: c

   int hubble_ble_utc_set(uint64_t utc_time);

Encryption Key Management
=========================

A 256-bit encryption key secures all network communication. The
`hubble_ble_key_set` function configures this key for ongoing network
operations:

.. code-block:: c

   int hubble_ble_key_set(const void *key);

Advertisement Retrieval
=======================

The `hubble_ble_advertise_get` function processes input data to create
Bluetooth advertisements, a critical step in enabling device discovery and
interaction within the network:

.. code-block:: c

   int hubble_ble_advertise_get(const uint8_t *input, size_t len, uint8_t *output, size_t* out_len);

Security Details
****************

.. toctree::
   :maxdepth: 1
   :glob:

   security.rst

Timing management
*****************

.. toctree::
   :maxdepth: 1
   :glob:

   timing.rst

API Reference
*************

.. toctree::
   :maxdepth: 1
   :glob:

   api.rst
