:orphan:

.. hubble-integration-sdk-init

Initializing the Hubble Device SDK
***********************************

Once time, location, and orbital parameters are available, initialize the SDK
before calling any other Hubble API:

.. code-block:: c

   /*
    * At this point unix_time_ms, device_pos, and orb_params are assumed to be
    * valid. Either baked into firmware or received via BLE provisioning.
    */
   err = hubble_init(unix_time_ms, master_key);
   if (err != 0) {
       LOG_ERR("Failed to initialize Hubble Device SDK (err %d)", err);
       return err;
   }

   err = hubble_sat_satellites_set(orb_params, orb_params_count);
   if (err != 0) {
       LOG_ERR("Failed to set orbital parameters (err %d)", err);
       return err;
   }

:c:func:`hubble_init` takes the current Unix time in milliseconds and a
pointer to the master key. :c:func:`hubble_sat_satellites_set` registers the
orbital parameters array with the SDK.

.. warning::

   * **The key buffer MUST remain valid for the lifetime of SDK usage.** The
     SDK stores the pointer directly and does not copy the key. Do not use a
     stack or temporary buffer.
   * **Unix time must be non-zero.** Passing ``0`` in
     ``CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME`` mode returns an error.
   * **The orbital parameters array MUST remain valid** for as long as pass
     prediction is used. The SDK stores a pointer and does not copy the array.
