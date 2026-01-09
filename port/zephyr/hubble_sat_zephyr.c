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

#include "sat_board.h"

int hubble_sat_port_packet_send(uint8_t channel,
				const struct hubble_sat_packet *packet)
{
	int ret;

	ret = hubble_sat_board_enable();
	if (ret != 0) {
		return ret;
	}

	/* TODO: Handle retransmissions here */
	ret = hubble_sat_board_packet_send(channel, packet);
	if (ret != 0) {
		hubble_sat_board_disable();
		return ret;
	}

	return hubble_sat_board_disable();
}

int hubble_sat_port_init(void)
{
	return hubble_sat_board_init();
}
