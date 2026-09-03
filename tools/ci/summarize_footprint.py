#!/usr/bin/env python3
#
# Copyright (c) 2025 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0

"""
Script to summarize RAM and ROM consumption per testsuite from `twister_out`.

Given a twister output dir, this script discovers if sysbuild was used
and produces per-image ROM/RAM JSON, then aggregates and prints a summary.
"""

import argparse
import datetime
import json
import os
import subprocess
import sys
from pathlib import Path

import yaml

TESTSUITE_COLUMN_SIZE = 45
TARGET_COLUMN_SIZE = 50
RAM_COLUMN_SIZE = 15
ROM_COLUMN_SIZE = 15
TOTAL_COLUMN_SIZE = TESTSUITE_COLUMN_SIZE + TARGET_COLUMN_SIZE + RAM_COLUMN_SIZE + ROM_COLUMN_SIZE


def format_size(size_bytes):
    """Format size in bytes to human-readable format."""
    if size_bytes is None or size_bytes < 0:
        return 'N/A'
    for unit in ['B', 'KB', 'MB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.2f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.2f} GB"


def zephyr_base_get():
    """Locate the Zephyr base directory."""
    # try to find the west dir first
    try:
        topdir = subprocess.run(
            ['west', 'topdir'],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        raise RuntimeError(f"failed to determine Zephyr topdir via 'west topdir': {exc}") from exc

    # check if zephyr dir exists
    zephyr_base = Path(topdir) / 'zephyr'
    if not zephyr_base.is_dir():
        raise RuntimeError(f"zephyr base not found at {zephyr_base}")

    return zephyr_base


def size_report_path_get(zephyr_base):
    """Return the path to the size_report tool, or raise if missing."""
    script = zephyr_base / 'scripts' / 'footprint' / 'size_report'
    if not script.is_file():
        raise RuntimeError(f"size_report tool not found at {script}")
    return script


def scenario_build_infos_find(twister_out):
    """Find all build info for each test scenario.

    Each test scenario has a build_info.yml in its build dir. For sysbuild, each
    image has its own build_info.yml as well, we just need the top-level info.
    """
    # collect all files that contains build_info.yml
    all_files = list(twister_out.rglob('build_info.yml'))

    # collect directories name that contains build_info.yml
    all_dirs = {f.parent.resolve() for f in all_files}

    # walk through each and skip any nested build_info.yml
    # by checking if its parent dir is in the all_dirs set
    scenario_files = []
    for f in all_files:
        parent = f.parent.resolve()
        nested = False
        for ancestor in parent.parents:
            if ancestor in all_dirs:
                nested = True
                break
            if ancestor == twister_out.resolve():
                break
        if not nested:
            scenario_files.append(f)
    return scenario_files


def build_info_parse(build_info_path):
    """Parse a scenario-level build_info.yml.

    Returns a dict. E.g.:
        {
          'platform': 'nrf54l15dk/nrf54l15/cpuapp',
          'sysbuild': True/False,
          'images': ['sat-dual-stack', ...], # sysbuild only, else []
        }
    """
    try:
        with open(build_info_path) as f:
            doc = yaml.safe_load(f) or {}
    except (OSError, yaml.YAMLError) as exc:
        raise RuntimeError(f"failed to read {build_info_path}: {exc}") from exc

    cmake = doc.get('cmake', {}) or {}
    board = cmake.get('board', {}) or {}
    name = board.get('name', '')
    qualifiers = board.get('qualifiers', '')
    revision = board.get('revision', '')
    if not name:
        raise RuntimeError(f"no board.name in {build_info_path}")

    platform = name
    if qualifiers:
        platform = f"{name}/{qualifiers}"
    if revision:
        platform = f"{platform}/{revision}"

    # build_info stores sysbuild as the string true / false
    sysbuild_raw = cmake.get('sysbuild', doc.get('sysbuild', False))
    sysbuild = str(sysbuild_raw).lower() == 'true'

    # find the list of images for sysbuild, if any
    images = []
    if sysbuild:
        for img in cmake.get('images', []) or []:
            img_name = img.get('name')
            if img_name:
                images.append(img_name)

    return {'platform': platform, 'sysbuild': sysbuild, 'images': images}


def size_report_run(size_report, zephyr_base, elf, target, json_out):
    """Run size_report (ROM/RAM) for one target and writing JSON to json_out"""
    txt_out = Path(json_out).with_suffix('.txt')
    cmd = [
        sys.executable,
        str(size_report),
        target,
        '-k',
        str(elf),
        '-z',
        str(zephyr_base),
        '-q',
        '-o',
        str(txt_out),
        '--json',
        str(json_out),
    ]
    try:
        subprocess.run(cmd, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(
            f"size_report {target} failed for {elf}:\n{exc.stderr or exc.stdout}"
        ) from exc

    # check if the JSON output was produced
    if not Path(json_out).is_file():
        raise RuntimeError(f"size_report {target} did not produce {json_out} for {elf}")


def size_report_load(json_path):
    """Load a size_report JSON, returning (symbols_tree, total_size)."""
    try:
        with open(json_path) as f:
            data = json.load(f)
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"failed to read {json_path}: {exc}") from exc

    return data.get('symbols'), data.get('total_size')


def hubblenetwork_components_find(symbols_node, all_components):
    """Recursively traverse footprint tree to find hubblenetwork-sdk components."""
    if not symbols_node:
        return

    identifier = symbols_node.get('identifier', '')
    size = symbols_node.get('size', 0)
    children = symbols_node.get('children', [])

    # Check if this node is related to hubblenetwork-sdk library
    # and has a non-zero size, if size 0, continue to look into children
    if 'sdk' in identifier and size > 0:
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
                    all_components.append(('src', size, 1))
                elif len(path_parts) == 2 and path_parts[0] == 'port' and path_parts[1] == 'zephyr':
                    # port/zephyr directory - capture everything under port/zephyr/
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


def _entry_build(
    scenario, platform, domain_label, scenario_dir, image_subdir, zephyr_base, size_report
):
    """Run size_report for one image and build a report entry.

    Args:
    scenario:       name of the test scenario
    platform:       target platform name
    domain_label:   '[name]' suffix for sysbuild images, or '' for non-sysbuild.
    scenario_dir:   path to the scenario directory
    image_subdir:   sub-path from the scenario dir to the image build dir
    zephyr_base:    path to the Zephyr base directory
    size_report:    path to the size_report tool

    Returns:
    A dict with the following keys:
        'name': scenario name
        'platform': target platform name (with domain label if sysbuild)
        'rom_size': total ROM size in bytes
        'ram_size': total RAM size in bytes
        'components_rom': dict of component names to ROM sizes
        'components_ram': dict of component names to RAM sizes
    """
    # find the ELF file for this image
    build_dir = scenario_dir / image_subdir if image_subdir else scenario_dir
    elf = build_dir / 'zephyr' / 'zephyr.elf'
    if not elf.is_file():
        target = f"domain '{image_subdir}' of " if image_subdir else ''
        raise ValueError(f"ELF not found for {target}'{scenario}': {elf}")

    # create output directory for footprint JSON files
    out_dir = build_dir / 'footprint'
    out_dir.mkdir(parents=True, exist_ok=True)
    rom_json = out_dir / 'rom.json'
    ram_json = out_dir / 'ram.json'

    # run size_report for ROM and RAM
    size_report_run(size_report, zephyr_base, elf, 'rom', rom_json)
    size_report_run(size_report, zephyr_base, elf, 'ram', ram_json)

    rom_symbols, rom_total = size_report_load(rom_json)
    ram_symbols, ram_total = size_report_load(ram_json)

    rom_components_raw = []
    ram_components_raw = []
    hubblenetwork_components_find(rom_symbols, rom_components_raw)
    hubblenetwork_components_find(ram_symbols, ram_components_raw)

    target = f"{platform} {domain_label}".rstrip() if domain_label else platform
    return {
        'name': scenario,
        'platform': target,
        'rom_size': rom_total,
        'ram_size': ram_total,
        'components_rom': components_size_get(rom_components_raw),
        'components_ram': components_size_get(ram_components_raw),
    }


def footprint_data_collect(twister_out, zephyr_base, size_report):
    """Discover scenarios via build_info.yml and collect footprint entries.

    For sysbuild type, label as '<platform> [<image>]' where <image> is
    the image name from build_info.yml.
    """
    twister_out = Path(twister_out)
    entries = []

    build_infos = scenario_build_infos_find(twister_out)
    if not build_infos:
        raise ValueError(f"no build_info.yml found under {twister_out} - was the build run?")

    # walk through all discovered build_info.yml files
    # and collect footprint data
    for build_info_path in build_infos:
        scenario_dir = build_info_path.parent
        scenario = scenario_dir.name
        info = build_info_parse(build_info_path)
        platform = info['platform']

        if info['sysbuild']:
            # if this is sysbuild but no images are listed, raise an error
            if not info['images']:
                raise RuntimeError(
                    f"sysbuild scenario '{scenario}' lists no images in {build_info_path}"
                )

            # for each image, run size_report and build an entry
            for image in info['images']:
                entries.append(
                    _entry_build(
                        scenario,
                        platform,
                        f"[{image}]",
                        scenario_dir,
                        image,
                        zephyr_base,
                        size_report,
                    )
                )
        else:
            # for non-sysbuild, just run size_report once for the scenario
            entries.append(
                _entry_build(
                    scenario,
                    platform,
                    '',
                    scenario_dir,
                    '',
                    zephyr_base,
                    size_report,
                )
            )

    return entries


def format_csv(commit_date, commit_hash, footprint_data):
    """Output as CSV."""

    print(
        "Testsuite,Target,ROM (bytes),RAM (bytes),ROM (formatted),RAM (formatted),"
        "Revision,Commit Date,SDK Components ROM,SDK Components RAM"
    )
    for entry in footprint_data:
        rom_str = str(entry['rom_size']) if entry['rom_size'] is not None else 'N/A'
        ram_str = str(entry['ram_size']) if entry['ram_size'] is not None else 'N/A'
        rom_formatted = format_size(entry['rom_size'])
        ram_formatted = format_size(entry['ram_size'])

        # Format components
        if entry['components_rom']:
            components_rom_str = '; '.join(
                [f"{k}:{format_size(v)}" for k, v in sorted(entry['components_rom'].items())]
            )
        else:
            components_rom_str = 'N/A'

        if entry['components_ram']:
            components_ram_str = '; '.join(
                [f"{k}:{format_size(v)}" for k, v in sorted(entry['components_ram'].items())]
            )
        else:
            components_ram_str = 'N/A'

        print(
            f"{entry['name']},{entry['platform']},{rom_str},{ram_str},"
            f"{rom_formatted},{ram_formatted},{commit_hash},{commit_date},"
            f"\"{components_rom_str}\",\"{components_ram_str}\""
        )


def format_table(zephyr_version, commit_date, run_date, commit_hash, footprint_data):
    """Output as formatted table."""

    print("\nRAM and ROM Consumption Summary per Testsuite")
    print("=" * TOTAL_COLUMN_SIZE)
    print(f"Revision: {commit_hash}")
    print(f"Zephyr Version: {zephyr_version}")
    print(f"Commit Date: {commit_date}")
    print(f"Run Date: {run_date}")
    print("=" * TOTAL_COLUMN_SIZE)
    print(
        f"{'Testsuite':<{TESTSUITE_COLUMN_SIZE}} {'Target':<{TARGET_COLUMN_SIZE}}"
        f"{'ROM':<{ROM_COLUMN_SIZE}} {'RAM':<{RAM_COLUMN_SIZE}}"
    )
    print("-" * TOTAL_COLUMN_SIZE)

    count_with_data = 0

    for entry in footprint_data:
        rom_str = format_size(entry['rom_size'])
        ram_str = format_size(entry['ram_size'])

        if entry['rom_size'] is not None or entry['ram_size'] is not None:
            count_with_data += 1
            print(
                f"{entry['name']:<{TESTSUITE_COLUMN_SIZE}}"
                f"{entry['platform']:<{TARGET_COLUMN_SIZE}} {rom_str:<{ROM_COLUMN_SIZE}}"
                f"{ram_str:<{RAM_COLUMN_SIZE}}"
            )

        # Display hubblenetwork-sdk components if present
        if entry['components_rom'] or entry['components_ram']:
            print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'Hubblenetwork-SDK:':<{TARGET_COLUMN_SIZE}}")
            # Show ROM components
            if entry['components_rom']:
                print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'  ROM:':<{TARGET_COLUMN_SIZE}}")
                for comp_name, comp_size in sorted(entry['components_rom'].items()):
                    comp_label = f"    {comp_name}:"
                    print(
                        f"{'':>{TESTSUITE_COLUMN_SIZE}} {comp_label:<{TARGET_COLUMN_SIZE}} "
                        f"{format_size(comp_size)}"
                    )
            # Show RAM components
            if entry['components_ram']:
                print(f"{'':>{TESTSUITE_COLUMN_SIZE}} {'  RAM:':<{TARGET_COLUMN_SIZE}}")
                for comp_name, comp_size in sorted(entry['components_ram'].items()):
                    comp_label = f"    {comp_name}:"
                    print(
                        f"{'':>{TESTSUITE_COLUMN_SIZE}} {comp_label:<{TARGET_COLUMN_SIZE}} "
                        f"{format_size(comp_size)}"
                    )

    print("-" * TOTAL_COLUMN_SIZE)
    print(
        f"\nSummary: {len(footprint_data)} testsuites processed, "
        f"{count_with_data} with footprint data"
    )
    print("=" * TOTAL_COLUMN_SIZE)


def footprint_summarize(twister_out, output_format='table'):
    """Summarize footprint data from twister output directory."""

    out_path = Path(twister_out)
    if not out_path.exists():
        print(f"Error: path not found: {twister_out}", file=sys.stderr)
        return 1

    # get the metadata
    zephyr_version = os.environ.get('ZEPHYR_VERSION', 'unknown')
    commit_date = os.environ.get('COMMIT_DATE', 'unknown')

    # Extract commit hash from zephyr_version if present (format: v4.2.0-3768-g1dcbee9decaa)
    commit_hash = 'unknown'
    if zephyr_version and 'g' in zephyr_version:
        parts = zephyr_version.split('-g')
        if len(parts) > 1:
            commit_hash = parts[-1]

    # Run date is now
    run_date = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')

    zephyr_base = zephyr_base_get()
    size_report = size_report_path_get(zephyr_base)

    # Extract footprint data for each testsuite
    footprint_data = footprint_data_collect(out_path, zephyr_base, size_report)

    # Sort by ROM size (descending), then by RAM size
    footprint_data.sort(key=lambda x: (x['rom_size'] or 0, x['ram_size'] or 0), reverse=True)

    if output_format == 'csv':
        format_csv(commit_date, commit_hash, footprint_data)
    else:
        format_table(zephyr_version, commit_date, run_date, commit_hash, footprint_data)
    return 0


def main():
    parser = argparse.ArgumentParser(
        description='Discover scenarios under a twister output tree, run '
        'size_report per image, and summarize ROM/RAM consumption'
    )
    parser.add_argument(
        'twister_out',
        nargs='?',
        default='twister-out',
        help='Path to the twister output directory (default: twister-out)',
    )
    parser.add_argument(
        '--format',
        choices=['table', 'csv'],
        default='table',
        help='Output format: table (default) or csv',
    )

    args = parser.parse_args()
    try:
        return footprint_summarize(args.twister_out, args.format)
    except Exception as exc:
        print(f"Error: {exc}", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
