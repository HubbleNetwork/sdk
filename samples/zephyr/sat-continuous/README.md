# Hubble Network Satellite Continuous Sample

This sample continuously builds Hubble satellite packets and transmits them in a
loop. Use it as a starting point for satellite (space) connectivity on a
supported radio.

The sample uses the **device uptime** counter source
(`CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`), so it does not need UTC time
provisioned: the EID counter used for encryption starts at 0 and advances with
device uptime. Only the master key has to be embedded before building.

> [!NOTE]
> Satellite functionality is pre-production and not yet ready for production
> deployments.

## Requirements

- A cryptographic key provided by Hubble Network.
- A supported satellite-capable board (see [Supported boards](#supported-boards)).
- For Nordic and Silicon Labs targets: the prebuilt radio blob (see
  [Radio blobs](#radio-blobs)).

## Radio blobs

The satellite radio implementation for Nordic SoCs ships as a prebuilt static
library (a "blob") rather than source. These are **not** fetched by
`west update`; you must pull them in explicitly once after setting up the
workspace:

```sh
west blobs fetch hubblenetwork-sdk
```

This downloads the satellite libraries for the nRF52/nRF53/nRF54 families into
the module's `zephyr/blobs/` directory. If the blob for your target is missing,
the build fails to link the satellite radio. Run `west blobs list
hubblenetwork-sdk` to see what is available and whether it has been fetched.

Silicon Labs targets (`xg24_rb4187c`, `xiao_mg24`) use the in-tree Gecko custom
radio PHY (`CONFIG_SOC_GECKO_CUSTOM_RADIO_PHY`), but that PHY links against
Silicon Labs' RAIL library, which is itself distributed as a blob. Fetch it once
before building for those boards:

```sh
west blobs fetch hal_silabs
```

## Building and running

Pass your base64 master key (the string Hubble gave you, already base64) on the
build command line with `-DCONFIG_SAMPLE_HUBBLE_KEY`. The string is embedded in the firmware
and decoded into the master key on startup using Zephyr's `base64_decode`, so
there is no pre-build script and no generated source.

```sh
west blobs fetch hubblenetwork-sdk    # once, for Nordic targets
west build -b nrf54l15dk/nrf54l15/cpuapp . -- -DCONFIG_SAMPLE_HUBBLE_KEY="<your base64 key>"
west flash
```

After flashing, the device builds a satellite packet and transmits it every
`CONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS` seconds (10 by default).

> [!WARNING]
> The decoded key length must match `CONFIG_HUBBLE_KEY_SIZE` (32 bytes for a
> 256-bit key, 16 for 128-bit). Make sure that your generated secret key from
> [Hubble API](https://hubble.com/docs/api-specification/register-new-devices)
> is using the same key size and counter source (DEVICE_UPTIME) as the sample.
> A wrong-size key is rejected at startup with a logged error and the sample exits;
> if `-DCONFIG_SAMPLE_HUBBLE_KEY` is omitted the firmware uses an all-zero placeholder
> (with a warning) so it still builds and
> runs in CI.

> [!TIP]
> `-DCONFIG_SAMPLE_HUBBLE_KEY` is cached in the build directory, so subsequent incremental
> `west build` runs reuse it without repeating the flag. Pass it again (or use
> `-p always`) to change the key.

## Supported boards

A board is wired up by providing a `boards/<board>.conf` (and optionally an
overlay). Boards that supply their own satellite radio support set
`CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT=n`; otherwise the sample mocks the
board radio APIs so it still builds (e.g. on `native_sim`).

| Board | Notes |
|-------|-------|
| `nrf54l15dk/nrf54l15/cpuapp` | Nordic nRF54L15 — needs the nRF54 blob |
| `nrf21540dk` | Nordic nRF52840 + FEM — needs the nRF52 blob |
| `thingy53/nrf5340/cpunet` | Nordic nRF5340 net core — needs the nRF53 blob |
| `xg24_rb4187c` | Silicon Labs xG24 — uses the Gecko custom radio PHY |
| `xiao_mg24` | Seeed XIAO MG24 — uses the Gecko custom radio PHY |

## Configuration

- `-DCONFIG_SAMPLE_HUBBLE_KEY`: the base64-encoded Hubble master key, embedded at build time
  and decoded on the device at startup (see
  [Building and running](#building-and-running)).
- `CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`: use the device uptime counter
  source for EID/encryption so no UTC time provisioning is required (enabled in
  `prj.conf`).
- `CONFIG_HUBBLE_NETWORK_SECURITY_ENFORCE_NONCE_CHECK`: nonce-reuse enforcement,
  which persists the sequence counter to NVS and rejects reused/decreasing
  counters. Disabled (`n`) in `prj.conf` for this sample, which transmits
  continuously and needs no NVS.
- `CONFIG_SAMPLE_SAT_TX_INTERVAL_SECONDS`: seconds the sample sleeps between
  consecutive satellite transmissions (default `5`).
- `CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT`: when `n`, the board provides real
  satellite radio support; when `y` (default), the sample mocks the board radio
  APIs.
