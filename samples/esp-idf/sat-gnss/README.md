# Hubble Network GNSS Pass Prediction Sample

Welcome to the sample project that demonstrates the integration of the
[Espressif ESP-IDF](https://github.com/espressif/esp-idf) with the
[Hubble Network Satellite Network](https://github.com/HubbleNetwork/sdk).

The project is based on and extends the
[ESP-IDF NMEA 0183 Parser example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart/nmea0183_parser),
adapting it into a reusable GNSS abstraction suitable for satellite
pass prediction.

This sample application demonstrates how to use the Hubble Network satellite
ephemeris library together with a GNSS receiver to predict upcoming satellite
passes and schedule transmissions accordingly.

The application is intended as a reference design for devices equipped with a
GNSS receiver. It shows how GNSS-provided position and time can be used at the
application layer to drive pass prediction and wake/sleep scheduling.

## Requirements

- An Espressif development board supported by ESP-IDF.
- [ESP-IDF SDK](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- An external GNSS receiver that outputs NMEA sentences over UART.
  - The sample was tested with the u-blox MAX M10S.
- GNSS antenna with clear sky view

## Overview

The application implements a simple state machine to acquire a GNSS fix and use
the location data to predict the next satellite pass using the
`hubble_next_pass_get()` function from the Hubble Satellite library.

The application then waits until the next satellite pass to perform the
satellite transmission.

At wake-up time, the application optionally refreshes the GNSS fix and
re-evaluates the pass timing if the device has moved significantly.

## GNSS Abstraction

GNSS access is implemented in a small helper module (`gnss_manager.c`), which
acts as a platform-specific GNSS abstraction layer:

- Uses the ESP-IDF NMEA parser sample as a base.
- Registers an ESP event handler for GNSS updates
- Converts GNSS UTC date/time into Unix time
- Caches the latest valid GNSS fix
- Provides a simple API call `gnss_get_fix()` for the main application

## Building and Running

### Command-line

First, connect the GNSS module to UART port. Then, setup the environment.
This step assumes you've installed esp-idf to `~/esp/esp-idf`.
If you haven't, follow the initial steps in examples/esp_idf/README.md

```sh
source ~/esp/esp-idf/export.sh
```
You may have to set target based on the ESP32 chip you are using.
For example, if you are using ESP32-C3, enter this:

```
idf.py set-target esp32c3
```

### Configure the Project

`cd` into the `sat-gnss` sample, then configure the project using the available
Kconfig options. Or open the menu using `idf.py menuconfig`.

**Note:**
- The default UART baud rate is 9600.
- The default RX pin is GPIO5.

After that, build, flash, and, monitor the sample:

```
idf.py build
idf.py flash
idf.py monitor
```
