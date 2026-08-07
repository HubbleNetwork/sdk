:orphan:

.. hubble-integration-data-requirements

Preparing for Satellite Transmission
*************************************

The satellite stack requires three inputs before it can schedule and transmit:

* **Unix time**: used for pass prediction and key derivation
* **Device location**: latitude and longitude, used to compute satellite passes
* **Orbital parameters**: satellite ephemeris data describing each satellite's
  orbit

How these are provisioned is up to the application.

Time
====

Many approaches work: BLE provisioning from a phone, NTP over Wi-Fi, GPS, an
RTC, or reading from persistent storage across reboots. See
:ref:`hubble_timing` for best practices and trade-offs.

Location
========

If the device is deployed at a fixed location, latitude and longitude can be
hard-coded directly in firmware:

.. code-block:: c

   struct hubble_sat_device_pos device_pos = {
       .lat = 47.6,
       .lon = -122.3,
   };

For mobile devices, location can be obtained from an onboard GPS module if
present, or provisioned at runtime from a companion app. For example,
delivered over BLE from a phone/gateway that has GPS access.

Orbital Parameters
==================

Orbital parameters describe each satellite's orbit and are used by
:c:func:`hubble_sat_next_pass_get` to predict when a satellite will be
visible from the device location.

Since Hubble satellites are station-keeping, orbital parameters are stable
enough to be baked into firmware at build time. Use the
``tools/orbital_params_fetch.py`` helper to fetch current parameters from the
Hubble API and generate a ``sat_params.c`` file ready to compile into your
application:

.. code-block:: bash

   export HUBBLE_API_TOKEN=<your-api-token>
   python tools/orbital_params_fetch.py path/to/output

.. TODO: Add recommended maximum age for baked-in orbital parameters before
   a firmware update is needed to refresh them.

See :ref:`hubble_satellite_orbital_params` for details on the generated format
and how to register the array with the SDK.
