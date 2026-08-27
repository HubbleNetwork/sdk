# Hubble Network Satellite Pass-Prediction Sample

This sample times satellite transmissions to **predicted passes**. It waits for
the next pass of a Hubble satellite over the device's location, transmits one
packet, then waits for the following pass.

Orbital parameters are **hard-coded** into the firmware but **generated** from
Hubble's live ephemeris API by `tools/orbital_params_fetch.py` before building.
The master key and device location are provisioned at build time.

> [!NOTE]
> Satellite functionality is pre-production and not yet ready for production
> deployments.

## The two clocks

- **Crypto** uses the device uptime counter source
  (`CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`), so it needs no UTC: the EID
  counter starts at 0 and advances with uptime.
- **Pass prediction** needs real Unix time. CMake captures the build's
  wall-clock time into `SAMPLE_BUILD_UNIX_TIME`; at startup the firmware seeds
  the SDK clock with `hubble_time_set()` and reads it back with
  `hubble_time_get()`, which advances with uptime. The timestamp is captured at
  CMake *configure* time, so build with `-p always` right before flashing to
  keep it fresh.

## Requirements

- A cryptographic key provided by Hubble Network.
- A `HUBBLE_API_TOKEN` (JWT) for the ephemeris API.
- A supported satellite-capable board, or `native_sim` for build-only checks.
- For Nordic/Silicon Labs targets: the prebuilt radio blob (see below).

## Radio blobs

The satellite radio for Nordic SoCs ships as a prebuilt static library. Fetch
it once after setting up the workspace:

```sh
west blobs fetch hubblenetwork-sdk
```

Silicon Labs targets (`xg24_rb4187c`, `xiao_mg24`) use the in-tree Gecko custom
radio PHY, which links Silicon Labs' RAIL blob:

```sh
west blobs fetch hal_silabs
```

## Generate orbital parameters

`src/sat_params.c` is committed as an obvious dummy placeholder. Generate the
real table from the ephemeris API before building:

```sh
pip install requests    # tools/orbital_params_fetch.py dependency
export HUBBLE_API_TOKEN="<JWT>"
python3 tools/orbital_params_fetch.py samples/zephyr/sat-pass-prediction/src
```

This writes `src/sat_params.generated.c` with the `sats[]` table, one entry per
satellite returned by the API. Values are copied verbatim (orbits/second — the
units the SDK expects); no conversion is applied. The element count is derived
in `src/satellites.c`, so no regeneration of other files is needed.

`sat_params.generated.c` is gitignored, and the build prefers it over the
committed `sat_params.c` placeholder — so regenerating it (e.g. to refresh
ephemeris before a flash) never shows up as a tree change.

## Building and running

```sh
west blobs fetch hubblenetwork-sdk    # once, for Nordic targets
west build -p always -b nrf54l15dk/nrf54l15/cpuapp samples/zephyr/sat-pass-prediction -- \
    -DCONFIG_SAMPLE_HUBBLE_KEY="<your base64 key>" \
    -DCONFIG_SAMPLE_DEVICE_LAT="37.7749" \
    -DCONFIG_SAMPLE_DEVICE_LON="-122.4194"
west flash
```

`-p always` is recommended so the build timestamp (and any changed key/location)
is captured fresh. After flashing, the device logs each predicted pass and
transmits a packet when the pass window opens.

## Telemetry shell

The sample registers a `sat` shell command (over the console UART) for inspecting
runtime state without parsing logs:

| Command | Output |
|---------|--------|
| `sat status` | Current Unix time, uptime, device location, configured satellite count, total transmissions, and time of the last transmission. |
| `sat next` | The next predicted pass: start, culmination, duration, max elevation, longitude, and seconds until start. |

The commands read cached telemetry published by the main loop, so they never
interfere with pass prediction or transmission. Disable the shell with
`-DCONFIG_SHELL=n` to drop it (and its footprint) entirely.

> [!NOTE]
> The decoded key length must match `CONFIG_HUBBLE_KEY_SIZE` (32 bytes for a
> 256-bit key, 16 for 128-bit). A wrong-size key is rejected at startup; if
> `-DCONFIG_SAMPLE_HUBBLE_KEY` is omitted an all-zero placeholder is used so the
> sample still builds and runs in CI.

## Supported boards

| Board | Notes |
|-------|-------|
| `nrf54l15dk/nrf54l15/cpuapp` | Nordic nRF54L15 — needs the nRF54 blob |
| `nrf21540dk` | Nordic nRF52840 + FEM — needs the nRF52 blob |
| `thingy53/nrf5340/cpunet` | Nordic nRF5340 net core — needs the nRF53 blob |
| `xg24_rb4187c` | Silicon Labs xG24 — Gecko custom radio PHY |
| `xiao_mg24` | Seeed XIAO MG24 — Gecko custom radio PHY |

## Configuration

- `CONFIG_SAMPLE_HUBBLE_KEY`: base64 master key, embedded at build time.
- `CONFIG_SAMPLE_DEVICE_LAT` / `CONFIG_SAMPLE_DEVICE_LON`: device location in
  degrees, parsed with `strtod` and passed to the pass-prediction API.
- `CONFIG_SAMPLE_SAT_RELIABILITY_NORMAL` (default) /
  `CONFIG_SAMPLE_SAT_RELIABILITY_HIGH`: transmission reliability mode.
- `CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT`: when `n`, the board provides real
  satellite radio support; when `y` (default), the sample mocks the board radio
  APIs.
