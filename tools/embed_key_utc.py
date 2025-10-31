#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

import argparse
import base64
import time


KEY_TEMPLATE = """
/*
 * This file contents was automatically generated.
 */
#define HUBBLE_KEY_SET 1

static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE] = {key};
"""

UTC_TEMPLATE = """
/*
 * This file contents was automatically generated.
 */
#define HUBBLE_UTC_SET 1

static uint64_t utc_time = {utc};
"""

def provision_data(key: str, encoded: bool, path: str, dry: bool) -> None:
    with open(key, "rb") as f:
        key_data = bytearray(f.read())
        if encoded:
            key_data = bytearray(base64.b64decode(key_data))

    key_hex = "{" +", ".join([hex(x) for x in key_data]) + "}"
    utc_ms =  str(int(time.time() * 1000))

    if dry:
        print(f"static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE] = {key_hex}")
        print(f"static uint64_t utc_time = {utc_ms}")
        return

    with open(path + "/key.c", "w") as f:
        f.write(KEY_TEMPLATE.format(key=key_hex))

    with open(path + "/utc.c", "w") as f:
        f.write(UTC_TEMPLATE.format(utc=utc_ms))


def parse_args() -> None:
    """
    Embed key & utc into the fw.

    This is a simple script to provision a key and utc into a device
    for test purpose.

    usage: provisioning-key.py [-h] [-b] key
    """

    global args

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter, allow_abbrev=False)

    parser.add_argument("key",
                        help="The key to provision")
    parser.add_argument("-b", "--base64",
                        help="The key is encoded in base64", action='store_true', default=False)
    parser.add_argument("-o", "--output-dir",
                        help="Path where utc and key will be generated", default=".")
    parser.add_argument("-d", "--dry-run",
                        help="Just print the data into console", action='store_true', default=False)
    args = parser.parse_args()


def main():
    parse_args()

    provision_data(args.key, args.base64, args.output_dir, args.dry_run)


if __name__ == '__main__':
    main()
