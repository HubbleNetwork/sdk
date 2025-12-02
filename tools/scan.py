#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import click
import os
import base64
from datetime import datetime
from typing import Optional
from hubblenetwork import Organization, decrypt
from hubblenetwork import ble as ble_mod


def _print_packet_table_header() -> None:
    click.echo("\nTIME                      RSSI PAYLOAD (B)")
    click.echo("--------------------------------------------------------------")


def _print_packet_table_row(pkt) -> None:
    ts = datetime.fromtimestamp(pkt.timestamp).strftime("%c")

    click.echo(f"{ts}  {pkt.rssi}  ", nl=False)
    click.echo(f"{pkt.payload.hex()} ({len(pkt.payload)} bytes)")


def _get_env_or_fail(name: str) -> str:
    val = os.getenv(name)
    if not val:
        raise click.ClickException(f"[ERROR] {name} environment variable not set")
    return val


@click.command()
@click.option(
    "--key",
    "-k",
    type=str,
    default=None,
    show_default=False,
    help="Attempt to decrypt any received packet with the given key",
)
@click.option("--ingest", "-i", is_flag=True)
def scan(ingest: bool = False, key: str = None) -> None:
    """
    Scan for UUID 0xFCA6 and print packets received.
    """
    click.secho("[INFO] Scanning for Hubble devices...")
    _print_packet_table_header()

    if ingest:
        org = Organization(
            org_id=_get_env_or_fail("HUBBLE_ORG_ID"),
            api_token=_get_env_or_fail("HUBBLE_API_TOKEN"),
        )

    while True:
        pkt = ble_mod.scan_single(timeout=None)
        if not pkt:
            break

        # If we have a key, attempt to decrypt
        if key:
            decoded_key = bytearray(base64.b64decode(key))
            decrypted_pkt = decrypt(decoded_key, pkt)
            if decrypted_pkt:
                _print_packet_table_row(decrypted_pkt)
                # We only allow ingestion of packets you know the key of
                # so we don't ingest bogus data in the backend
                if ingest:
                    org.ingest_packet(pkt)
        else:
            _print_packet_table_row(pkt)


if __name__ == "__main__":
    # This makes `python -m scan.y -k 123` work
    scan()
