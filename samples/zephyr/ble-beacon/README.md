# Hubble Network BLE Beacon Sample

This sample application demonstrates how to use the Hubble Network SDK to
create a BLE beacon that advertises its presence.

## Requirements

- Cryptographic key provided by Hubble Network

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

> [!TIP]
> The application can be configured to sync time at runtime using *Current time Service (CTS)*,
> this option is recommended for long running tests because it is possible sync time after
> reboots without changing the firmware. To enable this option add the following option
> in `prj.conf`
>
> ```
> CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS=y
> ```

### 2. Run the Provisioning Script

The `embed_key_utc.py` script takes the key file and embeds it along with the
current timestamp into the source code (`src/key.c` and `src/utc.c`).

**For a raw key file:**

```sh
# Script is located in SDK_BASE/tools

python ../../../tools/embed_key_utc.py master.key -o ./src
```

**For a base64-encoded key file:**

Use the `-b` or `--base64` flag:

```sh
# Script is located in SDK_BASE/tools

python ../../../tools/embed_key_utc.py -b master.key -o ./src
```

After running the script, the key and timestamp will be compiled into the application.

## Building and Running

Once the key and time are provisioned, you can build and flash the application to your target board.

```sh
west build -b nrf52840dk/nrf52840 .
west flash
```

After flashing, the device will start advertising as a Hubble BLE beacon.

> [!WARNING]
> If the application was built with `CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS` enabled, it is necessary
> to sync utc time. This can be done using the `sync_time.py` script:
>
> ```sh
> ./sync_time.py
> ```

## Configuration

The sample provides a few Kconfig options to customize its behavior:

- `CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV`: When enabled, the beacon includes additional custom data in its advertisement packet.
  - `CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_UUID`: The UUID for the additional service data.
  - `CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_DATA`: The data for the additional service.

- `CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS`: When enabled, the beacon's BLE address is periodically updated.
  - `CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS_PERIOD`: The period in seconds at which the address is updated.

## Testing

The `scan.py` tool can be used to test the BLE
beacon. The script scans for BLE devices, and when it finds a Hubble Network
beacon, it attempts to decode the advertisement data using the provided master
key.

### Running the Test Script

To run the test script, you need to provide the same master key that was provisioned into the device.

**For a raw key file:**

```sh
./scan.py master.key
```

**For a base64-encoded key file:**

Use the `-b` or `--base64` flag:

```sh
./scan.py -b master.key
```

The script will then start scanning for BLE advertisements and print the decoded data to the console.

### Ingesting Data into Hubble Network

The script can also ingest the scanned data into the Hubble Network. To do
this, you need to set the `HUBBLE_API_TOKEN` and `HUBBLE_ORG_ID` environment
variables and use the `-i` or `--ingest` flag.

```sh
export HUBBLE_API_TOKEN=<your_api_token>
export HUBBLE_ORG_ID=<your_org_id>
./scan.py -i master.key
```
