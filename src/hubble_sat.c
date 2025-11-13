/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdlib.h>

#include <hubble/sat.h>
#include <hubble/port/sys.h>
#include <hubble/port/sat_radio.h>

static const struct hubble_sat_api *sat_api;

int hubble_sat_init(void)
{
	sat_api = hubble_sat_api_get();

	if (sat_api == NULL || sat_api->transmit_packet == NULL) {
		return -ENOSYS;
	}

	HUBBLE_LOG_INFO("Hubble Satellite Network initialized\n");

	return 0;
}

int hubble_sat_enable(void)
{
	if (sat_api == NULL || sat_api->enable == NULL) {
		return -ENOSYS;
	}

	return sat_api->enable();
}

void hubble_sat_disable(void)
{
	if (sat_api == NULL || sat_api->disable == NULL) {
		return;
	}

	sat_api->disable();
}

int hubble_sat_transmit_packet(const struct hubble_sat_packet *packet)
{
	if (sat_api == NULL || sat_api->transmit_packet == NULL) {
		return -ENOSYS;
	}

	return sat_api->transmit_packet(packet);
}
