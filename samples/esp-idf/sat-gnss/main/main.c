/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <hubble/sat/ephemeris.h>

#include "gnss_manager.h"

#define FIX_MARGIN_S        30U
#define TRANSMIT_DURATION_S 20U
#define LOCATION_DELTA      0.01 /* ~1 km */

static const char *DEMO_TAG = "main";

enum app_state {
	GET_FIX,
	PREDICT_PASS,
	WAIT_FOR_MARGIN,
	PREPARE_TRANSMIT,
	TRANSMIT
};

static enum app_state current_state = GET_FIX;
static struct gnss_fix current_fix;
static SemaphoreHandle_t transmit_sem;

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

static void timer_cb(void *arg)
{
	(void)xSemaphoreGive(transmit_sem);
}

static uint64_t compute_wait_time(uint64_t next_pass_time)
{
	uint64_t wait_time, current_time;

	current_time = ((uint64_t)esp_timer_get_time() - current_fix.uptime_us) /
			       1000000 +
		       current_fix.utc;
	wait_time = next_pass_time - current_time;

	if (wait_time < FIX_MARGIN_S) {
		wait_time = 0;
	} else {
		wait_time -= FIX_MARGIN_S;
	}

	return wait_time;
}

void app_main(void)
{
	int err;
	uint64_t wait_time;
	struct gnss_fix fix;
	struct ground_info ground;
	struct hubble_pass_info next_pass;
	esp_timer_handle_t transmit_timer;

	transmit_sem = xSemaphoreCreateBinary();
	if (transmit_sem == NULL) {
		ESP_LOGE(DEMO_TAG, "Failed to create transmit semaphore");
		goto end;
	}
	xSemaphoreTake(transmit_sem, 0);

	err = esp_timer_create(&(esp_timer_create_args_t){.callback = &timer_cb},
			       &transmit_timer);
	if (err != ESP_OK) {
		ESP_LOGE(DEMO_TAG, "Failed to create transmit timer (err %d)",
			 err);
		goto end;
	}

	err = gnss_init();
	if (err != 0) {
		ESP_LOGE(DEMO_TAG, "GNSS initialization failed (err %d)", err);
		goto end;
	}

	while (true) {
		switch (current_state) {
		case GET_FIX:
			err = gnss_get_fix(WAIT_FOREVER, &current_fix);
			if (err != 0) {
				ESP_LOGE(DEMO_TAG,
					 "Failed to get GNSS fix (err %d)", err);
				goto end;
			}

			ESP_LOGD(DEMO_TAG, "GNSS Fix acquired: Lat: %.6f Lon: %.6f, UTC(s): %llu",
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
				ESP_LOGE(
					DEMO_TAG,
					"Failed to get next pass info (err %d)",
					err);
				goto end;
			}

			ESP_LOGD(DEMO_TAG, "Next pass at %llu (lon %.6f, %s)",
				 next_pass.t, next_pass.lon,
				 next_pass.ascending ? "ascending"
						     : "descending");

			current_state = WAIT_FOR_MARGIN;
			break;

		case WAIT_FOR_MARGIN:
			/* Get current time */
			wait_time = compute_wait_time(next_pass.t);

			if (wait_time != 0) {
				esp_timer_start_once(transmit_timer,
						     wait_time * 1000000LL);
				xSemaphoreTake(transmit_sem, portMAX_DELAY);
			}

			current_state = PREPARE_TRANSMIT;
			break;

		case PREPARE_TRANSMIT:
			/* Try to get another fix */
			err = gnss_get_fix(FIX_MARGIN_S, &fix);
			if (err != 0) {
				ESP_LOGW(DEMO_TAG, "Failed to get GNSS fix before transmit (err %d)",
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
				ESP_LOGI(DEMO_TAG,
					 "Location changed since last fix, "
					 "recompute transmit");

				current_fix = fix;
				current_state = PREDICT_PASS;
				break;
			}

			current_state = TRANSMIT;
			break;

		case TRANSMIT:
			ESP_LOGI(DEMO_TAG, "Start transmit to satellite...");
			/* Transmit logic goes here */
			vTaskDelay(
				TRANSMIT_DURATION_S * 1000 / portTICK_PERIOD_MS);
			ESP_LOGI(DEMO_TAG, "Stop transmit to satellite");
			current_state = GET_FIX;
			break;

		default:
			ESP_LOGE(DEMO_TAG, "Unknown application state!");
			goto end;
		}
	}

end:
	esp_timer_delete(transmit_timer);
	vSemaphoreDelete(transmit_sem);
	return;
}
