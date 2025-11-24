#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""
Script to summarize RAM and ROM consumption per testsuite from twister_footprint.json
"""

import json
import sys
import argparse
from pathlib import Path


TESTSUITE_COLUMN_SIZE = 45
TARGET_COLUMN_SIZE    = 25
RAM_COLUMN_SIZE       = 15
ROM_COLUMN_SIZE       = 15
TOTAL_COLUMN_SIZE     = TESTSUITE_COLUMN_SIZE + TARGET_COLUMN_SIZE + RAM_COLUMN_SIZE + ROM_COLUMN_SIZE


def format_size(size_bytes):
    """Format size in bytes to human-readable format."""
    if size_bytes is None or size_bytes < 0:
        return 'N/A'
    for unit in ['B', 'KB', 'MB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} GB"


def hubblenetwork_components_find(symbols_node, all_components):
    """Recursively traverse footprint tree to find hubblenetwork-sdk components."""
    if not symbols_node:
        return

    identifier = symbols_node.get('identifier', '')
    size = symbols_node.get('size', 0)
    children = symbols_node.get('children', [])

    # Check if this node is related to hubblenetwork-sdk library
    if 'sdk' in identifier:
        # Extract component path after hubblenetwork-sdk
        parts = identifier.split('sdk')
        if len(parts) > 1:
            component_path = parts[-1].strip('/')
            if component_path:
                path_parts = component_path.split('/')

                # Capture aggregate components:
                # - src (everything under src/)
                # - port/zephyr (everything under port/zephyr/)
                if path_parts and path_parts[0] == 'src' and len(path_parts) == 1:
                    # Top-level src directory - capture everything under src/
                    if size > 0:
                        all_components.append(('src', size, 1))
                elif (len(path_parts) == 2 and path_parts[0] == 'port' and
                      path_parts[1] == 'zephyr'):
                    # port/zephyr directory - capture everything under port/zephyr/
                    if size > 0:
                        all_components.append(('port/zephyr', size, 2))

    # Recursively process children
    for child in children:
        hubblenetwork_components_find(child, all_components)


def components_size_get(all_components):
    """Gets the size for each component, keeping maximum size if duplicates exist."""
    components_dict = {}

    for component_name, size, _ in all_components:
        if component_name not in components_dict:
            components_dict[component_name] = size
        else:
            # Keep the maximum size if component appears multiple times
            components_dict[component_name] = max(components_dict[component_name], size)

    return components_dict


def hubblenetwork_components_extract(footprint):
    """Extract hubblenetwork-sdk component sizes from footprint data."""
    all_components_rom = []
    all_components_ram = []

    # Extract ROM components
    if 'ROM' in footprint and 'symbols' in footprint['ROM']:
        hubblenetwork_components_find(footprint['ROM']['symbols'], all_components_rom)

    # Extract RAM components
    if 'RAM' in footprint and 'symbols' in footprint['RAM']:
        hubblenetwork_components_find(footprint['RAM']['symbols'], all_components_ram)

    # Get component sizes (handles duplicates by keeping maximum)
    components_rom = components_size_get(all_components_rom)
    components_ram = components_size_get(all_components_ram)

    return components_rom, components_ram


def footprint_data_extract(testsuite):
    """Extract ROM and RAM sizes from a testsuite entry."""
    name = testsuite.get('name', 'unknown')
    platform = testsuite.get('platform', 'unknown')

    rom_size = None
    ram_size = None

    footprint = testsuite.get('footprint', {})

    if 'ROM' in footprint and 'symbols' in footprint['ROM']:
        rom_size = footprint['ROM']['symbols'].get('size')

    if 'RAM' in footprint and 'symbols' in footprint['RAM']:
        ram_size = footprint['RAM']['symbols'].get('size')

    # Extract hubblenetwork-sdk components
    components_rom, components_ram = hubblenetwork_components_extract(footprint)

    return {
        'name': name,
        'platform': platform,
        'rom_size': rom_size,
        'ram_size': ram_size,
        'components_rom': components_rom,
        'components_ram': components_ram
    }


def format_csv(commit_date, commit_hash, footprint_data):
    """Output as CSV."""

    print("Testsuite,Target,ROM (bytes),RAM (bytes),ROM (formatted),RAM (formatted),Revision,Commit Date,SDK Components ROM,SDK Components RAM")
    for entry in footprint_data:
        rom_str = str(entry['rom_size']) if entry['rom_size'] is not None else 'N/A'
        ram_str = str(entry['ram_size']) if entry['ram_size'] is not None else 'N/A'
        rom_formatted = format_size(entry['rom_size'])
        ram_formatted = format_size(entry['ram_size'])

        # Format components
        if entry['components_rom']:
            components_rom_str = '; '.join([
                f"{k}:{format_size(v)}" for k, v in sorted(entry['components_rom'].items())
            ])
        else:
            components_rom_str = 'N/A'

        if entry['components_ram']:
            components_ram_str = '; '.join([
                f"{k}:{format_size(v)}" for k, v in sorted(entry['components_ram'].items())
            ])
        else:
            components_ram_str = 'N/A'

        print(f"{entry['name']},{entry['platform']},{rom_str},{ram_str},"
              f"{rom_formatted},{ram_formatted},{commit_hash},{commit_date},"
              f"\"{components_rom_str}\",\"{components_ram_str}\"")


def format_table(zephyr_version, commit_date, run_date, commit_hash, footprint_data):
    """Output as formatted table."""

    print("\nRAM and ROM Consumption Summary per Testsuite")
    print("="*TOTAL_COLUMN_SIZE)
    print(f"Revision: {commit_hash}")
    print(f"Zephyr Version: {zephyr_version}")
    print(f"Commit Date: {commit_date}")
    print(f"Run Date: {run_date}")
    print("="*TOTAL_COLUMN_SIZE)
    print(f"{'Testsuite':<{TESTSUITE_COLUMN_SIZE}} {'Target':<{TARGET_COLUMN_SIZE}}"
          f"{'ROM':<{ROM_COLUMN_SIZE}} {'RAM':<{RAM_COLUMN_SIZE}}")
    print("-"*TOTAL_COLUMN_SIZE)

    count_with_data = 0

    for entry in footprint_data:
        rom_str = format_size(entry['rom_size'])
        ram_str = format_size(entry['ram_size'])

        if entry['rom_size'] is not None or entry['ram_size'] is not None:
            count_with_data += 1
            print(f"{entry['name']:<{TESTSUITE_COLUMN_SIZE}}"
              f"{entry['platform']:<{TARGET_COLUMN_SIZE}} {rom_str:<{ROM_COLUMN_SIZE}}"
              f"{ram_str:<{RAM_COLUMN_SIZE}}")

        # Display hubblenetwork-sdk components if present
        if entry['components_rom'] or entry['components_ram']:
            print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'Hubblenetwork-SDK:':<{TARGET_COLUMN_SIZE}}")
            # Show ROM components
            if entry['components_rom']:
                print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'  ROM:':<{TARGET_COLUMN_SIZE}}")
                for comp_name, comp_size in sorted(entry['components_rom'].items()):
                    comp_label = f"    {comp_name}:"
                    print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {comp_label:<{TARGET_COLUMN_SIZE}} {format_size(comp_size)}")
            # Show RAM components
            if entry['components_ram']:
                print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'  RAM:':<{TARGET_COLUMN_SIZE}}")
                for comp_name, comp_size in sorted(entry['components_ram'].items()):
                    comp_label = f"    {comp_name}:"
                    print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {comp_label:<{TARGET_COLUMN_SIZE}} {format_size(comp_size)}")

    print("-"*TOTAL_COLUMN_SIZE)
    print(f"\nSummary: {len(footprint_data)} testsuites processed, {count_with_data} with footprint data")
    print("="*TOTAL_COLUMN_SIZE)


def footprint_summarize(json_file, output_format='table'):
    """Summarize RAM and ROM consumption from twister_footprint.json."""

    json_path = Path(json_file)
    if not json_path.exists():
        print(f"Error: File not found: {json_file}", file=sys.stderr)
        return 1

    try:
        with open(json_path, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: Invalid JSON file: {e}", file=sys.stderr)
        return 1
    except Exception as e:
        print(f"Error reading file: {e}", file=sys.stderr)
        return 1

    testsuites = data.get('testsuites', [])
    if not testsuites:
        print("Warning: No testsuites found in the file", file=sys.stderr)
        return 0

    # Extract revision information from environment
    environment = data.get('environment', {})
    zephyr_version = environment.get('zephyr_version', 'unknown')
    commit_date = environment.get('commit_date', 'unknown')
    run_date = environment.get('run_date', 'unknown')

    # Extract commit hash from zephyr_version if present (format: v4.2.0-3768-g1dcbee9decaa)
    commit_hash = 'unknown'
    if zephyr_version and 'g' in zephyr_version:
        parts = zephyr_version.split('-g')
        if len(parts) > 1:
            commit_hash = parts[-1]

    # Extract footprint data for each testsuite
    footprint_data = []
    for testsuite in testsuites:
        footprint_info = footprint_data_extract(testsuite)
        footprint_data.append(footprint_info)

    # Sort by ROM size (descending), then by RAM size
    footprint_data.sort(key=lambda x: (x['rom_size'] or 0, x['ram_size'] or 0), reverse=True)

    if output_format == 'csv':
        format_csv(commit_date, commit_hash, footprint_data)
    else:
        format_table(zephyr_version, commit_date, run_date, commit_hash, footprint_data)

    return 0


def main():
    parser = argparse.ArgumentParser(
        description='Summarize RAM and ROM consumption per testsuite from twister_footprint.json'
    )
    parser.add_argument(
        'json_file',
        nargs='?',
        default='twister-out/twister_footprint.json',
        help='Path to twister_footprint.json file (default: twister-out/twister_footprint.json)'
    )
    parser.add_argument(
        '--format',
        choices=['table', 'csv'],
        default='table',
        help='Output format: table (default) or csv'
    )

    args = parser.parse_args()
    return footprint_summarize(args.json_file, args.format)


if __name__ == '__main__':
    sys.exit(main())
