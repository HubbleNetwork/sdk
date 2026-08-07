:orphan:

.. hubble-integration-pass-prediction

With the SDK initialized, the application enters the main dual-stack loop:
compute the next satellite pass, beacon over BLE while waiting, stop BLE,
transmit to the satellite, then repeat.

Compute the Next Pass
=====================

.. code-block:: c

   for (;;) {
       now_ms = hubble_time_get();

       err = hubble_sat_next_pass_get(now_ms, &device_pos, &pass_info);
       if (err != 0) {
           LOG_ERR("Failed to get next pass (err %d)", err);
           return err;
       }

       /* If the pass has already started, find the next one. */
       if (pass_info.start <= now_ms) {
           err = hubble_sat_next_pass_get(
               pass_info.start + pass_info.duration,
               &device_pos, &pass_info);
           if (err != 0) {
               LOG_ERR("Failed to get next pass (err %d)", err);
               return err;
           }
       }

:c:func:`hubble_sat_next_pass_get` returns the soonest pass visible from the
device location. If ``pass_info.start`` is already in the past, a pass is in
progress, skip it and compute the next one by searching from after its end
(``pass_info.start + pass_info.duration``).
