# Hubble Network Sat Continuous Sample

This sample application demonstrates how to use the Hubble Device SDK to
continuously transmit packets to satellite.

## Requirements

- Cryptographic key provided by Hubble Network
- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- ESP32-C6 hardware

## Overview

This project is designed to:

- Demonstrate the use of the ESP-IDF together with the HubbleNetwork-SDK for
  satellite-enabled applications.
- Provide a practical starting point for developers integrating the Hubble
  Satellite Network on ESP32-C6 hardware.

Once running, the device initializes the Hubble Device SDK and then enters a loop that
builds and transmits a satellite packet with normal reliability.

> [!NOTE]
> The sample uses the **device uptime** counter source
> (`CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`), so it does not need UTC time
> provisioned: the EID counter used for encryption starts at 0 and advances with
> device uptime. Only the master key has to be embedded before building.

> [!WARNING]
> Make sure that your generated secret key from [Hubble API](https://hubble.com/docs/api-specification/register-new-devices)
> is using the same key size (AES-128 or AES-256) and counter source (DEVICE_UPTIME) as the sample.

## Configuration

The sample exposes the following options (via `idf.py menuconfig`, under
"Sat Continuous Sample Configuration"):

| Option                     | Default | Description                                              |
| -------------------------- | ------- | -------------------------------------------------------- |
| `CONFIG_HUBBLE_DEVICE_KEY` | `""`    | Hubble device cryptographic key, base64-encoded.         |

## Building and Running

First, set up the environment. This step assumes you've installed esp-idf
to `~/esp/esp-idf`. If you haven't, follow the initial steps in the [Installation
guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html#installation) for your OS.

```sh
source ~/esp/esp-idf/export.sh
```

Set the target chip to ESP32-C6:

```sh
idf.py set-target esp32c6
```

Configure the device key:

```sh
idf.py menuconfig
```

or add the config `CONFIG_HUBBLE_DEVICE_KEY="your_b64_key"` to
`sdkconfig.defaults`.

Next, `cd` to the sat-continuous example where you can build/flash/monitor:

```sh
idf.py build flash monitor
```

After flashing, the device continuously transmits a packet to satellite with
normal reliability.
