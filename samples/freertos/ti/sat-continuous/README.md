# Hubble Satellite Network on TI (FreeRTOS)

Welcome to this sample project demonstrating the integration of the
[Texas Instruments (TI) SimpleLink Low Power F3 SDK](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK)
with the [Hubble Device SDK](https://github.com/HubbleNetwork/hubble-device-sdk).

This project showcases how to integrate the Hubble Satellite Network
on TI device using FreeRTOS.

## Overview

This project is designed to:

- Demonstrate the use of the TI SDK together with the HubbleNetwork-SDK for satellite-enabled applications.
- Provide a practical starting point for developers integrating Hubble Satellite Network on TI hardware.

The project targets the **CC23xx** and **CC27xx** families and uses **FreeRTOS**
as the operating system. It is originally based on the TI
[`rfCarrierWave`](https://github.com/TexasInstruments/simplelink-prop_rf-examples/tree/main/examples/rtos/LP_EM_CC2340R5/prop_rf/rfCarrierWave)
example and has been extended to support Hubble Satellite transmission.

> [!NOTE]
> The sample uses the **device uptime** counter source
> (`CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME`), so it does not need UTC time
> provisioned: the EID counter used for encryption starts at 0 and advances with
> device uptime. Only the master key has to be embedded before building.

> [!WARNING]
> Make sure that your generated secret key from [Hubble API](https://hubble.com/docs/api-specification/register-new-devices)
> is using the same key size (AES-128) and counter source (DEVICE_UPTIME) as the sample.

## Features

- Integration with the **Hubble Device SDK** for satellite-specific transmission.
- FreeRTOS-based task and synchronization management.
- Modular and extensible code structure suitable for customization.

## Requirements

To build and run this project, you will need:

- A TI **CC23xx** (e.g. **LP_EM_CC2340R5**) or **CC27xx** (e.g. **LP_EM_CC2755P10**) development board
- The [TI SimpleLink Low Power F3 SDK](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK).
- The [TI toolchain](https://www.ti.com/tool/CCSTUDIO).
- The [HubbleNetwork-SDK](https://github.com/HubbleNetwork/hubble-device-sdk) cloned locally.

### Setup Instructions

1. **Install Dependencies**

   Ensure that the TI SDK is installed on your
   system. Set *SYSCONFIG_TOOL*, *SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR* and *TICLANG_ARMCOMPILER*
   environment variables. e.g:
```bash
export TICLANG_ARMCOMPILER=/path/to/ti/ti-cgt-armllvm
export SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/path/to/ti/simplelink_lowpower_f3_sdk
export SYSCONFIG_TOOL=/path/to/ti/sysconfig/sysconfig_cli.sh
```
2. **Provision Key**

The device's Hubble key must be baked into the firmware at build time. Use the
*embed_key_time.py* script to generate the key in hex. The tool will generate
`key.c` and `time.c` into your project's src/ directory.

```bash
python ../../../../tools/embed_key_time.py --base64 <path-to-key> -o src/
```

3. **Build the Project**

   Build the project using the provided *makefile*:

```bash
make -f cc2340r5.mk

# or make -f cc2755p10.mk
```

4. **Flash the Firmware**

   Flash the generated firmware (*sat-continuous.out*) onto the target device using your preferred flashing tool.

### Usage

Once the firmware is flashed:

1. Power on the development board.
2. The device will continuously transmit satellite packets.

### Key Files

+ **src/main.c**: Main thread that continuously transmit to satellite.
+ **src/hubble_ti_crypto.c**: This is a core file to integrate with HubbleNetwork SDK. It implements the required cryptographic API.
+ **makefile**: Build system for the project.

### Project Notes

The project uses different build system approaches:

- **CC23xx** does not use **Hubble Device SDK** SysConfig integration
- **CC27xx** uses **Hubble Device SDK** SysConfig integration
