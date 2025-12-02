.. _hubble_timing:

Best Practices for Time Management
##################################

Proper time management is critical for ensuring accurate encryption
and secure operations in the SDK. Below are the best practices for
provisioning UTC time and accounting for drifts.

Provisioning UTC Time
*********************

To ensure the SDK operates with accurate time, follow these steps to provision UTC time:

1. **Obtain Accurate Time Source**

   + Use a reliable time source such as an NTP (Network Time Protocol) server or GPS.
   + Ensure the time source is synchronized and trusted to avoid tampering.

2. **Set System Time**

   + Use the Hubble APIs (:c:func:`hubble_init` and
     :c:func:`hubble_utc_set`) to set the obtained UTC time.

3. **Validate Time Synchronization**

   + Periodically validate the system time against the trusted time source.
   + Log any discrepancies for debugging and auditing purposes.

Accounting for Time Drifts
**************************

Time drifts can occur due to hardware clock inaccuracies. To mitigate this, follow these practices:

1. **Periodic Time Synchronization**

   + Regularly synchronize the system clock with the trusted time source.
   + Use a configurable interval based on the expected drift rate of the hardware clock.

2. **Monitor Drift**

   + Measure the drift rate of the hardware clock during testing.
   + Use this information to adjust the synchronization interval.

3. **Apply Drift Compensation**

   + Implement drift compensation logic to adjust the system clock between synchronizations.


Tips for Handling Time Persistence Across Power Cycles
******************************************************

To accommodate a certain amount of drift and ensure time persistence across power cycles, you can use alternative methods such as storing the time in flash memory or other persistent storage. Below are recommendations, risks, and a workflow for managing time in such scenarios.

Alternatives for Time Persistence Across Reboots
================================================

1. **Store Time in Flash Memory**

   + Write the current UTC time to flash memory before a power cycle or periodically during operation.
   + Risks:

     + *Flash Wear*: Flash memory has a limited number of write cycles. Frequent writes can lead to premature wear.
     + *Tearing*: If a power failure occurs during a write operation, the data may become corrupted.
     + *No Time Gap Information*: Flash storage does not provide information about how long the device was powered off.

2. **Store Time in Non-Volatile RAM (NVRAM)**

   + Use NVRAM or battery-backed RAM to store the current UTC time.
   + This method avoids flash wear and allows faster read/write operations.
   + Risks:

     + *Hardware Dependency*: Requires specific hardware support for NVRAM.
     + *Battery Depletion*: If the battery backing the RAM depletes, the stored time will be lost.

3. **Use an RTC Device**

   + If the hardware includes an RTC, initialize it with the current UTC time and use it as a persistent time source.
   + This is the most reliable method for time persistence across reboots.
   + Risks:

     + *RTC Drift*: The RTC may drift over time and require periodic synchronization.

4. **Hybrid Approach**

   + Combine flash storage and RTC. Use the RTC for short-term persistence and flash storage as a backup in case of RTC failure or battery depletion.
   + Risks:

     + *Complexity*: Increases system complexity and requires careful coordination between the two methods.

Recommended Workflow for Time Management
========================================

Below is a recommended workflow for managing time in the SDK:

1. **Device Boots**

   + Retrieve the last known time from persistent storage (e.g., flash, NVRAM, or RTC).
   + If no valid time is available, initialize the system to a default value (e.g., epoch time).

2. **Wait to Sync Time**

   + Attempt to synchronize the system clock with a trusted time source (e.g., NTP server or GPS).
   + Block critical operations that depend on accurate time until synchronization is complete.

3. **Start Tracking Time**

   + Once synchronized, start tracking time using the system clock.
   + Periodically log the current time for debugging and auditing purposes.

4. **Monitor Drift**

   + Continuously monitor the drift of the system clock.
   + If the drift exceeds a predefined threshold, trigger a resynchronization with the trusted time source.

5. **Handle Resynchronization**

   + During resynchronization, adjust the system clock to the correct UTC time.
   + Log the adjustment to maintain an audit trail.


Encryption and Time Dependency
******************************

Encryption often relies on accurate time for operations such as
generating time-based keys or validating timestamps. Follow these
guidelines:

+ Ensure the system clock is synchronized before performing encryption-related tasks.
+ Log the UTC time used for encryption to facilitate debugging and auditing.

Additional Notes
================

+ Use the log functionality to record time synchronization events and drift adjustments.
+ Test the SDK on hardware with varying clock drift rates to ensure robustness.
+ Consider implementing fallback mechanisms if the primary time source becomes unavailable.

By following these best practices, you can ensure accurate time
management in the SDK, which is essential for secure and reliable
encryption.
