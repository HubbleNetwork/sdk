# python/src/hubble/cli.py
from __future__ import annotations

import click
import os
import json
import time
import base64
import sys
from datetime import timezone, datetime
from typing import Optional
from hubble import Organization
from hubble import flash_elf, fetch_elf, patch_elf
from hubble import Device, DecryptedPacket, EncryptedPacket
from hubble import ble as ble_mod
from hubble import decrypt


def _get_env_or_fail(name: str) -> str:
    val = os.getenv(name)
    if not val:
        raise click.ClickException(f"[ERROR] {name} environment variable not set")
    return val


def _get_org_and_token(org_id, token) -> tuple[str, str]:
    """
    Helper function that checks if the given token and/or org
    are None and gets the env var if not
    """
    if not token:
        token = _get_env_or_fail("HUBBLE_API_TOKEN")
    if not org_id:
        org_id = _get_env_or_fail("HUBBLE_ORG_ID")
    return org_id, token


def _print_packets_pretty(pkts) -> None:
    """Pretty-print an EncryptedPacket."""
    for pkt in pkts:
        ts = datetime.fromtimestamp(pkt.timestamp).strftime("%c")
        loc = pkt.location
        loc_str = (
            f"{loc.lat:.6f},{loc.lon:.6f}"
            if getattr(loc, "lat", None) is not None
            else "unknown"
        )
        click.echo(click.style("=== BLE packet ===", bold=True))
        click.echo(f"time:    {ts}")
        click.echo(f"rssi:    {pkt.rssi} dBm")
        click.echo(f"loc:     {loc_str}")
        # Show both hex and length
        if isinstance(pkt, DecryptedPacket):
            click.echo(f'payload: "{pkt.payload}"')
        elif isinstance(pkt, EncryptedPacket):
            click.echo(f"payload: {pkt.payload.hex()} ({len(pkt.payload)} bytes)")


def _print_packets_csv(pkts) -> None:
    for pkt in pkts:
        ts = datetime.fromtimestamp(pkt.timestamp).strftime("%c")
        if isinstance(pkt, DecryptedPacket):
            payload = pkt.payload
        elif isinstance(pkt, EncryptedPacket):
            payload = pkt.payload.hex()
        click.echo(f"{ts}, {pkt.location.lat:.6f}, {pkt.location.lon:.6f}, {payload}")


def _print_packets_kepler(pkts) -> None:
    """
    https://kepler.gl/demo

    Can ingest this JSON to visualize a travel path for a device.
    """
    data = {
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"vendor": "A"},
                "geometry": {"type": "LineString", "coordinates": []},
            }
        ],
    }

    for pkt in pkts:
        row = [pkt.location.lon, pkt.location.lat, 0, pkt.timestamp]
        data["features"][0]["geometry"]["coordinates"].append(row)
    click.echo(json.dumps(data))


def _print_packets(pkts, output: str = "pretty") -> None:
    func_name = f"_print_packets_{output.lower().strip()}"
    func = getattr(sys.modules[__name__], func_name, None)
    if callable(func):
        func(pkts)
    else:
        _print_packets_pretty(pkts)


def _print_device(dev: Device) -> None:
    click.echo(f'id: "{dev.id}", ', nl=False)
    click.echo(f'name: "{dev.name}", ', nl=False)
    click.echo(f"tags: {str(dev.tags)}, ", nl=False)
    ts = datetime.fromtimestamp(dev.created_ts).strftime("%c")
    click.echo(f'created: "{ts}", ', nl=False)
    click.echo(f"active: {str(dev.active)}", nl=False)
    if dev.key:
        click.secho(f', key: "{dev.key}"')
    else:
        click.echo("")


@click.group(context_settings={"help_option_names": ["-h", "--help"]})
def cli() -> None:
    """Hubble SDK CLI."""
    # top-level group; subcommands are added below


@cli.group()
def ble() -> None:
    """BLE utilities."""
    # subgroup for BLE-related commands


@ble.command("scan")
@click.option(
    "--timeout",
    "-t",
    type=int,
    default=5,
    show_default=False,
    help="Timeout when scanning",
)
@click.option(
    "--key",
    "-k",
    type=str,
    default=None,
    show_default=False,
    help="Attempt to decrypt any received packet with the given key",
)
@click.option("--ingest", is_flag=True)
def ble_scan(timeout, ingest: bool = False, key: str = None) -> None:
    """
    Scan for UUID 0xFCA6 and print the first packet found within TIMEOUT seconds.

    Example:
      hubble ble scan 1
    """
    click.secho(
        f"[INFO] Scanning for Hubble devices (timeout={timeout}s)... ", nl=False
    )
    pkts = ble_mod.scan(timeout=timeout)
    if len(pkts) == 0:
        click.secho(f"[WARNING] No packet found within {timeout:.2f}s", fg="yellow")
        raise SystemExit(1)
    click.echo("[COMPLETE]")

    click.echo("\n[INFO] Encrypted packets received:")
    _print_packets(pkts)

    # If we have a key, attempt to decrypt
    if key:
        key = bytearray(base64.b64decode(key))
        decrypted_pkts = []
        for pkt in pkts:
            decrypted_pkt = decrypt(key, pkt)
            if decrypted_pkt:
                decrypted_pkts.append(decrypted_pkt)
        if len(decrypted_pkts) > 0:
            click.echo("\n[INFO] Locally decrypted packets:")
            _print_packets(decrypted_pkts)
        else:
            click.secho("\n[WARNING] No locally decryptable packets found", fg="yellow")

    if ingest:
        click.echo("[INFO] Ingesting packet(s) into the backend... ", nl=False)
        org = Organization(
            org_id=_get_env_or_fail("HUBBLE_ORG_ID"),
            api_token=_get_env_or_fail("HUBBLE_API_TOKEN"),
        )
        for pkt in pkts:
            org.ingest_packet(pkt)
        click.echo("[SUCCESS]")


@cli.group()
def org() -> None:
    """Organization utilities."""
    # subgroup for organization-related commands


@click.option(
    "--org-id",
    "-o",
    type=str,
    default=None,
    show_default=False,
    help="Organization ID (if not using HUBBLE_ORG_ID env var)",
)
@click.option(
    "--token",
    "-t",
    type=str,
    default=None,
    show_default=False,  # show default in --help
    help="Token (if not using HUBBLE_API_TOKEN env var)",
)
@org.command("list-devices")
def list_devices(org_id, token) -> None:
    org_id, token = _get_org_and_token(org_id, token)

    org = Organization(org_id=org_id, api_token=token)
    devices = org.list_devices()
    for device in devices:
        _print_device(device)


@click.option(
    "--org-id",
    "-o",
    type=str,
    default=None,
    show_default=False,
    help="Organization ID (if not using HUBBLE_ORG_ID env var)",
)
@click.option(
    "--token",
    "-t",
    type=str,
    default=None,
    show_default=False,  # show default in --help
    help="Token (if not using HUBBLE_API_TOKEN env var)",
)
@org.command("register_device")
def register_device(org_id, token) -> None:
    org_id, token = _get_org_and_token(org_id, token)

    org = Organization(org_id=org_id, api_token=token)
    click.secho(str(org.register_device()))


@org.command("get-packets")
@click.argument("device-id", type=str)
@click.option(
    "--org-id",
    "-o",
    type=str,
    default=None,
    show_default=False,
    help="Organization ID (if not using HUBBLE_ORG_ID env var)",
)
@click.option(
    "--token",
    "-t",
    type=str,
    default=None,
    show_default=False,  # show default in --help
    help="Token (if not using HUBBLE_API_TOKEN env var)",
)
@click.option(
    "--output",
    type=str,
    default=None,
    show_default=False,  # show default in --help
    help="Output format (None, pretty, csv)",
)
@click.option(
    "--days",
    "-d",
    type=int,
    default=7,
    show_default=False,  # show default in --help
    help="Number of days to query back (from now)",
)
def get_packets(device_id, org_id, token, output: str = None, days: int = 7) -> None:
    org_id, token = _get_org_and_token(org_id, token)

    org = Organization(org_id=org_id, api_token=token)
    device = Device(id=device_id)
    packets = org.retrieve_packets(device, days=days)
    _print_packets(packets, output)


@cli.group()
def demo() -> None:
    """Demo functionality"""


@demo.command("flash")
@click.argument("board", type=str)
@click.option(
    "--org-id",
    "-o",
    type=str,
    default=None,
    show_default=False,
    help="Organization ID (if not using HUBBLE_ORG_ID env var)",
)
@click.option(
    "--token",
    "-t",
    type=str,
    default=None,
    show_default=False,  # show default in --help
    help="Token (if not using HUBBLE_API_TOKEN env var)",
)
def flash(board: str, name: str = None, org_id: str = None, token: str = None) -> None:
    org_id, token = _get_org_and_token(org_id, token)
    org = Organization(org_id=org_id, api_token=token)

    click.secho(f'Registering new device with name "{name}"... ', nl=False)
    device = org.register_device(name=name)
    click.secho("[SUCCESS]")
    click.secho(f"\tDevice ID:  {device.id}")
    click.secho(f"\tDevice Key: {device.key}")

    click.secho(f"Retrieving binary for {board}... ", nl=False)
    buf = fetch_elf(board=board)
    click.secho("[SUCCESS]")

    click.secho("Patching key + UTC into binary... ", nl=False)
    patch_elf(buf, device)
    click.secho("[SUCCESS]")

    click.secho("Flashing binary onto device... ", nl=False)
    flash_elf(board=board, buf=buf)
    click.secho("[SUCCESS]")

    click.secho(f"{board} successfully flashed and provisioned!")


def main(argv: Optional[list[str]] = None) -> int:
    """
    Entry point used by console_scripts.

    Returns a process exit code instead of letting Click call sys.exit for easier testing.
    """
    try:
        # standalone_mode=False prevents Click from calling sys.exit itself.
        cli.main(args=argv, prog_name="hubble", standalone_mode=False)
    except SystemExit as e:
        return int(e.code)
    except Exception as e:  # safety net to avoid tracebacks in user CLI
        click.secho(f"Unexpected error: {e}", fg="red", err=True)
        return 2
    return 0


if __name__ == "__main__":
    # Forward command-line args (excluding the program name) to main()
    raise SystemExit(main())
