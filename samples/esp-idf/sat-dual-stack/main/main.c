/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"

/* FreeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ESP-IDF components */
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "mbedtls/base64.h"

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>
#include <hubble/sat/pass_prediction.h>

#include "app_ble.h"

static const char *APP_TAG = "main";

#define MS_PER_SEC 1000U
#define US_PER_MS  1000U
#define US_PER_SEC 1000000ULL

/* Device location and sat orbital parameters */
struct hubble_sat_device_pos device_pos;
struct hubble_sat_orbital_params orb_params[HUBBLE_MAX_SAT];
uint8_t orb_params_count;

/* Hubble device key and time */
static uint8_t _hubble_key[CONFIG_HUBBLE_KEY_SIZE];
uint64_t unix_time_ms;

/* Sem for sync time / orb params and wait for sat tx */
SemaphoreHandle_t sync_sem;
static SemaphoreHandle_t _sat_tx_sem;

static esp_timer_handle_t _sat_timer;

static void _sat_timer_cb(void *arg)
{
	xSemaphoreGive(_sat_tx_sem);
}

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

/*
 * NOTE: this function has many early returns.
 * For production code, we should clean up resources (sem, timers, deinit stack, etc.)
 * on each failure case.
 * For simplicity of the sample, we just exit on failure.
 */
void app_main(void)
{
	struct hubble_sat_pass_info pass_info = {0};
	struct hubble_sat_packet packet = {0};
	esp_timer_create_args_t sat_timer_args = {0};
	uint64_t now_ms;
	uint64_t sat_wait_us;
	esp_err_t err;
	int ret;

	/* NVS flash init – dependency of NimBLE stack */
	err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
	    err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);

	/* Decode device key */
	if (strlen(CONFIG_HUBBLE_DEVICE_KEY) != 0) {
		size_t outlen = 0;

		ret = mbedtls_base64_decode(
			_hubble_key, sizeof(_hubble_key), &outlen,
			(const unsigned char *)CONFIG_HUBBLE_DEVICE_KEY,
			strlen(CONFIG_HUBBLE_DEVICE_KEY));

		if (ret != 0) {
			ESP_LOGE(APP_TAG, "Invalid key provided!");
			return;
		}

		if (outlen != sizeof(_hubble_key)) {
			ESP_LOGE(APP_TAG, "Invalid key length provided!");
			return;
		}
	}

	/* Create the sat timer */
	sat_timer_args.callback = _sat_timer_cb;
	sat_timer_args.name = "sat_timer";

	err = esp_timer_create(&sat_timer_args, &_sat_timer);
	if (err != ESP_OK) {
		ESP_LOGE(APP_TAG, "Failed to create sat timer (rc=%d)", err);
		return;
	}

	/* Init the sem */
	_sat_tx_sem = xSemaphoreCreateBinary();
	sync_sem = xSemaphoreCreateBinary();

	if (_sat_tx_sem == NULL || sync_sem == NULL) {
		ESP_LOGE(APP_TAG, "Failed to create semaphores");
		return;
	}

	// Select external antenna
	select_external_antenna();

	/* Init and enable BLE */
	ret = ble_init();
	if (ret != 0) {
		ESP_LOGE(APP_TAG, "Failed to init BLE, ret: %d", ret);
		return;
	}

	/* We sit here waiting to have time */
	xSemaphoreTake(sync_sem, portMAX_DELAY);

	/* Init Hubble */
	ret = hubble_init(unix_time_ms, _hubble_key);
	if (ret != 0) {
		ESP_LOGE(APP_TAG,
			 "Failed to initialize Hubble Network, ret: %d", ret);
		return;
	}

	/* Set sats params */
	ret = hubble_sat_satellites_set(orb_params, orb_params_count);
	if (ret != 0) {
		ESP_LOGE(APP_TAG,
			 "Failed to set satellite orbital params data, ret: %d",
			 ret);
		return;
	}

	ESP_LOGI(APP_TAG, "Hubble Network initialized");

	for (;;) {
		/* Calculate the next pass time */
		now_ms = hubble_time_get();
		ret = hubble_sat_next_pass_get(now_ms, &device_pos, &pass_info);
		if (ret != 0) {
			ESP_LOGE(APP_TAG,
				 "Failed to get next pass info, err: %d", ret);
			return;
		}

		/*
		 * If the pass start <= current time, this means we're in the
		 * middle of a pass. We can compute the next one
		 */
		if (pass_info.start <= now_ms) {
			ESP_LOGI(APP_TAG, "Current pass is ongoing or in the "
					  "past, search for next pass...");

			ret = hubble_sat_next_pass_get(
				pass_info.start + pass_info.duration,
				&device_pos, &pass_info);
			if (ret != 0) {
				ESP_LOGE(APP_TAG,
					 "Failed to get next pass info: %d", ret);
				return;
			}
		}

#ifdef CONFIG_HUBBLE_SAMPLE_DEBUG
		sat_wait_us = 120 * US_PER_SEC;
		ESP_LOGI(APP_TAG, "Next pass in 120 seconds");
#else
		ESP_LOGI(APP_TAG, "Next pass at: %llu (unix epoch seconds)",
			 pass_info.start / MS_PER_SEC);

		/* Schedule the pass */
		sat_wait_us = (pass_info.start - now_ms) * US_PER_MS;
#endif
		err = esp_timer_start_once(_sat_timer, sat_wait_us);
		if (err != ESP_OK) {
			ESP_LOGE(APP_TAG, "Failed to start sat timer, ret: %d",
				 err);
			return;
		}

		/* Start BLE advertising */
		ret = ble_adv_start();
		if (ret != 0) {
			ESP_LOGE(APP_TAG,
				 "Failed to start advertising, ret: %d", ret);
			return;
		}
		ESP_LOGI(APP_TAG, "Starting BLE advertising...");

		/* Wait for sat pass */
		xSemaphoreTake(_sat_tx_sem, portMAX_DELAY);

		/* Stop BLE and start sat tx */
		ret = ble_adv_stop();
		if (ret != 0) {
			ESP_LOGE(APP_TAG, "Failed to stop advertising, ret: %d",
				 ret);
			return;
		}

		ret = hubble_sat_packet_get(&packet, NULL, 0);
		if (ret != 0) {
			ESP_LOGE(APP_TAG, "Failed to get sat packet, ret: %d",
				 ret);
			return;
		}

		ESP_LOGI(APP_TAG, "Starting satellite transmission...");
		ret = hubble_sat_packet_send(&packet,
					     HUBBLE_SAT_RELIABILITY_NORMAL);
		if (ret != 0) {
			ESP_LOGE(APP_TAG, "Failed to send sat packet, ret: %d",
				 ret);
			return;
		}
	}
}
