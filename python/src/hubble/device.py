# hubble/device.py
from __future__ import annotations
from dataclasses import dataclass
from typing import Dict, List, Optional
from .packets import EncryptedPacket, DecryptedPacket


@dataclass
class Device:
    """
    Represents a device; may or may not hold a key for local decryption.
    If created via Organization API calls, key is typically None.
    """

    id: str
    key: Optional[bytes] = None
    name: Optional[str] = None
    tags: Optional[List[Dict]] = None
    created_ts: Optional[int] = None
    active: Optional[bool] = False

    @classmethod
    def from_json(cls, json):
        return cls(
            id=str(json.get("id")),
            name=json.get("name"),
            tags=json.get("tags"),
            created_ts=json.get("created_ts"),
            active=json.get("active"),
        )

    def decrypt_packet(self, packet: EncryptedPacket) -> Optional[DecryptedPacket]:
        """
        Decrypt packet using this device's key (if present).
        Returns a DecryptedPacket if `key` is set, else None.
        """
        pass
