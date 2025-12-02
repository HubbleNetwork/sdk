# Copyright (c) 2024 Hubble Network, Inc.
#
# SPDX-License-Identifier: Apache-2.0


THIS_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Define base sources
HUBBLENETWORK_SDK_PORT_DIR = $(THIS_DIR)
HUBBLENETWORK_SDK_SRC_DIR = $(THIS_DIR)/../../src
HUBBLENETWORK_SDK_INCLUDE_DIR = $(THIS_DIR)/../../include

HUBBLENETWORK_SDK_BLE_FLAGS = \
	-I$(HUBBLENETWORK_SDK_INCLUDE_DIR) \
	-imacros $(HUBBLENETWORK_SDK_PORT_DIR)/config.h

HUBBLENETWORK_SDK_BLE_SOURCES = \
	$(HUBBLENETWORK_SDK_PORT_DIR)/hubble_freertos.c \
	$(HUBBLENETWORK_SDK_SRC_DIR)/hubble.c \
	$(HUBBLENETWORK_SDK_SRC_DIR)/hubble_ble.c
