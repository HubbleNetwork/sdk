/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>

#include <hubble/sat.h>
#include <hubble/port/sys.h>
#include <hubble/port/sat_radio.h>

/* TODO: Channel selection is going to be done at this layer.
 * This is not user visible.
 */
static uint8_t _channel = 0;

int hubble_sat_init(void)
{
	int ret = hubble_sat_port_init();

	if (ret != 0) {
		HUBBLE_LOG_ERROR(
			"Hubble Satellite Network initialization failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite Network initialized");

	return 0;
}

int hubble_sat_packet_send(const struct hubble_sat_packet *packet)
{
	int ret = hubble_sat_port_packet_send(_channel, packet);

	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}
