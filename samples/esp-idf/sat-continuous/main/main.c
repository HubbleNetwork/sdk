/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"

#include "mbedtls/base64.h"

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>

#define SAT_TX_SLEEP_MS 10000

static const char *APP_TAG = "sat_continuous";
static uint8_t _hubble_key[CONFIG_HUBBLE_KEY_SIZE];

void app_main(void)
{
	esp_err_t err = 0;
	struct hubble_sat_packet pkt;

	/* Decode device key */
	if (strlen(CONFIG_HUBBLE_DEVICE_KEY) != 0) {
		size_t outlen = 0;
		int err = mbedtls_base64_decode(
			_hubble_key, sizeof(_hubble_key), &outlen,
			(const unsigned char *)CONFIG_HUBBLE_DEVICE_KEY,
			strlen(CONFIG_HUBBLE_DEVICE_KEY));

		if (err != 0) {
			ESP_LOGE(APP_TAG, "Invalid key provided!");
			return;
		}

		ESP_LOGD(APP_TAG, "Device key decoded (%zu bytes):", outlen);
		ESP_LOG_BUFFER_HEX_LEVEL(APP_TAG, _hubble_key, outlen,
					 ESP_LOG_DEBUG);
	}

	err = hubble_init(0, _hubble_key);
	if (err != 0) {
		ESP_LOGE(APP_TAG,
			 "Failed to initialize Hubble Sat Network, error: %d",
			 err);
		return;
	}

	ESP_LOGI(APP_TAG, "Starting Sat Transmission");

	for (;;) {
		err = hubble_sat_packet_get(&pkt, NULL, 0);
		if (err != 0) {
			ESP_LOGE(APP_TAG, "Failed to get Hubble Sat Network packet, error: %d",
				 err);
			return;
		}

		/*
		 * Set reliability to NONE. This will trigger a single
		 * transmission instead of a sequence. This is only for testing
		 * purposes and not recommended for production.
		 */
		err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NONE);
		if (err != 0) {
			ESP_LOGE(APP_TAG,
				 "Failed to transmit packet, error: %d", err);
			return;
		}

		vTaskDelay(pdMS_TO_TICKS(SAT_TX_SLEEP_MS));
	}
}
