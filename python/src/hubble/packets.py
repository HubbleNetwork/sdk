# hubble/packets.py
from __future__ import annotations
from dataclasses import dataclass
from datetime import datetime
from typing import Optional


@dataclass(frozen=True)
class Location:
    """Geographic location (WGS84)."""

    lat: float
    lon: float
    alt_m: Optional[float] = None  # altitude meters, if known


@dataclass(frozen=True)
class EncryptedPacket:
    """A packet received locally (e.g., via BLE) that has not been decrypted."""

    timestamp: int  # timezone-aware UTC recommended
    location: Optional[Location]  # None if unknown
    payload: bytes  # opaque encrypted bytes
    rssi: int  # received signal strength (dBm)


@dataclass(frozen=True)
class DecryptedPacket:
    """A packet decrypted by backend or locally."""

    timestamp: int
    device_id: str
    device_name: str
    location: Optional[Location]
    tags: list[str]  # arbitrary tags
    payload: bytes  # decrypted payload bytes
    rssi: int  # received signal strength (dBm)
    counter: Optional[int] = None
    sequence: Optional[int] = None
