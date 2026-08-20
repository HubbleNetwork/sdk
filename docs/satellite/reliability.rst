.. _hubble_satellite_reliability:

Reliability and Power Consumption
#################################

The reliability mode passed to :c:func:`hubble_sat_packet_send` and
:c:func:`hubble_sat_packet_pass_send` controls how many times the SDK asks the
platform radio port to send the same packet and how far apart those
transmissions are spaced. It is the primary trade-off between delivery
probability and energy use.

Reliability Modes
*****************

.. list-table:: Satellite Reliability Modes
   :widths: 30 20 20 30
   :header-rows: 1

   * - Mode
     - Baseline transmissions
     - Interval
     - Intended use
   * - ``HUBBLE_SAT_RELIABILITY_NONE``
     - 1
     - 0 seconds
     - Testing or externally managed retries
   * - ``HUBBLE_SAT_RELIABILITY_NORMAL``
     - 8
     - 20 seconds
     - Default balance of reliability and power
   * - ``HUBBLE_SAT_RELIABILITY_HIGH``
     - 16
     - 10 seconds
     - Higher reliability with higher energy cost

For modes with retries, the SDK may add extra transmissions to compensate for
estimated clock drift since the last time synchronization. See
:ref:`hubble_satellite_clock_drift` for the drift model and how to configure it.

Choosing a Mode
***************

Satellite reliability and power consumption are directly related. More retries
increase the chance that a satellite receives the packet, but they keep the
radio active for longer and increase total energy use.

Use these guidelines when selecting a mode:

* Use ``HUBBLE_SAT_RELIABILITY_NONE`` only for testing, lab validation, or
  applications that implement their own scheduling and retry policy.
* Use ``HUBBLE_SAT_RELIABILITY_NORMAL`` as the default production setting.
* Use ``HUBBLE_SAT_RELIABILITY_HIGH`` when delivery probability is more
  important than energy consumption.
* Use pass prediction to avoid transmitting when a satellite is unlikely to be
  visible.
* Keep time synchronized to minimize drift-compensation retries.
* Avoid very short continuous-transmission intervals on battery-powered devices.

For battery-powered products, the most power-efficient design is usually a
pass-predicted workflow: sleep or use low-power BLE between passes, wake before
``pass.start``, transmit with the lowest reliability mode that meets the product
delivery requirement, then return to the low-power state.

Pass-Adaptive Transmission
***************************

:c:func:`hubble_sat_packet_pass_send` sends a packet using a
:c:struct:`hubble_sat_pass_info` window instead of the fixed baselines in the
table above. Rather than always transmitting the mode's baseline number of
times, it derives the retry count from ``pass.duration`` so the packet is
retransmitted as many times as will fit while the satellite is overhead.

.. code-block:: c

   struct hubble_sat_pass_info pass;

   err = hubble_sat_next_pass_region_get(hubble_time_get(), &region, &pass);
   if (err != 0) {
           return err;
   }

   err = hubble_sat_packet_pass_send(&packet, HUBBLE_SAT_RELIABILITY_NORMAL,
                                     &pass);
   if (err != 0) {
           return err;
   }

* **Required for a region pass.** A pass obtained from
  :c:func:`hubble_sat_next_pass_region_get` has no single point of reference
  the fixed baselines were tuned for, so :c:func:`hubble_sat_packet_send` has
  no way to fit its retries to that window. Use
  :c:func:`hubble_sat_packet_pass_send` whenever the pass came from a region
  query.
* **Also usable for a single-point pass.** A pass from
  :c:func:`hubble_sat_next_pass_get` works too — in that case
  :c:func:`hubble_sat_packet_pass_send` is a drop-in alternative to
  :c:func:`hubble_sat_packet_send` that adapts retries to that specific pass's
  duration instead of using the mode's fixed baseline.
* **``HUBBLE_SAT_RELIABILITY_NONE`` is invalid.** This function requires a
  mode that retries (``HUBBLE_SAT_RELIABILITY_NORMAL`` or
  ``HUBBLE_SAT_RELIABILITY_HIGH``); passing ``HUBBLE_SAT_RELIABILITY_NONE``, a
  NULL ``packet``, or a NULL ``pass`` returns ``-EINVAL``.

See :ref:`hubble_pass_prediction_best_practices` for how to obtain a
:c:struct:`hubble_sat_pass_info`.
