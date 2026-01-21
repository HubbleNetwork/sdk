# Copyright (c) 2025 Hubble Network, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

set(EXTRA_COMPONENT_DIRS ../../../port/esp-idf/)

set(COMPONENTS
        "main"
        "esp_timer"
        "esp_event"
        "esp_driver_uart"
        "hubblenetwork-sdk"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(sat-gnss LANGUAGES C)
