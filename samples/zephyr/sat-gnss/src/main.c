/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <hubble/sat/ephemeris.h>

#include "gnss_manager.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define FIX_MARGIN_S        30U
#define TRANSMIT_DURATION_S 20U
#define LOCATION_DELTA      0.01 /* ~1 km */

enum app_state {
	GET_FIX,
	PREDICT_PASS,
	WAIT_FOR_MARGIN,
	PREPARE_TRANSMIT,
	TRANSMIT
};

static enum app_state current_state = GET_FIX;
static struct gnss_fix current_fix;

static const struct orbit_info orbit = {
	.t0 = 1711296587,
	.n0 = 0.00017559780215620866,     /* orbital frequency in orbits/sec */
	.ndot = 3.6984685877857914e-14,
	.raan0 = -2.62346138227064,
	.raandot = 1.992330418167161e-07, /* approximation */
	.aop0 = 3.523598389978097,
	.aopdot = -6.981828658074634e-07, /* approximation */
	.inclination = 97.4608,
	.eccentricity = 0.0010652};

K_SEM_DEFINE(transmit_sem, 0, 1);

static void timer_cb(struct k_timer *timer)
{
	k_sem_give(&transmit_sem);
}

K_TIMER_DEFINE(transmit_timer, timer_cb, NULL);

static uint64_t compute_wait_time(uint64_t next_pass_time)
{
	uint64_t wait_time, current_time;

	current_time = (k_uptime_get() - current_fix.uptime_ms) / 1000 +
		       current_fix.utc;
	wait_time = next_pass_time - current_time;

	if (wait_time < FIX_MARGIN_S) {
		wait_time = 0;
	} else {
		wait_time -= FIX_MARGIN_S;
	}

	return wait_time;
}

int main(void)
{
	int err;
	uint64_t wait_time;
	struct gnss_fix fix;
	struct ground_info ground;
	struct hubble_pass_info next_pass;

	err = gnss_init();
	if (err != 0) {
		LOG_ERR("GNSS initialization failed (err %d)", err);
		return err;
	}

	while (true) {
		switch (current_state) {
		case GET_FIX:
			err = gnss_get_fix(WAIT_FOREVER, &current_fix);
			if (err != 0) {
				LOG_ERR("Failed to get GNSS fix (err %d)", err);
				return err;
			}

			LOG_DBG("GNSS Fix acquired: Lat: %.6f Lon: %.6f, "
				"UTC(s): %llu",
				current_fix.latitude, current_fix.longitude,
				current_fix.utc);

			current_state = PREDICT_PASS;
			break;

		case PREDICT_PASS:
			ground.lat = current_fix.latitude;
			ground.lon = current_fix.longitude;

			err = hubble_next_pass_get(&orbit, current_fix.utc,
						   &ground, &next_pass);
			if (err != 0) {
				LOG_ERR("Failed to get next pass info (err %d)",
					err);
				return err;
			}

			LOG_DBG("Next pass at %llu (lon %.6f, %s)", next_pass.t,
				next_pass.lon,
				next_pass.ascending ? "ascending" : "descending");

			current_state = WAIT_FOR_MARGIN;
			break;

		case WAIT_FOR_MARGIN:
			/* Get current time */
			wait_time = compute_wait_time(next_pass.t);

			if (wait_time != 0) {
				k_timer_start(&transmit_timer,
					      K_SECONDS(wait_time), K_NO_WAIT);
				k_sem_take(&transmit_sem, K_FOREVER);
			}

			current_state = PREPARE_TRANSMIT;
			break;

		case PREPARE_TRANSMIT:
			/* Try to get another fix */
			err = gnss_get_fix(FIX_MARGIN_S, &fix);
			if (err != 0) {
				LOG_WRN("Failed to get GNSS fix before "
					"transmit (err %d)",
					err);

				/* Proceed with last known fix */
				current_state = TRANSMIT;
				break;
			}

			/* Compare if we still in the same location */
			if (fabs(fix.latitude - current_fix.latitude) >
				    LOCATION_DELTA ||
			    fabs(fix.longitude - current_fix.longitude) >
				    LOCATION_DELTA) {
				LOG_INF("Location changed since last fix, "
					"recompute transmit");

				current_fix = fix;
				current_state = PREDICT_PASS;
				break;
			}

			current_state = TRANSMIT;
			break;

		case TRANSMIT:
			LOG_INF("Start transmit to satellite...");
			/* Transmit logic goes here */
			k_sleep(K_SECONDS(TRANSMIT_DURATION_S));
			LOG_INF("Stop transmit to satellite");
			current_state = GET_FIX;
			break;

		default:
			LOG_ERR("Unknown application state!");
			return -1;
		}
	}

	return 0;
}
