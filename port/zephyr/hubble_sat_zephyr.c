/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include <hubble/sat.h>
#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>
#include <hubble_rf_zephyr.h>

#define HUBBLE_WAIT_SYMBOL_US          8000U
#define HUBBLE_WAIT_PREAMBLE_US        1600U

#define HUBBLE_CHANNEL_OFFSET(channel) (channel * 66)

static uint8_t _channel = 0;
static const int8_t _preamble[] = {0, -1, 0, -1, 0, -1, 0, 0};

static int hubble_zephyr_transmit_packet(const struct hubble_sat_packet *packet)
{
	/* Send preamble */
	for (uint8_t i = 0; i < sizeof(_preamble); i++) {
		if (_preamble[i] == -1) {
			k_busy_wait(
				HUBBLE_WAIT_PREAMBLE_US + HUBBLE_WAIT_SYMBOL_US);
		} else {
			hubble_rf_frequency_step_set(
				HUBBLE_CHANNEL_OFFSET(_channel));
			hubble_rf_cw_start();
			k_busy_wait(HUBBLE_WAIT_SYMBOL_US);
			hubble_rf_cw_stop();
		}
	}

	/* Send payload */
	for (uint8_t i = 0; i < packet->length; i++) {
		hubble_rf_frequency_step_set(
			packet->data[i] + HUBBLE_CHANNEL_OFFSET(_channel));
		hubble_rf_cw_start();
		k_busy_wait(HUBBLE_WAIT_SYMBOL_US);
		hubble_rf_cw_stop();
		k_busy_wait(1800U);
	}

	return 0;
}

static int hubble_zephyr_sat_enable(void)
{
	return hubble_rf_enable();
}

static int hubble_zephyr_sat_disable(void)
{
	return hubble_rf_disable();
}

const struct hubble_sat_api *hubble_sat_api_get(void)
{
	static struct hubble_sat_api api = {
		.transmit_packet = hubble_zephyr_transmit_packet,
		.enable = hubble_zephyr_sat_enable,
		.disable = hubble_zephyr_sat_disable,
	};

	if (hubble_rf_init() != 0) {
		return NULL;
	}

	return &api;
}
