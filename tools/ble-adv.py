#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0


"""
Simple application to advertise data through Hubble BLE Network.
"""

import argparse
import base64

from bitstring import BitArray
from bluezero import broadcaster
from datetime import datetime

from Crypto.Cipher import AES
from Crypto.Hash import CMAC
from Crypto.Protocol.KDF import SP800_108_Counter

HUBBLE_AES_KEY_SIZE = 32
HUBBLE_AES_NONCE_SIZE = 12
HUBBLE_DEVICE_ID_SIZE = 4
HUBBLE_AES_TAG_SIZE = 4


def generate_kdf_key(key: bytes, key_size: int, label: str,
                     context: int) -> bytes:
    label = label.encode()
    context = str(context).encode()

    return SP800_108_Counter(
        key,
        key_size,
        lambda session_key, data: CMAC.new(session_key, data, AES).digest(),
        label=label,
        context=context,
    )


def get_device_id(time_counter: int) -> int:
    device_key = generate_kdf_key(args.master_key, HUBBLE_AES_KEY_SIZE,
                                  'DeviceKey', time_counter)
    device_id = generate_kdf_key(device_key, HUBBLE_DEVICE_ID_SIZE,
                                 'DeviceID', 0)

    return int.from_bytes(device_id, byteorder='big')


def get_nonce(time_counter: int, counter: int) -> bytes:
    nonce_key = generate_kdf_key(
        args.master_key, HUBBLE_AES_KEY_SIZE, "NonceKey", time_counter
    )

    return generate_kdf_key(nonce_key, HUBBLE_AES_NONCE_SIZE, "Nonce", counter)


def get_encryption_key(time_counter: int, counter: int) -> bytes:
    encryption_key = generate_kdf_key(
        args.master_key, HUBBLE_AES_KEY_SIZE, "EncryptionKey", time_counter
    )

    return generate_kdf_key(encryption_key, HUBBLE_AES_KEY_SIZE,
                            'Key', counter)


def get_auth_tag(key: bytes, ciphertext: bytes) -> bytes:
    computed_cmac = CMAC.new(key, ciphertext, AES).digest()

    return computed_cmac[:HUBBLE_AES_TAG_SIZE]


def aes_encrypt(key: bytes, nonce_session: bytes, data: bytes) -> bytes:
    ciphertext = AES.new(key, AES.MODE_CTR, nonce=nonce_session).encrypt(data)
    tag = get_auth_tag(key, ciphertext)

    return ciphertext, tag


def parse_args() -> None:
    """
    Advertise data using Hubble BLE Network.

    usage: ble_adv.py [-h] [-b] key <optional-payload>
    """

    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False)

    parser.add_argument("master_key",
                        help="The device key")
    parser.add_argument("-b", "--base64",
                        help="The key is encoded in base64",
                        action='store_true', default=False)
    parser.add_argument("payload", nargs="?", default="",
                        help="Data to transmit")
    args = parser.parse_args()


def generate_ble_adv(device_id, seq_no, auth_tag, encrypted_payload) -> bytes:
    ble_adv = BitArray()

    if len(encrypted_payload) > 13:
        raise ValueError('Encrypted Payload is too long.')

    protocol_version = 0b000000

    ble_adv.append(f'uint:6={protocol_version}')
    ble_adv.append(f'uint:10={seq_no}')
    ble_adv.append(f'uint:32={device_id}')

    ble_adv.append(auth_tag)
    ble_adv.append(encrypted_payload)

    return ble_adv.tobytes()


def main() -> None:
    parse_args()

    key = None
    seq_no = 0

    with open(args.master_key, "rb") as f:
        key = bytearray(f.read())
        if args.base64:
            key = bytearray(base64.b64decode(key))

    args.master_key = key
    time_counter = int(datetime.now().timestamp()) // 86400

    device_id = get_device_id(time_counter)
    nonce = get_nonce(time_counter, seq_no)
    key = get_encryption_key(time_counter, seq_no)
    encrypted_payload, auth_tag = aes_encrypt(key, nonce,
                                              args.payload.encode())

    ble_adv = generate_ble_adv(device_id, seq_no, auth_tag, encrypted_payload)

    url_beacon = broadcaster.Beacon()
    url_beacon.add_service_data('FCA6', ble_adv)
    url_beacon.start_beacon()


if __name__ == '__main__':
    main()
