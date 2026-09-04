# Hubble Network BLE Beacon Sample

This sample application demonstrates how to use the Hubble Device SDK to
create a BLE beacon that advertises its presence.

## Requirements

- Cryptographic key provided by Hubble Network
- To keep the key out of the firmware, see
  [Using a key stored in the KMU](#using-a-key-stored-in-the-kmu). That mode
  requires an nRF54L15 device and the nRF Connect SDK.

## Overview

The beacon advertises data that can be picked up by the Hubble Network. The
advertised data is generated using the `hubble_ble_advertise_get()` function
from the Hubble BLE library.

The sample requires a master key and the current Unix time to be provisioned
into the device. This is done by running the `embed_key_time.py` script before
building the application.

## Provisioning

The provisioning process embeds a master key and the current Unix time into the
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

The `embed_key_time.py` script takes the key file and embeds it along with the
current timestamp into the source code (`src/key.c` and `src/unix_time.c`).

**For a raw key file:**

```sh
# Script is located in SDK_BASE/tools

python ../../../tools/embed_key_time.py master.key -o ./src
```

**For a base64-encoded key file:**

Use the `-b` or `--base64` flag:

```sh
# Script is located in SDK_BASE/tools

python ../../../tools/embed_key_time.py -b master.key -o ./src
```

After running the script, the key and timestamp will be compiled into the application.

## Using a key stored in the KMU

Instead of embedding the key material in the firmware, the SDK can reference
the key by its PSA key identifier. When
`CONFIG_HUBBLE_NETWORK_CRYPTO_PSA_USE_KEY_ID` is enabled, the SDK never needs
nor has access to the key material: the key is provisioned into the Key
Management Unit (KMU) and every cryptographic operation is performed by CRACEN
on the key it holds internally, so the key never leaves the secure storage.

### Requirements

- An nRF54L15 device, for example `nrf54l15dk/nrf54l15/cpuapp`. The KMU and the
  CRACEN cryptographic accelerator are only available on this family.
- The nRF Connect SDK, which provides the CRACEN PSA driver.
- `nrfutil` with the `device` command, used to provision the key.

### 1. Generate the key attributes

The `ncs-generate-key.py` script describes the key for `nrfutil`: it takes the
base64-encoded key and writes a JSON file with the key material and its PSA
attributes (AES-CMAC, sign usage, PROTECTED usage scheme).

```sh
# Script is located in SDK_BASE/tools

python ../../../tools/ncs-generate-key.py --id 1 --key "$(cat master.key)" --file keyslot.json
```

The sample expects the key in **KMU slot 1**. `src/key_id.c` builds the key
identifier with `PSA_KEY_ID_FROM_CRACEN_KMU_SLOT(
CRACEN_KMU_KEY_USAGE_SCHEME_PROTECTED, PSA_KEY_ID_USER_MIN)`, and
`PSA_KEY_ID_USER_MIN` is slot 1.

> [!NOTE]
> A KMU slot holds 128 bits of key material, so a 256-bit key is stored across
> slots 1 and 2. Pass `--key-bits 128` for a 128-bit key, and make sure the
> size matches the one selected with `CONFIG_HUBBLE_NETWORK_KEY_256` or
> `CONFIG_HUBBLE_NETWORK_KEY_128`.

Run `python ../../../tools/ncs-generate-key.py --help` for the remaining
options, such as `--persistence` to control whether the slot can be reused,
revoked, or written only once.

### 2. Provision the key

```sh
nrfutil device x-provision-keys --key-file keyslot.json
```

### 3. Build the sample

The `ncs_cracen.conf` fragment selects the CRACEN driver and enables the key id
support in the SDK:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp . -- -DEXTRA_CONF_FILE=ncs_cracen.conf
```

## Building and Running

Once the key and time are provisioned, you can build and flash the application to your target board.

```sh
west build -b nrf52840dk/nrf52840 .
west flash
```

After flashing, the device will start advertising as a Hubble BLE beacon.

> [!WARNING]
> If the application was built with `CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS` enabled, it is necessary
> to sync time. This can be done using the `sync_time.py` script:
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
