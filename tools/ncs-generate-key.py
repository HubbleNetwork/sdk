#!/usr/bin/env python3
#
# Copyright (c) 2026 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""
Generate the PSA attributes of a Hubble key held in the CRACEN KMU.

The key is an AES-128 or AES-256 key used to sign with CMAC. It lives in a
KMU slot under the PROTECTED usage scheme, meaning CRACEN is the only one
that ever sees the key material.

The generated JSON is written to a file with --file, or to the console, and
is provisioned to the device with:

    nrfutil device x-provision-keys --key-file <file>
"""

import base64
import binascii
import json
import struct
from pathlib import Path

import click

# PSA values, see psa/crypto_values.h and nrf_security
KEY_TYPE_AES = 0x2400
ALG_CMAC = 0x03C00200
USAGE_SIGN = 0x1400  # PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_SIGN_HASH
LOCATION_CRACEN_KMU = 0x804E4B00
KMU_KEY_ID = 0x7FFF0000
KMU_SCHEME_PROTECTED = 0 << 12

# What happens to the slot once the key is destroyed
PERSISTENCE = {
    "DEFAULT": 1,  # the slot can be reused
    "REVOKABLE": 2,  # the slot is revoked
    "READ_ONLY": 3,  # the key can not be destroyed
}


def attributes(slot: int, key_bits: int, persistence: int) -> bytes:
    """Pack the key attributes as the psa_key_attributes_s C struct."""
    return struct.pack(
        "<hhIIIIII",
        KEY_TYPE_AES,
        key_bits,
        LOCATION_CRACEN_KMU | persistence,  # lifetime
        USAGE_SIGN,
        ALG_CMAC,
        0,  # no second algorithm
        KMU_KEY_ID | KMU_SCHEME_PROTECTED | slot,
        0,  # reserved, only used if the key id encodes an owner
    )


def key_material(key: str, key_bits: int) -> bytes:
    """Decode a base64 key and check that it has the expected size."""
    try:
        material = base64.b64decode(key.strip(), validate=True)
    except binascii.Error as error:
        raise click.BadParameter(f"not valid base64, {error}", param_hint="'--key'") from error

    if len(material) != key_bits // 8:
        raise click.BadParameter(
            f"a {key_bits} bits key must decode to {key_bits // 8} bytes, got {len(material)}",
            param_hint="'--key'",
        )

    return material


@click.command(context_settings={"help_option_names": ["-h", "--help"]})
@click.option("--id", "slot", type=click.IntRange(0, 255), required=True, help="KMU slot number")
@click.option("--key", required=True, help="Key material, base64 encoded")
@click.option(
    "--key-bits",
    type=click.Choice([128, 256]),
    default=256,
    show_default=True,
    help="AES key size in bits",
)
@click.option(
    "--persistence",
    type=click.Choice(PERSISTENCE),
    default="DEFAULT",
    show_default=True,
    help="Fate of the slot when the key is destroyed",
)
@click.option(
    "--file",
    "path",
    type=click.Path(dir_okay=False, path_type=Path),
    help="JSON file to write, created if missing and appended to otherwise",
)
def main(slot: int, key: str, key_bits: int, persistence: str, path: Path | None) -> None:
    """Generate the PSA attributes of a Hubble key to be stored in a CRACEN KMU slot.

    The output is a keyslot description that nrfutil provisions to the device with
    `nrfutil device x-provision-keys --key-file <file>`.
    """
    keyslot = {
        "metadata": "0x" + attributes(slot, key_bits, PERSISTENCE[persistence]).hex().upper(),
        "value": "0x" + key_material(key, key_bits).hex(),
    }

    if path and path.is_file():
        document = json.loads(path.read_text())
    else:
        document = {"version": 0, "keyslots": []}

    document["keyslots"].append(keyslot)

    output = json.dumps(document, indent=4)
    if path:
        path.write_text(output)
    else:
        click.echo(output)


if __name__ == "__main__":
    main()
