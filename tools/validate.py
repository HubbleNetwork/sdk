#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import click
import os
import time
import base64
import uuid
from datetime import datetime
from typing import Optional
import hubblenetwork


def _get_env_or_fail(name: str) -> str:
    val = os.getenv(name)
    if not val:
        raise click.ClickException(f"{name} environment variable not set")
    return val


def info(msg):
    click.secho(f"[INFO] ", fg="cyan", bold=True, nl=False)
    click.echo(msg + "... ", nl=False)


def error(msg):
    click.secho("[ERROR]", fg="red", bold=True)
    click.secho(f"\n{msg}", bold=True)
    exit(1)


def success():
    click.secho("[SUCCESS]", fg="green", bold=True)


def get_pkt_from_be_with_timestamp(org, device, timestamp):
    backend_pkts = org.retrieve_packets(device, days=1)
    backend_pkt = None
    for p in backend_pkts:
        if p.timestamp == timestamp:
            backend_pkt = p
            break
    return backend_pkt


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def cli() -> None:
    """Hubble SDK CLI."""
    # top-level group; subcommands are added below


@click.command()
@click.option(
    "--key",
    "-k",
    type=str,
    required=True,
    show_default=False,
    help="Device key (to test packet encryption)",
)
@click.option(
    "--device-id",
    "-d",
    type=str,
    required=True,
    show_default=False,
    help="Device ID (to test backend)",
)
def validate(key: str, device_id: str) -> None:
    """
    Validates the operation of a Hubble device, including:

    \b
    - Valid credentials passed in
    - Device registration (must be a registered device)
    - BLE advertisements
    - Advertisement encryption
    - Backend ingestion/retrieval of data

    NOTE: HUBBLE_ORG_ID and HUBBLE_API_TOKEN env vars must be set
    prior to running this script.
    """

    """
    Validate inputs
    """
    info("Validating format of inputs")
    try:
        decoded_key = bytearray(base64.b64decode(key))
    except Exception as e:
        error(
            'Incorrectly formatted device key passed in. Must be a base 64\
            \nencoded string such as (fake keys):\
            \n 16byte key: "q9vH3u2J4aN8Rw1KpZsO+A=="\
            \n 32byte key: "N4e7xq9X1pQ0sVbY2mT3uA6fH9rK2dW5cG8jL1oQ0vU="\
            \nNote the "=" characters at the end which must be included.'
        )
    try:
        u = uuid.UUID(device_id)
    except ValueError:
        error(
            'Device UUID formatted incorrectly.\
            \nMust be in standard 8-4-4-4-12 format (removing hyphens accepted).\
            \nExample UUID: "3f4b2c0c-2d43-4cbe-9c1f-0a4c2d59e2a1"\
            \n\nIf you are having troubles with your UUID please contact support@hubble.com'
        )
    success()

    """
    Ensure the credentials are available
    """
    info("Getting organization ID and API token")
    try:
        org_id = _get_env_or_fail("HUBBLE_ORG_ID")
        api_token = _get_env_or_fail("HUBBLE_API_TOKEN")
    except click.ClickException as e:
        error(e)
    success()

    """
    Validate the org credentials
    """
    info("Validating organization credentials")
    try:
        org = hubblenetwork.Organization(
            org_id=org_id,
            api_token=api_token,
        )
    except hubblenetwork.InvalidCredentialsError as e:
        error("Invalid credentials (Org ID or API token) passed in.")
    success()

    """
    Validate that the device ID has been registered
    """
    info("Validating that the given device is registered")
    device = hubblenetwork.Device(id=device_id)
    try:
        org.retrieve_packets(device)
    except hubblenetwork.errors.RequestError as e:
        error("Device ID not found in backend")
    success()

    """
    Validate that a Hubble device is advertising
    """
    timeout = 30
    info(f"Scanning for Hubble-compatible advertisers (timeout={timeout}s)")
    pkts = hubblenetwork.ble.scan(timeout=timeout)
    if len(pkts) == 0:
        error(
            'No Hubble advertisements found.\
            \n\nNOTE: This may be due to a slow advertising interval and BLE-scanning\
            \n      optimizations done by your operating system. Try running this\
            \n      script again if your advertising interval is slow.\
            \n\nOther debug tips:\
            \n 1. Ensure your advertising packet is constructed correctly with both\
            \n    the "Complete List of 16-bit Service UUIDs" advertising type (with\
            \n    the Hubble UUID) and "Service Data" type included.\
            \n 2. Ensure your device as advertising at all (if in doubt, try a BLE\
            \n    scanning app on your phone)\
            \n\nIf these do not resolve your issue please contact support@hubble.com.'
        )
    success()

    """
    Validating that the packet was encrypted correctly
    """
    info("Validating encryption of received packets")
    for pkt in pkts:
        decrypted_pkt = hubblenetwork.decrypt(decoded_key, pkt)
        if decrypted_pkt:
            pkt_to_ingest = pkt
            break
    if not decrypted_pkt:
        error(
            "Unable to decrypt packet with given device key.\
            \n\nDebug tips:\
            \n 1. Ensure you entered the key correctly when running this script.\
            \n 2. Check that your device is provisioned with this same key.\
            \n 3. Check that your device-level encryption is working.\
            \n\nIf these do not resolve your issue please contact support@hubble.com."
        )
    success()

    """
    Ingesting into the backend (not part of validation but necessary)
    """
    info("Ingesting packet into the backend")
    try:
        org.ingest_packet(pkt_to_ingest)
    except Exception as e:
        error("Unable to ingest packet on the backend (not your fault)")
    success()

    """
    Validate that the backend was able to parse your packet
    """
    info("Checking for packet in the backend")
    timestamp = pkt_to_ingest.timestamp
    for _ in range(10):
        time.sleep(1)
        backend_pkt = get_pkt_from_be_with_timestamp(org, device, timestamp)
        if backend_pkt:
            break
    if not backend_pkt:
        error("Unable to retrieve packet from the backend")
    success()

    click.secho(f"\n[COMPLETE] All validation steps passed!", fg="green", bold=True)
    click.secho("Packet metadata:")
    click.secho(f'\tname:     "{backend_pkt.device_name}"')
    click.secho(f'\tpayload:  "{backend_pkt.payload}"')
    click.secho(f"\tsequence: {backend_pkt.sequence}")


if __name__ == "__main__":
    validate()
