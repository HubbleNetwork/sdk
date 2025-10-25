# Hubble Network BLE ESP-IDF Beacon Sample

This sample application demonstrates how to use the Hubble Network SDK to
create a BLE beacon that advertises its presence.

## Requirements

- Cryptographic key provided by Hubble Network
- ESP-IDF SDK (https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)

## Overview

The beacon advertises data that can be picked up by the Hubble Network. The
advertised data is generated using the `hubble_ble_advertise_get()` function
from the Hubble BLE library.

The sample requires a master key and the current UTC time to be provisioned
into the device. This is done by running the `embed_key_utc.py` script before
building the application.

## Provisioning

The provisioning process embeds a master key and the current UTC time into the
firmware. This is a necessary step before building and flashing the
application.

The `embed_key_utc.py` script takes the key file and embeds it along with the
current timestamp into the source code (`src/key.c` and `src/utc.c`).

**For a raw key file:**

```sh
<SDK_ROOT>/tools/embed_key_utc.py master.key -o main/
```

**For a base64-encoded key file:**

Use the `-b` or `--base64` flag:

```sh
<SDK_ROOT>/tools/embed_key_utc.py -b master.key -o main/
```

After running the script, the key and timestamp will be compiled into the application.

## Building and Running

### Command-line

First, setup the environment. This step assumes you've installed esp-idf
to `~/esp/esp-idf`. If you haven't, follow the initial steps in
examples/esp_idf/README.md

```sh
source ~/esp/esp-idf/export.sh
```
You may have to set target based on the ESP32 chip you are using.
For example, if you are using ESP32-C3, enter this:

```
idf.py set-target esp32c3
```

Next, `cd` to the hello example where you can build/flash/monitor:

```
idf.py build
idf.py flash
idf.py monitor
```

After flashing, the device will start advertising as a Hubble BLE beacon.

## Testing

The `scan.py` tool can be used to test the BLE
beacon. The script scans for BLE devices, and when it finds a Hubble Network
beacon, it attempts to decode the advertisement data using the provided master
key.

### Running the sample and testing it

To run the sample and test it, you need to provide the same master key that was provisioned into the device.

**For a raw key file:**

```sh
<SDK_ROOT>/tools/scan.py master.key
```

**For a base64-encoded key file:**

Use the `-b` or `--base64` flag:

```sh
<SDK_ROOT>/tools/scan.py -b master.key
```

