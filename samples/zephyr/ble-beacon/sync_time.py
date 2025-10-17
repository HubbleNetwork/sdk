#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import asyncio
import datetime
import os
import signal
import struct
import sys
import time


from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData


CTS_SERVICE_UUID        =         "00001805-0000-1000-8000-00805f9b34fb"
CTS_CHARACTERISTIC_UUID =         "00002a2b-0000-1000-8000-00805f9b34fb"
HUBBLE_BLE_UUID_SYNC    =         "0000fca7"


def cts_time_get() -> bytes:
    now = datetime.datetime.now(datetime.UTC)

    year = now.year
    month = now.month
    day = now.day
    hour = now.hour
    minute = now.minute
    second = now.second
    day_of_week = (now.isoweekday())  # Monday=1 … Sunday=7
    fractions256 = int(now.microsecond / 1_000_000 * 256)
    adjust_reason = 0  # No adjustment

    # Pack into little-endian structure
    cts_payload = struct.pack(
        "<HBBBBBB2B",
        year, month, day, hour, minute, second,
        day_of_week, fractions256, adjust_reason
    )

    return cts_payload


async def scan(stop_event: asyncio.Event) -> None:
    def match_hubble_sync_uuid(device: BLEDevice, adv: AdvertisementData):
        for uuid in adv.service_uuids:
            if uuid.startswith(HUBBLE_BLE_UUID_SYNC):
                return True

        return False

    device = await BleakScanner.find_device_by_filter(match_hubble_sync_uuid)
    if device is not None:
        async with BleakClient(device) as client:
            data = bytearray(cts_time_get())
            print(f"Setting time to: {datetime.datetime.now().isoformat()}")
            await client.write_gatt_char(CTS_CHARACTERISTIC_UUID, data, response=True)
    else:
        print("No device found")


def main() -> None:
    stop_event = asyncio.Event()
    signal.signal(signal.SIGINT, lambda _, __: stop_event.set())

    asyncio.run(scan(stop_event))


if __name__ == '__main__':
    main()
