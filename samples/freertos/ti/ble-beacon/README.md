Welcome to the sample project that demonstrates the integration of the
[Texas Instruments (TI) SDK](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK)
with the [HubbleNetwork-SDK](https://github.com/HubbleNetwork/sdk). This
project showcases the development of a BLE (Bluetooth Low Energy)
application using FreeRTOS, leveraging the capabilities of both SDKs.

## Overview

This project is designed to:

+ Demonstrate the use of the TI SDK for BLE application development.
+ Showcase the integration of the HubbleNetwork SDK for BLE-specific operations.
+ Provide a starting point for developers working on BLE applications with TI devices.

The project targets the *CC23xx* family of devices and uses *FreeRTOS*
as the operating system. It is originally a copy of
[basic-ble](https://github.com/TexasInstruments/simplelink-ble5stack-examples/tree/main/examples/rtos/LP_EM_CC2340R5/ble5stack/basic_ble)
sample.

### Features

+ Integration with the HubbleNetwork-SDK for BLE-specific operations.
+ FreeRTOS-based task management.
+ Modular and extensible codebase.

### Requirements

To build and run this project, you will need:

+ A TI CC23xx development board (e.g., **LP_EM_CC2340R5**).
+ The [TI SDK](https://www.ti.com/tool/download/SIMPLELINK-LOWPOWER-F3-SDK) installed on your system.
* [TI toolchain](https://www.ti.com/tool/CCSTUDIO)
+ The [HubbleNetwork-SDK](https://github.com/HubbleNetwork/hubblenetwork-sdk) cloned into the project directory.
+ Python 3 for running the *embed_key_utc.py* script.

### Project Structure
+ **app**: Contains application-specific source files.
+ **common**: Contains shared utilities and startup code.
+ **freertos**: FreeRTOS-specific configuration and build files.
+ **embed_key_utc.py**: Script to embed a BLE key and UTC timestamp into the firmware.

### Setup Instructions

1. **Install Dependencies**

   Ensure that the TI SDK is installed on your
   system. Set *SYSCONFIG_TOOL*, *SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR* and *TICLANG_ARMCOMPILER*
   environment variables. e.g:
```
   export TICLANG_ARMCOMPILER=/Applications/ti/ccs1281/ccs/tools/compiler/ti-cgt-armllvm_3.2.1.LTS/
   export SIMPLELINK_LOWPOWER_F3_SDK_INSTALL_DIR=/Applications/ti/simplelink_lowpower_f3_sdk_9_10_00_83
   export SYSCONFIG_TOOL=/Applications/ti/sysconfig_1.23.1/sysconfig_cli.sh
```

2. **Embed Key and UTC**

   Use the *embed_key_utc.py* script to provision a BLE key and UTC timestamp:

```
   python embed_key_utc.py --base64 <path-to-key>
```

3. **Build the Project**

   Build the project using the provided *Makefile*:

```
   make -C freertos/ticlang
```

4. **Flash the Firmware**

   Flash the generated firmware (*basic_ble.out*) onto the target device using your preferred flashing tool.

### Usage

Once the firmware is flashed:

1. Power on the development board.
2. The device will start BLE advertising.

### Key Files

+ **src/hubble_ble_adv.c**: BLE advertising implementation.
+ **src/hubble_ble_ti.c**: This is a core file to integrate with HubbleNetwork SDK. It implements the required cryptograhic API.
+ **Makefile**: Build system for the project.
