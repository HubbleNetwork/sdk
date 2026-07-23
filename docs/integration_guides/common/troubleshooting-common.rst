:orphan:

.. hubble-integration-troubleshooting-common

``hubble_init`` returns an error
=================================

**Symptom:** ``<wrn> hubblenetwork: Failed to set Unix Epoch time``

**Cause:** ``unix_time_ms`` passed to :c:func:`hubble_init` is ``0``.
The SDK requires a valid non-zero Unix timestamp.

**Fix:** Ensure time is provisioned (over BLE, NTP, GPS, or RTC) before
calling :c:func:`hubble_init`. See the **Preparing for Satellite
Transmission** section in this guide.

---

**Symptom:** ``<wrn> hubblenetwork: Failed to set key``

**Cause:** The key buffer is NULL, zero-length, or the wrong size for the
configured key type.

**Fix:** Verify the key is correctly decoded and its length matches the
configured key size.

Pass prediction returns an error
=================================

**Symptom:** ``<wrn> hubblenetwork: Hubble Satellite next pass get: no satellites configured``

**Cause:** :c:func:`hubble_sat_satellites_set` was not called before
:c:func:`hubble_sat_next_pass_get`.

**Fix:** Call :c:func:`hubble_sat_satellites_set` with a valid orbital
parameters array immediately after :c:func:`hubble_init`.

---

**Symptom:** ``<wrn> hubblenetwork: Hubble Satellite next pass get: no pass found``

**Cause:** No satellite pass is visible from the given location within the
search window. Most commonly caused by incorrect device coordinates or a
stale Unix timestamp.

**Fix:** Verify that ``device_pos.lat`` and ``device_pos.lon`` are correct
and that ``unix_time_ms`` reflects current wall-clock time.

---

**Symptom:** Firmware appears to hang or stall inside
:c:func:`hubble_sat_next_pass_get`.

**Cause:** The pass prediction algorithm iterates forward orbit-by-orbit until
it finds a pass. If the input time is near zero (e.g. Unix time was passed in
**seconds** instead of **milliseconds**), or if ``device_pos.lat``
and ``device_pos.lon`` are invalid, the loop can spin through thousands of
orbits before returning.

**Fix:** Confirm ``unix_time_ms`` is in **milliseconds** and that
``device_pos.lat`` and ``device_pos.lon`` hold the actual device coordinates.
