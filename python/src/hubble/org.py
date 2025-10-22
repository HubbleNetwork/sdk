# hubble/org.py
from __future__ import annotations
from dataclasses import dataclass
from typing import Optional, List

from . import cloud
from .packets import DecryptedPacket, Location
from .device import Device


@dataclass
class Organization:
    """
    Organization-scoped operations that require org ID and API token.
    Used to manage devices and fetch decrypted packets from the backend.
    """

    org_id: str
    api_token: str

    # Optional per-request timeout
    timeout_s: float = 10.0

    def register_device(self, name: Optional[str] = None) -> Device:
        """
        Register a new device in this organization and return it.
        Returned Device will have an ID and provisioned key.
        """
        resp = cloud.register_device(
            org_id=self.org_id, api_token=self.api_token, name=name
        )
        # Currently, only registering a single device and taking the
        # first in the returned list
        device = resp["devices"][0]
        return Device(id=device["device_id"], key=device["key"], name=name)

    def list_devices(self) -> list[Device]:
        """
        Call the Cloud API “List Devices” endpoint and return Device objects.

        Returns:
            list[Device]
        """

        payload = cloud.list_devices(org_id=self.org_id, api_token=self.api_token)
        raw_list = payload["devices"]

        # Turn each JSON object into a Device
        devices: List[Device] = []
        for item in raw_list:
            devices.append(Device.from_json(item))
        return devices

    def retrieve_packets(
        self, device: Device, days: int = 7
    ) -> Optional[DecryptedPacket]:
        """
        Return the most recent decrypted packet for the given device,
        or None if none exists.
        """
        resp = cloud.retrieve_packets(
            org_id=self.org_id, api_token=self.api_token, device_id=device.id, days=days
        )
        packets = []
        for packet in resp["packets"]:
            packets.append(
                DecryptedPacket(
                    timestamp=int(packet["device"]["timestamp"]),
                    device_id=packet["device"]["id"],
                    device_name=packet["device"]["name"]
                    if "name" in packet["device"]
                    else "",
                    location=Location(
                        lat=packet["location"]["latitude"],
                        lon=packet["location"]["longitude"],
                    ),
                    tags=None,
                    payload=packet["device"]["payload"],
                    rssi=packet["device"]["rssi"],
                    counter=packet["device"]["counter"],
                    sequence=packet["device"]["sequence_number"],
                )
            )
        return packets

    def ingest_packet(self, packet: EncryptedPacket) -> None:
        cloud.ingest_packet(org_id=self.org_id, api_token=self.api_token, packet=packet)
