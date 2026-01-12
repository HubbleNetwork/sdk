/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>

#include <hubble/sat.h>
#include <hubble/port/sys.h>
#include <hubble/port/sat_radio.h>

#define HUBBLE_SAT_CHANNEL_DEFAULT 5

int hubble_sat_packet_send(const struct hubble_sat_packet *packet)
{
	int ret;
	uint8_t channel;

	ret = hubble_rand_get(&channel, sizeof(channel));
	if (ret != 0) {
		HUBBLE_LOG_WARNING("Could not get a random channel, falling "
				   "back to default channel");
		channel = HUBBLE_SAT_CHANNEL_DEFAULT;
	} else {
		channel = channel % HUBBLE_SAT_NUM_CHANNELS;
	}

	ret = hubble_sat_port_packet_send(channel, packet);
	if (ret < 0) {
		HUBBLE_LOG_WARNING(
			"Hubble Satellite packet transmission failed");
		return ret;
	}

	HUBBLE_LOG_INFO("Hubble Satellite packet sent");

	return 0;
}
