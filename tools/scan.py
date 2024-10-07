#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import asyncio
import base64
import signal

from datetime import datetime, timezone
from types import SimpleNamespace

from bleak import BleakScanner
from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from Crypto.Protocol.KDF import SP800_108_Counter

HUBBLE_AES_NONCE_SIZE = 12
HUBBLE_AES_TAG_SIZE = 4

# Valid values are 16 and 32, respectively for AES-128 and AES-256
hubble_aes_key_size = 32

def generate_kdf_key(key: bytes, key_size: int, label: str, context: int) -> bytes:
    label = label.encode()
    context = str(context).encode()

    return SP800_108_Counter(
        key,
        key_size,
        lambda session_key, data: CMAC.new(session_key, data, AES).digest(),
        label=label,
        context=context,
    )


def get_nonce(time_counter: int, counter: int) -> bytes:
    nonce_key = generate_kdf_key(
        args.master_key, hubble_aes_key_size, "NonceKey", time_counter
    )

    return generate_kdf_key(nonce_key, HUBBLE_AES_NONCE_SIZE, "Nonce", counter)


def get_encryption_key(time_counter: int, counter: int) -> bytes:
    encryption_key = generate_kdf_key(
        args.master_key, hubble_aes_key_size, "EncryptionKey", time_counter
    )

    return generate_kdf_key(encryption_key, hubble_aes_key_size, 'Key', counter)

def get_auth_tag(key: bytes, ciphertext: bytes) -> bytes:
    computed_cmac = CMAC.new(key, ciphertext, AES).digest()

    return computed_cmac[:HUBBLE_AES_TAG_SIZE]


def aes_decrypt(key: bytes, session_nonce: bytes, ciphertext: bytes) -> bytes:
    cipher = AES.new(key, AES.MODE_CTR, nonce=session_nonce)

    return cipher.decrypt(ciphertext)


def parse_ble_adv(ble_adv: bytes) -> SimpleNamespace:
    seq_no = int.from_bytes(ble_adv[0:2], "big") & 0x3FF
    device_id = ble_adv[2:6].hex()
    auth_tag = ble_adv[6:10]
    encrypted_payload = ble_adv[10:]
    day_offset = 0

    time_counter = int(datetime.now(timezone.utc).timestamp()) // 86400


    for t in range(-args.days, args.days + 1):
        key = get_encryption_key(time_counter + t, seq_no)
        tag = get_auth_tag(key, encrypted_payload)

        if tag == auth_tag:
            day_offset = t
            nonce = get_nonce(time_counter, seq_no)
            decrypted_payload = aes_decrypt(key, nonce, encrypted_payload)

            return SimpleNamespace(device=device_id, counter=seq_no, payload=decrypted_payload, day_offset=day_offset)

    return None


async def scan(stop_event: asyncio.Event):
    def callback(device, advertising_data):
        for uuid in advertising_data.service_uuids:
            if uuid.startswith("0000fca6"):
                data = parse_ble_adv(advertising_data.service_data[uuid])
                if data is None:
                    continue

                print("Device: {}".format(data.device))
                print("\tMAC address: {}".format(device.address))
                print("\tcounter: {}".format(data.counter))
                print("\tpayload (bytes): {}".format(data.payload.hex()))
                if data.day_offset != 0:
                    print("\tTHIS DEVICE IS OUT OF SYNC: {} days".format(data.day_offset))

    async with BleakScanner(callback) as _:
        await stop_event.wait()


def parse_args() -> None:
    """
    Scans for Hubble Network BLE device.

    This script will scan BLE devices and look for device
    advertisement data checking for Hubble Network UUID.
    When it finds one device it will try to decode the advertisement data
    using the given key.

    usage: scan.py [-h] [-b] key
    """

    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)

    parser.add_argument("master_key",
                        help="The device key")
    parser.add_argument("-b", "--base64",
                        help="The key is encoded in base64", action='store_true', default=False)
    parser.add_argument("-d", "--days", type=int,
                        help="The number (minus and plus) to use to decrypt. Useful for cheking if a device is out of sync",  default=4)
    args = parser.parse_args()


def main() -> None:
    parse_args()

    key = None
    global hubble_aes_key_size

    with open(args.master_key, "rb") as f:
        key = bytearray(f.read())
        if args.base64:
            key = bytearray(base64.b64decode(key))

    hubble_aes_key_size = len(key)
    if hubble_aes_key_size not in (16, 32):
        raise ValueError("The key must be either 16 or 32 bytes long")

    stop_event = asyncio.Event()
    args.master_key = key

    signal.signal(signal.SIGINT, lambda _, __: stop_event.set())

    asyncio.run(scan(stop_event))


if __name__ == '__main__':
    main()
