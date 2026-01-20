/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/random/random.h>

#include <hubble/sat.h>
#include <hubble/port/sat_radio.h>
#include <hubble/port/sys.h>

#include "sat_board.h"

K_SEM_DEFINE(_trans_sem, 1, 1);

static inline int16_t _time_offset_get_ms(void)
{
	/* Rand is anything in [0-255] (uint8_t). Let's split it into
	 * few offsets in a 2s range.
	 */
	int16_t offset_values[] = {-1000, -500, 0, 500, 1000};
	uint8_t rand_value;

	sys_rand_get(&rand_value, sizeof(rand_value));

	return offset_values[rand_value / 52];
}

int hubble_sat_port_packet_send(uint8_t channel,
				const struct hubble_sat_packet *packet,
				uint8_t retries, uint8_t interval_s)
{
	int ret;

	/* Should we add a parameter in the API instead of K_FOREVER ? */
	k_sem_take(&_trans_sem, K_FOREVER);

	ret = hubble_sat_board_enable();
	if (ret != 0) {
		goto enable_error;
	}

	while (retries-- > 0) {
		ret = hubble_sat_board_packet_send(channel, packet);
		if (ret != 0) {
			goto end;
		}

		if (retries > 0) {
			uint32_t sleep_ms = MAX(0, (interval_s * MSEC_PER_SEC) +
							   _time_offset_get_ms());
			k_sleep(K_MSEC(sleep_ms));
		}
	}

end:
	/* Let's preserve a possible earlier error */
	if (ret == 0) {
		ret = hubble_sat_board_disable();
	} else {
		(void)hubble_sat_board_disable();
	}

enable_error:
	k_sem_give(&_trans_sem);

	return ret;
}

int hubble_sat_port_init(void)
{
	return hubble_sat_board_init();
}
