.. _hubble_satellite_clock_drift:

Clock Drift Compensation
########################

For reliability modes with retries, the SDK may add extra transmissions to
account for estimated clock drift since the last time synchronization. The drift
estimate is based on ``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` in parts per
million (PPM).

The SDK computes the elapsed time since the last time sync, estimates drift, and
adds one retry for each full retransmission interval of drift. This widens the
effective transmission coverage when the device clock may be early or late.

Pass prediction also accounts for drift by starting the calculated transmission
window earlier. Keep Unix time synchronized when possible to reduce unnecessary
extra retries.

Understanding PPM
*****************

PPM (parts per million) is the maximum frequency deviation of an oscillator from
its nominal frequency. A clock rated at 50 PPM can drift at most 50 microseconds
per second, which accumulates to:

* 50 ms per 1,000 seconds (~17 minutes)
* 180 ms per hour
* 4.3 seconds per day

The SDK uses ``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` to estimate the total
accumulated drift since the last time synchronization:

.. code-block:: none

   drift_ms = (elapsed_since_last_sync_ms × TDR_ppm) / 1,000,000

One additional retry is then added for each full retransmission interval covered
by that drift. For example, with ``HUBBLE_SAT_RELIABILITY_NORMAL`` (20-second
interval) and a 100 PPM clock that has not synced for 3 hours:

.. code-block:: none

   drift_ms           = 10,800,000 ms × 100 / 1,000,000 = 1,080 ms
   additional_retries = 1,080 ms / 20,000 ms            = 0

At 30 hours without a sync:

.. code-block:: none

   drift_ms           = 108,000,000 ms × 100 / 1,000,000 = 10,800 ms
   additional_retries = 10,800 ms / 20,000 ms            = 0  (rounds down)

At 56 hours without a sync:

.. code-block:: none

   drift_ms           = 201,600,000 ms × 100 / 1,000,000 = 20,160 ms
   additional_retries = 20,160 ms / 20,000 ms            = 1

For most devices syncing time at least once per day, drift compensation adds
zero or one extra retries. Higher-PPM oscillators or longer sync gaps produce
more extra retries.

Maximum Additional Retries
==========================

Two caps bound the total number of retries:

* The drift estimate is capped at **90 minutes** regardless of how long the
  device has gone without a time sync.
* The total retry count (baseline + additional) is capped to 90 min / retry interval.

In practice these ceilings are only reached by high-PPM oscillators that have
not synchronized time for many days. At 100 PPM, reaching 90 minutes of
accumulated drift requires roughly 625 days without a sync.

Finding the TDR for Your Hardware
*********************************

The Time Drift Rate (TDR) value to configure comes directly from the oscillator
used as the device clock source. The typical lookup path is:

1. **Identify the clock source.** Check the board schematic or SoC reference
   manual to find which oscillator drives the timekeeping peripheral (RTC or
   low-frequency oscillator). It is usually a 32.768 kHz crystal or, for
   lower-cost designs, an internal RC oscillator.

2. **Read the datasheet.** Open the crystal or oscillator datasheet and look
   for the **frequency accuracy** or **frequency tolerance** specification,
   typically in a table labelled *Electrical Characteristics* or *AC
   Characteristics*. The value is expressed as ±X ppm over a stated temperature
   and supply-voltage range.

3. **Use the worst-case figure.** Datasheets often list both an initial
   tolerance at room temperature and a wider tolerance over the full operating
   temperature range. Use the larger value.

Typical ranges by oscillator type:

.. list-table::
   :widths: 40 20 40
   :header-rows: 1

   * - Oscillator type
     - Typical PPM
     - Notes
   * - TCXO (temperature-compensated crystal)
     - 0.5 – 5
     - Best accuracy; used in communication-grade designs
   * - External crystal (XTAL)
     - 10 – 100
     - Most common; check datasheet for your specific part
   * - Internal RC oscillator
     - 200 – 5,000
     - Factory calibration at room temperature only; degrades with temperature

If you cannot obtain the exact figure, use a conservative (higher) estimate.
Underestimating PPM causes the device to transmit fewer extra retries than
needed, which reduces delivery probability when the clock has drifted
significantly.
