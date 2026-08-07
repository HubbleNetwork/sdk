.. _hubble_pass_prediction_best_practices:

Pass Prediction
###############

Pass prediction lets a device compute *when* a Hubble satellite will be in view
of a location so the application only powers the radio during the expected
reception window. For battery-powered products this is the single most effective
way to reduce energy use, since the radio stays off between passes.

This guide focuses on **choosing the right pass option** for your product: which
query to use, how to read the result, how to select a minimum elevation angle,
and how to look several passes into the future. For the end-to-end satellite
workflow and configuration reference, see
:ref:`hubble_satellite_introduction`.

.. contents:: On this page
   :local:
   :depth: 2

Prerequisites
*************

Pass prediction depends on synchronized wall-clock time and orbital data.

Before requesting a pass, the application must provide three things:

* **Time** — the current Unix time in milliseconds (see :ref:`hubble_timing`).
* **Location** — the device position in degrees.
* **Orbital parameters** — one or more satellite records registered with
  :c:func:`hubble_sat_satellites_set`. See
  :ref:`hubble_satellite_orbital_params` for how to obtain and provision these.

The API at a Glance
*******************

Pass prediction is exposed through three functions in
``<hubble/sat/pass_prediction.h>``:

.. list-table::
   :widths: 45 55
   :header-rows: 1

   * - Function
     - Purpose
   * - :c:func:`hubble_sat_satellites_set`
     - Register the orbital parameters used by every subsequent query.
   * - :c:func:`hubble_sat_next_pass_get`
     - Find the next pass over a **single point** (latitude/longitude).
   * - :c:func:`hubble_sat_min_elevation_angle_set`
     - Set the minimum elevation angle a pass must reach to be reported.

All queries search across *every* registered satellite and return the earliest
qualifying pass. There is no per-call satellite selection: register the full set
once, and the SDK picks the soonest reachable satellite for you.

Registering Satellites
=======================

.. note::

   :c:func:`hubble_sat_satellites_set` stores a **pointer** to the array — it does
   not copy the data. The array must remain valid for as long as pass prediction is
   used.

.. code-block:: c

   static struct hubble_sat_orbital_params satellites[MAX_SATELLITES];
   size_t count = provision_satellites(satellites);

   err = hubble_sat_satellites_set(satellites, count);
   if (err != 0) {
           /* -EINVAL: NULL array with a non-zero count. */
   }

To clear the list, call with ``NULL`` and a count of ``0``. Registering more
satellites widens coverage (more chances for an early pass) at the cost of more
computation per query, since each call evaluates all of them.


Getting a satellite pass
************************

When the device knows its own location with reasonable
accuracy (for example, a fixed asset, or a mobile device with GNSS), it answers:
*"When is the next pass directly usable from this exact spot?"*

.. code-block:: c

   struct hubble_sat_device_pos pos = { .lat = 47.0, .lon = -122.0 };
   struct hubble_sat_pass_info pass;

   err = hubble_sat_next_pass_get(hubble_time_get(), &pos, &pass);
   if (err == 0) {
           /* Transmit around pass.start. */
   }

Reading the Result
*******************

The pass prediction function fills a ``struct
hubble_sat_pass_info``. All times are Unix time in **milliseconds**.

.. list-table::
   :widths: 30 70
   :header-rows: 1

   * - Field
     - Meaning
   * - ``start``
     - Recommended time to **begin** the transmission sequence. This is the value
       to schedule against.
   * - ``culmination``
     - Time of maximum pass alignment (the satellite's highest point for this
       pass).
   * - ``duration``
     - Length of the transmission window, in milliseconds.
   * - ``max_elevation_angle``
     - Peak elevation above the horizon, in degrees. Higher generally means a
       stronger link.
   * - ``lon``
     - Longitude of the pass, in degrees (East positive).

Schedule the radio against ``start``, not ``culmination``. The SDK already
centers the window on the expected reception time and shifts ``start`` earlier to
absorb clock drift (see `Time, Drift, and the Search Start`_).

Return values:

* ``0`` — a pass was found and ``pass`` is populated.
* ``-EINVAL`` — a required pointer argument was ``NULL``.
* ``-ENOENT`` — no satellites are registered, or no qualifying pass was found.

Selecting the Minimum Elevation Angle
*************************************

:c:func:`hubble_sat_min_elevation_angle_set` sets the elevation threshold a pass
must reach at culmination to be reported. Passes whose peak elevation falls below
it are skipped entirely. The default is **45 degrees**; the accepted range is
**[30, 90]**.

.. code-block:: c

   /* Report only high, close passes. */
   hubble_sat_min_elevation_angle_set(60);

This is the main quality-vs-frequency lever:

* **Higher angle** (closer to 90) → the satellite passes nearly overhead: shorter
  slant range, typically better link margin, but **fewer** qualifying passes and
  longer waits between them.
* **Lower angle** (toward 30) → more passes qualify and windows come sooner, but
  low passes skim the horizon with a longer, weaker link path.

Guidelines:

* Start with the default (45) unless you have a measured reason to change it.
* Raise it for products that prioritize link reliability per transmission and can
  tolerate longer gaps between opportunities.
* Lower it for latency-sensitive products that need the *soonest possible*
  window and can compensate for weaker passes with a higher reliability mode (see
  :ref:`hubble_satellite_reliability`).

The setting is global and applies to every subsequent queries. Set it
once during initialization; changing it between related queries makes
multi-pass results harder to reason about.

Getting Multiple Passes in the Future
*************************************

Each query returns only the **single next** pass. To build a schedule of several
upcoming passes, call the query in a loop, advancing the search start time past
the end of the pass you just found.

The key is to start the next search **after the current pass window ends**
(``start + duration``); starting at ``culmination`` or ``start`` risks getting the
same pass back.

.. code-block:: c

   #define MAX_PASSES 5

   struct hubble_sat_pass_info passes[MAX_PASSES];
   uint64_t t = hubble_time_get();
   size_t found = 0;

   for (size_t i = 0; i < MAX_PASSES; i++) {
           int err = hubble_sat_next_pass_get(t, &pos, &passes[i]);
           if (err != 0) {
                   break; /* -ENOENT: no further pass in range. */
           }

           /* Advance past this pass so the next search skips it. */
           t = passes[i].start + passes[i].duration;
           found++;
   }

This is also how a running application chains passes over time: after handling a
pass, search for the next one starting just past the window that was serviced.
The satellite sample uses this idiom to recover when a computed pass has already
begun:

.. code-block:: c

   if (pass.start <= now_ms) {
           /* The window already opened; look past it for the next one. */
           err = hubble_sat_next_pass_get(pass.start + pass.duration,
                                          &pos, &pass);
   }

.. note::

   Because each successive pass is computed from orbital parameters propagated
   further into the future, accuracy degrades the further out you predict.
   Prefer computing a short horizon (the next pass, or the next few) and
   recomputing as the device approaches each window, rather than building a long
   schedule from a single point in time. Re-synchronize time before long-horizon
   predictions.

Time, Drift, and the Search Start
*********************************

The ``t`` argument is both the current time and the point the search starts from.
Passing ``hubble_time_get()`` returns the next pass from *now*; passing a future
time returns the next pass from *then*.

The SDK automatically compensates for clock drift: it shifts the reported
``start`` earlier by half of the estimated drift window so the transmission stays
centered on the true reception window even if the device clock is slightly fast
or slow. The drift estimate is derived from
``CONFIG_HUBBLE_SAT_NETWORK_DEVICE_TDR`` (in PPM) and the time since the last
synchronization. See :ref:`hubble_satellite_clock_drift` for the full model.

The practical consequences for pass selection:

* Keep Unix time synchronized. A stale clock widens the drift window, pulling
  ``start`` earlier and lengthening radio-on time.
* Do not add your own margin on top of ``start`` for drift — the SDK already
  applied it. Schedule directly against ``start``.

Decision Summary
****************

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - If your device...
     - Use
   * - Prioritizes link quality over frequency
     - A higher minimum elevation angle (e.g. 60)
   * - Needs the soonest possible window
     - The default or a lower minimum elevation angle
   * - Needs a schedule of upcoming passes
     - Loop the query, advancing ``t`` by ``start + duration``

For the complete satellite workflow, reliability modes, and configuration
options, see :ref:`hubble_satellite_introduction`. The full API reference is in
:ref:`hubble_sat_api`.
