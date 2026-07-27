/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"

#include "mbedtls/base64.h"

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>

#define SAT_TX_SLEEP_MS 2000

static const char *APP_TAG = "sat_continuous";
static uint8_t _hubble_key[CONFIG_HUBBLE_KEY_SIZE];

// Conditional external antenna selection
static void select_external_antenna(void)
{
#ifdef CONFIG_EXTERNAL_ANTENNA
    // GPIO3: RF switch power enable (active LOW)
    gpio_config_t io_conf3 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_3),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf3));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_3, 0));  // LOW = power ON

    // GPIO14: Antenna select (HIGH = external)
    gpio_config_t io_conf14 = {
        .pin_bit_mask = (1ULL << GPIO_NUM_14),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf14));
    ESP_ERROR_CHECK(gpio_set_level(GPIO_NUM_14, 1));  // HIGH = external antenna

    ESP_LOGI(APP_TAG, "External antenna selected (GPIO3=LOW, GPIO14=HIGH)");
#else
    ESP_LOGI(APP_TAG, "Using default (onboard) antenna");
#endif
}

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
	// Select external antenna
	select_external_antenna();

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

		err = hubble_sat_packet_send(&pkt, HUBBLE_SAT_RELIABILITY_NORMAL);
		if (err != 0) {
			ESP_LOGE(APP_TAG,
				 "Failed to transmit packet, error: %d", err);
			return;
		}

		vTaskDelay(pdMS_TO_TICKS(SAT_TX_SLEEP_MS));
	}
}
