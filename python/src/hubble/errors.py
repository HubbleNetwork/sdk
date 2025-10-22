"""
Exception hierarchy for the Hubble Python SDK.

Usage:
    from .errors import (
        BackendError, ServerError, ScanError, DecryptionError,
        ValidationError, ScanError, DecryptionError, raise_for_response,
    )
"""

from __future__ import annotations
from typing import Any, Optional


# ----- Base classes ---------------------------------------------------------


class HubbleError(Exception):
    """Base exception for all SDK errors."""


# Cloud/backend-facing errors
class BackendError(HubbleError):
    """Generic back end error from the Hubble Cloud API"""


class RequestError(BackendError):
    """Bad request error from the Hubble Cloud API."""


class InternalServerError(BackendError):
    """Server side error from the Hubble Cloud API."""


class NetworkError(BackendError):
    """Transport-layer failures (DNS, connection reset, etc.)."""


class APITimeout(BackendError):
    """The API call exceeded its allowed timeout."""


# Request/response semantics
class ValidationError(BackendError):
    """The request was invalid (schema/semantics)."""


# Local/host-side errors
class ScanError(HubbleError):
    """BLE scanning failed locally (adapter/permissions/OS/driver)."""


class DecryptionError(HubbleError):
    """Local decryption failed (bad key, corrupt packet, etc.)."""


# Demo errors
class InvalidDeviceError(HubbleError):
    """Invalid device for a given task"""


class ElfFetchError(RuntimeError):
    """Generic failure to fetch or parse an ELF from the Hubble TLDM repo."""


class FlashError(RuntimeError):
    """Generic failure during flashing or target connection."""


__all__ = [
    "HubbleError",
    "RequestError",
    "InternalServerError",
    "ScanError",
    "DecryptionError",
    "raise_for_response",
    "map_http_status",
]


# ----- Helpers for HTTP client code ----------------------------------------


def map_http_status(status_code: int, detail: Optional[str] = None) -> BackendError:
    """
    Map an HTTP status code to a concrete exception instance.
    `detail` should be a short server-provided error message if available.
    """
    msg = f"{status_code}: {detail or 'unexpected response'}"

    if status_code == 400:
        return RequestError(msg)
    if status_code == 500:
        return InternalServerError(msg)
    return BackendError(msg)


def raise_for_response(
    status_code: int,
    body: Any = None,
    *,
    default_message: str = "",
) -> None:
    """
    Raise a specific BackendError subclass based on `status_code` and optional `body`.

    `body` can be a parsed JSON object, a string, or None; we try to extract a helpful
    message from common fields like 'error' or 'message'.
    """
    detail = None
    if isinstance(body, dict):
        detail = (
            body.get("error_description")
            or body.get("error")
            or body.get("message")
            or body.get("detail")
        )
    elif isinstance(body, str):
        detail = body.strip() or None

    if not detail:
        detail = default_message or None

    raise map_http_status(status_code, detail)
