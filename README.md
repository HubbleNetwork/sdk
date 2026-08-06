# Hubble Device SDK

Add global connectivity to your device using its Bluetooth® chip.

## What This Does

The SDK encodes your data into Bluetooth Low Energy (BLE) advertisements that use the Hubble Service UUID. Hubble's Terrestrial Network of 90M+ gateways pick up these packets and deliver your custom payload and device location data to your backend via API.

No cellular modem. No SIM card. No gateway hardware to deploy. Just integrate the **Hubble Device SDK** with your firmware.

## Supported Platforms

Works with any Bluetooth LE 5.0+ chip — including hardware from Texas Instruments, Nordic, Silicon Labs, Espressif, InPlay, and any other BLE-compliant silicon.

→ [Full MCU / SoC compatibility list](https://hubble.com/docs/compatibility)

## Quick Start

**Fastest path:** follow the [Dash Quick Start](https://hubble.com/docs/guides/dashboard/dash-quick-start) in your Hubble Dashboard for a supported developer kit. You'll have your first "device" transmitting on Terrestrial Network in minutes.

**Reference apps:** [clone a complete application repository](https://hubble.com/docs/compatibility) for your specific Bluetooth chip or MCU.

**Integrating into your existing firmware:** follow [step-by-step instructions for sample applications](https://hubble.com/docs/device-sdk/terrestrial/samples) that match your RTOS.
   - Zephyr
   - FreeRTOS (TI SDK)
   - ESP-IDF (Espressif)
   - Bare-metal

## Sign Up

Get started by signing up for your **Hubble Dashboard** for free: [dash.hubble.com](https://dash.hubble.com/login)

### Onboard Your First Device
- Generate a device encryption key using the [Cloud API](https://hubble.com/docs/api-specification/register-new-devices), using the [Hubble CLI](https://hubble.com/docs/guides/cli), or directly in Dash ("Add a device", then "I just need a device key"). 
- Flash your device and start beaconing on Terrestrial Network.
- See your data come through in Dash, [via API or webhook](https://hubble.com/docs/guides/cloud-integration/get-data).

## Resources

| | |
|---|---|
| **Sign Up or Sign In** | [dash.hubble.com](https://dash.hubble.com) |
| **Terrestrial Coverage** | [network.hubble.com](https://network.hubble.com/) |
| **Device SDK Documentation** | [hubblenetwork.github.io/hubble-device-sdk/main](https://hubblenetwork.github.io/hubble-device-sdk/main/) |
| **Cloud API Reference** | [hubble.com/docs/api-specification/hubble-platform-api](https://hubble.com/docs/api-specification/hubble-platform-api) |
| **Network & Security** | [hubble.com/docs/security/network](https://hubble.com/docs/security/network) |
| **How Terrestrial Network Works** | [hubble.com/docs/network/terrestrial/how-it-works](https://hubble.com/docs/network/terrestrial/how-it-works) |
| **Careers** | [hubble.com/careers](https://hubble.com/careers) |

## Questions & Support

- [GitHub Discussions](https://github.com/HubbleNetwork/hubble-device-sdk/discussions) — ask questions, share projects
- [GitHub Issues](https://github.com/HubbleNetwork/hubble-device-sdk/issues) — report bugs, request features
- [Contact Us](https://hubble.com/contact-us) — get a demo
