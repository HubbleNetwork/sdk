.. _hubble_satellite_reliability:

Reliability and Power Consumption
#################################

The reliability mode passed to :c:func:`hubble_sat_packet_send` controls how
many times the SDK asks the platform radio port to send the same packet and how
far apart those transmissions are spaced. It is the primary trade-off between
delivery probability and energy use.

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
