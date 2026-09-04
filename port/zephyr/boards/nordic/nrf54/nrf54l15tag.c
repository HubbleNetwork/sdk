/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>

#include <hubble/sat.h>
#include <hubble/sat/packet.h>
#include <sat_soc.h>

int hubble_sat_board_init(void)
{
	return 0;
}

int hubble_sat_board_enable(void)
{
	/* Set power, enable pa ... */
	return hubble_sat_soc_enable();
}

int hubble_sat_board_disable(void)
{
	return hubble_sat_soc_disable();
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	return hubble_sat_soc_packet_send(packet);
}
