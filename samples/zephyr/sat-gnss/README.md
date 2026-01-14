# Hubble Network GNSS Pass Prediction Sample

This sample application demonstrates how to use the Hubble Network satellite
ephemeris library together with a GNSS receiver to predict upcoming satellite
passes and schedule transmissions accordingly.

The application is intended as a reference design for devices equipped with a
GNSS receiver. It shows how GNSS-provided position and time can be used at the
application layer to drive pass prediction and wake/sleep scheduling.

## Requirements

- A development board supported by Zephyr
- An external GNSS receiver supported by Zephyr’s GNSS subsystem (NMEA over UART)
  - e.g. u-blox M8/M9/M10 based modules
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

GNSS access is implemented in a small helper module to manage GNSS
(`gnss_manager.c`):

- Uses Zephyr’s GNSS subsystem
- Registers a GNSS data callback
- Converts GNSS UTC date/time into Unix time
- Caches the latest valid fix
- Provides a simple API call `gnss_get_fix()` for the main application

## Device Tree Configuration

The GNSS device must be defined in the board’s devicetree overlay:

Example overlay snippet:

```dts

&uart1 {
    status = "okay";
    current-speed = <115200>;

    gnss: gnss-generic {
        compatible = "gnss-nmea-generic";
        status = "okay";
    };
};
```

## Building and Running

Connect the GNSS module to UART port, and flash the application to the
target board.

```sh
west build -b nrf52840dk/nrf52840 .
west flash
```
