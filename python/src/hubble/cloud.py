# hubble/cloud_api.py
from __future__ import annotations
import httpx
import time
import base64
from typing import Any, Optional
from collections.abc import MutableMapping
from .packets import EncryptedPacket, DecryptedPacket
from .device import Device
from .errors import (
    BackendError,
    NetworkError,
    APITimeout,
    raise_for_response,
)

_API_BASE: str = "https://api.hubble.com/api"


def _auth_headers(api_token: str) -> dict[str, str]:
    return {
        "Authorization": f"Bearer {api_token}",
        "Accept": "application/json",
        "Content-Type": "application/json",
    }


def _list_devices_endpoint(org_id: str) -> str:
    return f"/org/{org_id}/devices"


def _register_device_endpoint(org_id: str) -> str:
    return f"/v2/org/{org_id}/devices"


def _retrive_org_packets_endpoint(org_id: str) -> str:
    return f"/org/{org_id}/packets"


def _ingest_packets_endpoint(org_id: str) -> str:
    return f"/org/{org_id}/packets"


def cloud_request(
    method: str,
    path: str,
    *,
    api_token: Optional[str] = None,
    json: Any = None,
    timeout_s: float = 10.0,
    params: Optional[MutableMapping[str, Any]] = None,
) -> Any:
    """
    Make a single HTTP request to the Hubble Cloud API and return parsed JSON.

    - `method`: "GET", "POST", etc.
    - `path`: endpoint path (e.g., "/devices" or "orgs/{id}/devices")
    - `api_token`: API token for auth (optional, but recommended)
    - `org_id`: if provided, will be added as query param `orgId=<org_id>`
               (skip or embed in `path` if your endpoint uses a path param instead)
    - `json`: request JSON body (for POST/PUT/PATCH)
    - `timeout_s`: request timeout in seconds
    - `params`: optional HTTP request parameters
    """
    path = path.lstrip("/")
    url = f"{_API_BASE}/{path}"

    # headers
    headers: MutableMapping[str, str] = {
        "Accept": "application/json",
        "Content-Type": "application/json",
    }
    if api_token:
        headers["Authorization"] = f"Bearer {api_token}"

    try:
        with httpx.Client(timeout=timeout_s) as client:
            resp = client.request(
                method.upper(), url, params=params, headers=headers, json=json
            )
    except httpx.TimeoutException as e:
        raise APITimeout(f"Request timed out: {method} {url}") from e
    except httpx.HTTPError as e:
        raise NetworkError(f"Network error: {method} {url}: {e}") from e

    if resp.status_code != 200:
        body = None
        try:
            body = resp.json()
        except Exception:
            body = resp.text
        raise_for_response(resp.status_code, body=body)

    # Parse JSON body
    try:
        return resp.json()
    except ValueError as e:
        # Server said "application/json" but body isn't JSON
        raise BackendError(f"Non-JSON response from {url}") from e


def ingest(*, org_id: str, api_token: str, packet: EncryptedPacket) -> None:
    """Push an encrypted packet to the cloud."""
    pass


def register_device(
    *, org_id: str, api_token: str, name: Optional[str] = None
) -> Device:
    """Create a new device and return it."""
    data = {
        "n_devices": 1,
        "encryption": "AES-256-CTR",
    }
    return cloud_request(
        method="POST",
        path=_register_device_endpoint(org_id),
        api_token=api_token,
        json=data,
    )


def list_devices(*, org_id: str, api_token: str) -> list[Device]:
    """
    List devices for the org (keys typically omitted).

    Returns:
        json response from server

    """
    return cloud_request(
        method="GET",
        path=_list_devices_endpoint(org_id),
        api_token=api_token,
    )


def retrieve_packets(
    *, org_id: str, api_token: str, device_id: Optional[str] = None, days: int = 7
) -> Optional[DecryptedPacket]:
    """Fetch decrypted packets for a device."""
    params = {"start": (int(time.time()) - (days * 24 * 60 * 60))}
    if device_id:
        params["device_id"] = device_id
    return cloud_request(
        method="GET",
        path=_retrive_org_packets_endpoint(org_id),
        api_token=api_token,
        params=params,
    )


def ingest_packet(*, org_id: str, api_token: str, packet: EncryptedPacket) -> None:
    body = {
        "ble_locations": [
            {
                "location": {
                    "latitude": packet.location.lat,
                    "longitude": packet.location.lon,
                    "timestamp": packet.timestamp,
                    "horizontal_accuracy": 42,
                    "altitude": 42,
                    "vertical_accuracy": 42,
                },
                "adv": [
                    {
                        "payload": base64.b64encode(packet.payload).decode("utf-8"),
                        "rssi": packet.rssi,
                        "timestamp": packet.timestamp,
                    }
                ],
            }
        ]
    }
    return cloud_request(
        method="POST",
        path=_ingest_packets_endpoint(org_id),
        api_token=api_token,
        json=body,
    )
