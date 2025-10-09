# Copyright (c) 2025 Hubble Network, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

target_sources(app PRIVATE src/main.c)
target_sources_ifdef(CONFIG_HUBBLE_BEACON_SAMPLE_STORE_NONCE_SEQUENCE app PRIVATE src/store.c)
