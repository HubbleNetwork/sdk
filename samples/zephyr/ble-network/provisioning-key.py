#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import base64
import sys
import time

import serial


SLEEP_TIME = 0.05


def provision_key(key: str, term: str, encoded: bool) -> None:
    with open(key, "rb") as f:
        key_data = bytearray(f.read())
        if encoded:
            key_data = bytearray(base64.b64decode(key_data))

    try:
        ser = serial.Serial(port=term, baudrate=115200, timeout=1)
    except serial.SerialException as e:
        print(f"{e}", file=sys.stderr)
        sys.exit(1)

    for x in key_data:
        ser.write(bytes(format(x, "02x"), "ascii"))
        time.sleep(SLEEP_TIME)


def parse_args() -> None:
    """
    Provisioning key to a device using serial port.

    The device must be listening on the serial port and be ready to receive
    the key.

    usage: provisioning-key.py [-h] [-b] key /path/to/serial
    """

    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)

    parser.add_argument("key",
                        help="The key to provision")
    parser.add_argument("serial",
                        help="The serial port connected to the device")
    parser.add_argument("-b", "--base64",
                        help="The key is encoded in base64", action='store_true', default=False)
    args = parser.parse_args()


def main():
    parse_args()

    provision_key(args.key, args.serial, args.base64)


if __name__ == '__main__':
    main()
