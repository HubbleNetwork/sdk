# hubble/ble.py
from __future__ import annotations

import asyncio
from datetime import datetime, timezone
from typing import Optional
import geocoder

from bleak import BleakScanner

# Import your dataclass
from .packets import (
    Location,
    EncryptedPacket,
)

"""
16-bit UUID 0xFCA6 in 128-bit Bluetooth Base UUID form

Bluetooth spec defines a base UUID 0000xxxx-0000-1000-8000-00805F9B34FB.
Any 16-bit (or 32-bit) UUID is expanded into that base by substituting xxxx.

Libraries normalize to consistent 128-bit strings so you don’t have to guess
whether a platform will report 16- vs 128-bit in scan results.

In bleak, AdvertisementData.service_uuids and the keys in AdvertisementData.service_data
are 128-bit strings. So matching against the normalized 128-bit form is the most portable.
"""
_TARGET_UUID = "0000fca6-0000-1000-8000-00805f9b34fb"


def _get_location() -> Location:
    geo = geocoder.ip("me")
    lat, lon = geo.latlng
    return Location(lat=lat, lon=lon)


def scan(timeout: float) -> Optional[EncryptedPacket]:
    """
    Scan for a BLE advertisement that includes service data for UUID 0xFCA6 and
    return it as an EncryptedPacket (payload=data bytes, rssi from the adv).
    Returns None if nothing is found within `timeout` seconds.

    This is a synchronous convenience wrapper around an asyncio scanner.
    """

    async def _scan_async(ttl: float) -> List[EncryptedPacket]:
        done = asyncio.Event()
        packets = []

        def on_detect(device, adv_data) -> None:
            nonlocal packets
            # Normalize to a dict; bleak provides service_data as {uuid_str: bytes}
            service_data = getattr(adv_data, "service_data", None) or {}
            payload = None

            # Keys are 128-bit UUID strings; compare lowercased
            for uuid_str, data in service_data.items():
                if (uuid_str or "").lower() == _TARGET_UUID:
                    payload = bytes(data)
                    break

            if payload is not None:
                rssi = getattr(adv_data, "rssi", getattr(device, "rssi", 0)) or 0
                packets.append(
                    EncryptedPacket(
                        timestamp=int(datetime.now(timezone.utc).timestamp()),
                        location=_get_location(),
                        payload=payload,
                        rssi=int(rssi),
                    )
                )

        # Start scanning and wait for first match or timeout
        async with BleakScanner(detection_callback=on_detect):
            try:
                await asyncio.wait_for(done.wait(), timeout=ttl)
            except asyncio.TimeoutError:
                pass

        return packets

    # Run the async scanner. If there's already a running event loop (e.g., Jupyter),
    # you can adapt this to use `await _scan_async(timeout)` instead.
    try:
        return asyncio.run(_scan_async(timeout))
    except RuntimeError:
        # Fallback for environments with an active loop (e.g., notebooks/async apps)
        loop = asyncio.get_event_loop()
        if loop.is_running():
            # Create a task and block until it’s done via a new Future
            return loop.run_until_complete(_scan_async(timeout))  # type: ignore[func-returns-value]
        return loop.run_until_complete(_scan_async(timeout))
