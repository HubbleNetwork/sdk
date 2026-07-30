.. _hubble_satellite_introduction:

Satellite Network Overview
###########################

.. warning::

   The satellite functionality is currently in **pre-production** and is not yet
   ready for production deployments. APIs and behavior may change in future
   releases.

Introduction
************

The Hubble Satellite Network lets devices send messages through Hubble's
satellite network without adding a cellular modem or gateway. It is designed
for devices that need connectivity beyond the terrestrial coverage.

The application is responsible for deciding *when* to transmit. Two common
patterns are supported:

* **Continuous transmission** for devices that are constantly powered or are in a
  test environment.
* **Pass-predicted transmission** for devices that should transmit only when a
  satellite pass is expected.

Satellite packets can carry payloads of ``0``, ``4``, ``9``, or ``13`` bytes.
The SDK encrypts the payload using the configured Hubble device key and derives
the packet identifiers from the SDK time counter.

How It Works
************

At a high level, a satellite transmission follows this sequence:

1. Initialize the SDK with :c:func:`hubble_init`.
2. Build a satellite packet with :c:func:`hubble_sat_packet_get`.
3. Send the packet with :c:func:`hubble_sat_packet_send`.
4. The platform port enables the radio, converts the packet into frames, sends
   each frame, and disables the radio when transmission is complete.

The SDK uses the configured counter source for key derivation and packet
identity:

* ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME`` uses Unix time in milliseconds.
* ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`` uses a device-uptime-derived
  counter and does not require UTC time.

Use Unix time when the device can provision or synchronize time and needs pass
prediction. Use device uptime for simple continuous transmission workflows that
do not need wall-clock scheduling.

Packet Generation
=================

:c:func:`hubble_sat_packet_get` creates the encrypted satellite packet. The
input payload length must be one of the supported sizes: ``0``, ``4``, ``9``, or
``13`` bytes.

.. code-block:: c

   struct hubble_sat_packet packet;
   uint8_t payload[4] = { 0x01, 0x02, 0x03, 0x04 };

   err = hubble_sat_packet_get(&packet, payload, sizeof(payload));
   if (err != 0) {
           /* Handle error. */
   }

An empty payload is valid and is useful when the device only needs to report its
presence:

.. code-block:: c

   err = hubble_sat_packet_get(&packet, NULL, 0);

Transmission
============

:c:func:`hubble_sat_packet_send` sends a prepared packet with the selected
reliability mode:

.. code-block:: c

   err = hubble_sat_packet_send(&packet, HUBBLE_SAT_RELIABILITY_NORMAL);

The function is blocking. It returns after the full transmission sequence has
completed or after an error is reported by the platform radio port.

The reliability mode controls the trade-off between delivery probability and
energy use. See :ref:`hubble_satellite_reliability` for guidance on selecting a
mode.

Pass Prediction
***************

Pass prediction lets the application transmit only when a satellite is expected
to be visible from the device location. This reduces radio-on time and is the
recommended workflow for battery-powered devices.

.. seealso::

   :ref:`hubble_pass_prediction_best_practices` is the complete guide to pass
   prediction: choosing the minimum elevation angle, reading the pass result,
   and computing multiple upcoming passes. This section covers only the inputs
   the SDK needs and how to obtain them.

.. warning::

   Pass prediction relies on synchronized Unix time. When the device uses
   ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME``, this time is provisioned as part of
   normal initialization. When the device uses
   ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME``, the SDK counter is derived from
   device uptime rather than wall-clock time, so the application must provision
   Unix time with :c:func:`hubble_time_set` before calling any pass prediction
   API. Both pass prediction and the drift-based retry compensation depend on this
   synchronized time.

The pass prediction API needs three inputs:

* Current Unix time in milliseconds when asking for a pass.
* Device location as latitude and longitude in degrees.
* One or more satellite orbital parameter records.

.. _hubble_satellite_orbital_params:

Orbital Parameters (Satellites information)
===========================================

The SDK stores a pointer to the orbital parameter array passed to
:c:func:`hubble_sat_satellites_set`; it does not copy the array. Keep the array
valid for as long as pass prediction is used.

.. code-block:: c

   #define MAX_SATELLITES 6

   static struct hubble_sat_orbital_params satellites[MAX_SATELLITES];
   static size_t satellite_count;

   err = hubble_sat_satellites_set(satellites, satellite_count);
   if (err != 0) {
           /* Handle invalid provisioning data. */
   }

Each ``struct hubble_sat_orbital_params`` describes one satellite well enough
for the SDK to predict when it can be reached from the device location.
Provision these values from a trusted source such as a companion application,
BLE provisioning flow, manufacturing data, or another backend channel.

The dual-stack samples provision orbital parameters over BLE. The provisioning
payload maps directly to ``struct hubble_sat_orbital_params`` fields:

.. code-block:: c

   memcpy(&sat->t0,           p + 0,  sizeof(uint64_t));
   memcpy(&sat->n0,           p + 8,  sizeof(double));
   memcpy(&sat->ndot,         p + 16, sizeof(double));
   memcpy(&sat->raan0,        p + 24, sizeof(double));
   memcpy(&sat->raandot,      p + 32, sizeof(double));
   memcpy(&sat->aop0,         p + 40, sizeof(double));
   memcpy(&sat->aopdot,       p + 48, sizeof(double));
   memcpy(&sat->inclination,  p + 56, sizeof(double));
   memcpy(&sat->eccentricity, p + 64, sizeof(double));
   memcpy(&sat->satellite_id, p + 72, sizeof(uint32_t));

When receiving binary provisioning data, copy each field separately as shown
above. This avoids unaligned accesses on MCUs that do not support them.

For devices that do not receive orbital parameters at runtime, you can generate
a C source file with hard-coded satellite parameters and build it into the
application. The ``tools/orbital_params_fetch.py`` helper fetches current
orbital parameters from the Hubble API and writes ``sat_params.c``.

Set ``HUBBLE_API_TOKEN`` to a Hubble API bearer token, then run the tool from the
SDK repository:

.. code-block:: console

   export HUBBLE_API_TOKEN=<token>
   python tools/orbital_params_fetch.py path/to/output

The generated file contains an array named ``sats``:

.. code-block:: c

   #include <hubble/sat/pass_prediction.h>

   struct hubble_sat_orbital_params sats[] = {
           /* Generated satellite orbital parameters. */
   };

Compile the generated ``sat_params.c`` into your application and register the
array before computing passes:

.. code-block:: c

   extern struct hubble_sat_orbital_params sats[];

   err = hubble_sat_satellites_set(sats, ARRAY_SIZE(sats));
   if (err != 0) {
           /* Handle error. */
   }

Hard-coded parameters are simple and work well for fixed firmware images or
devices without a provisioning channel. Refresh and rebuild the generated file
when the orbital data used by your product needs to be updated.

Computing a Pass
================

Once time, location, and orbital data are available, use
:c:func:`hubble_sat_next_pass_get` to find the next pass and schedule the
transmission against ``pass.start``. Reading the result, selecting a minimum
elevation angle, and looking several passes into the future are all covered in
:ref:`hubble_pass_prediction_best_practices`.

Common API Workflow
*******************

Continuous Transmission
=======================

Use continuous transmission when the device is constantly powered, when power is
not the primary constraint, or when validating radio integration. The sample
applications under ``samples/*/sat-continuous`` follow this pattern.

The example below initializes the SDK with ``hubble_init(0, master_key)``. This
requires ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`` so the time counter is
derived from device uptime.

Typical workflow:

1. Provision the Hubble key.
2. Initialize the SDK.
3. Build a packet.
4. Send the packet.
5. Sleep for an application-defined interval.
6. Repeat.

.. code-block:: c

   err = hubble_init(0, master_key);
   if (err != 0) {
           return err;
   }

   for (;;) {
           err = hubble_sat_packet_get(&packet, NULL, 0);
           if (err != 0) {
                   return err;
           }

           err = hubble_sat_packet_send(&packet,
                                        HUBBLE_SAT_RELIABILITY_NORMAL);
           if (err != 0) {
                   return err;
           }

           sleep_until_next_application_interval();
   }

This mode is simple, but it can spend energy transmitting when no satellite is
in view. For production battery-powered devices, prefer pass prediction when
time and orbital data are available.

Dual Stack with Pass Prediction
===============================

The dual-stack samples combine BLE provisioning/beaconing with satellite
transmission. The device uses BLE while waiting for the next satellite pass,
then stops BLE and transmits over the satellite radio during the pass window.

Typical workflow:

1. Start BLE provisioning.
2. Receive Unix time, device location, and satellite orbital parameters.
3. Initialize the Hubble Device SDK with the provisioned Unix time and key.
4. Register orbital parameters with :c:func:`hubble_sat_satellites_set`.
5. Compute the next pass with :c:func:`hubble_sat_next_pass_get`.
6. Start BLE beaconing while waiting for ``pass.start``.
7. Stop BLE when the pass timer expires.
8. Build and send a satellite packet.
9. Repeat from pass calculation (step 5).

.. code-block:: c

   err = hubble_init(unix_time_ms, master_key);
   if (err != 0) {
           return err;
   }

   err = hubble_sat_satellites_set(orb_params, orb_params_count);
   if (err != 0) {
           return err;
   }

   for (;;) {
           uint64_t now_ms = hubble_time_get();

           err = hubble_sat_next_pass_get(now_ms, &device_pos, &pass);
           if (err != 0) {
                   return err;
           }

           schedule_timer_for(pass.start - now_ms);
           ble_adv_start();
           wait_for_pass_timer();
           ble_adv_stop();

           err = hubble_sat_packet_get(&packet, NULL, 0);
           if (err != 0) {
                   return err;
           }

           err = hubble_sat_packet_send(&packet,
                                        HUBBLE_SAT_RELIABILITY_NORMAL);
           if (err != 0) {
                   return err;
           }
   }

This workflow is appropriate for devices that already use BLE for local
connectivity, provisioning, or nearby network operation and only need satellite
transmission during predicted windows. For a complete, platform-specific
walkthrough of this pattern, see the :ref:`hubble_integration_guides`.

Radio Timing
************

:c:func:`hubble_sat_packet_send` is synchronous. During the call, the platform
port typically:

1. Acquires the satellite transmission lock or semaphore.
2. Enables the satellite radio front-end.
3. Builds radio frames from the packet for each retry.
4. Sends the frames through the board radio implementation.
5. Sleeps until the next retry interval when more retries remain.
6. Disables the satellite radio front-end.
7. Releases the transmission lock or semaphore.

Applications should treat the call as a blocking radio operation and avoid
starting conflicting radio activity until it returns. Dual-stack applications
should stop BLE before satellite transmission unless the platform explicitly
supports concurrent operation.

Reliability and Clock Drift
***************************

The reliability mode selects the number of retries and their spacing, trading
delivery probability against energy use. For modes with retries, the SDK also
adds transmissions to compensate for accumulated clock drift.

* :ref:`hubble_satellite_reliability` — the reliability modes and how to choose
  one for your power budget.
* :ref:`hubble_satellite_clock_drift` — the drift model, the PPM math, and how
  to find the ``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` value for your hardware.

Supporting New Boards
*********************

See :ref:`board_support` for integration paths, required APIs, SoC-specific
guidance, and the board support checklist.

Configuration Notes
*******************

Enable the satellite network with ``CONFIG_HUBBLE_SAT_NETWORK``.

Important related options include:

* ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME`` for Unix-time-based operation.
* ``CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`` for uptime-counter operation.
* ``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` for clock-drift retry compensation.
* ``CONFIG_HUBBLE_NETWORK_KEY_128`` or ``CONFIG_HUBBLE_NETWORK_KEY_256`` for key
  size selection.
* ``CONFIG_HUBBLE_NETWORK_CRYPTO_*`` for crypto backend selection.

See :ref:`hubble_configuration` for the complete configuration reference. The
full satellite API is documented in the :ref:`hubble_sat_api` reference.

.. toctree::
   :maxdepth: 1
   :hidden:

   reliability
   clock-drift
   board-support
