/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "gnss_manager.h"
#include "nmea_parser.h"

#define GNSS_FIX_RATE_MS 30000U

static const char *TAG = "gnss";

static struct gnss_fix latest_fix;
static nmea_parser_handle_t nmea_hdl;
static SemaphoreHandle_t fix_sem;
static SemaphoreHandle_t data_mutex;

static void gnss_event_handler(void *event_handler_arg,
			       esp_event_base_t event_base, int32_t event_id,
			       void *event_data)
{
	gps_t *gps = NULL;
	struct tm utc_time = {0};
	uint64_t utc_s;
	char *original_tz;

	if (event_id != GPS_UPDATE) {
		return;
	}

	gps = (gps_t *)event_data;
	if (gps == NULL || !gps->valid || gps->fix == GPS_FIX_INVALID) {
		return;
	}

	/*
	 * Convert and store the last GPS fix time
	 * GNSS API returns years since 2000
	 */
	utc_time.tm_year = gps->date.year + 100;
	utc_time.tm_mon = gps->date.month - 1;
	utc_time.tm_mday = gps->date.day;
	utc_time.tm_hour = gps->tim.hour;
	utc_time.tm_min = gps->tim.minute;
	utc_time.tm_sec = gps->tim.second;

	/* Convert to utc */
	/* TODO: not the best, check for alternative of this */
	original_tz = getenv("TZ");
	setenv("TZ", "UTC0", 1);
	tzset();

	utc_s = (uint64_t)mktime(&utc_time);

	if (original_tz) {
		setenv("TZ", original_tz, 1);
	} else {
		unsetenv("TZ");
	}
	tzset();

	xSemaphoreTake(data_mutex, portMAX_DELAY);
	latest_fix.latitude = gps->latitude;
	latest_fix.longitude = gps->longitude;
	latest_fix.utc = utc_s;
	latest_fix.uptime_us = (uint64_t)esp_timer_get_time();
	xSemaphoreGive(data_mutex);

	xSemaphoreGive(fix_sem);
}

esp_err_t gnss_init(void)
{
	esp_err_t ret;
	nmea_parser_config_t config;

	fix_sem = xSemaphoreCreateBinary();
	data_mutex = xSemaphoreCreateMutex();
	if (fix_sem == NULL || data_mutex == NULL) {
		ESP_LOGE(TAG, "Failed to create semaphores");
		return ESP_FAIL;
	}

	/* NMEA parser init */
	config = (nmea_parser_config_t)NMEA_PARSER_CONFIG_DEFAULT();
	nmea_hdl = nmea_parser_init(&config);

	if (nmea_hdl == NULL) {
		ESP_LOGE(TAG, "NMEA parser init failed");
		return ESP_FAIL;
	}

	/* register event handler for NMEA parser library */
	ret = nmea_parser_add_handler(nmea_hdl, gnss_event_handler, NULL);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "NMEA parser add handler failed (err %d)", ret);
		(void)nmea_parser_deinit(nmea_hdl);
	}

	return ret;
}

esp_err_t gnss_deinit(void)
{
	esp_err_t ret;

	ret = nmea_parser_remove_handler(nmea_hdl, gnss_event_handler);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "NMEA parser remove handler failed (err %d)", ret);
	}

	/* deinit NMEA parser library */
	ret = nmea_parser_deinit(nmea_hdl);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "NMEA parser deinit failed (err %d)", ret);
	}

	vSemaphoreDelete(fix_sem);
	vSemaphoreDelete(data_mutex);

	return ESP_OK;
}

esp_err_t gnss_get_fix(uint32_t timeout, struct gnss_fix *out)
{
	esp_err_t ret;
	TickType_t wait_ticks;

	wait_ticks = (timeout == WAIT_FOREVER) ? portMAX_DELAY
					       : pdMS_TO_TICKS(timeout * 1000);

	/* Drain the sem to make sure we get the lastest fix */
	while (xSemaphoreTake(fix_sem, 0) == pdTRUE);

	ret = xSemaphoreTake(fix_sem, wait_ticks);
	if (ret != pdTRUE) {
		return ESP_ERR_TIMEOUT;
	}

	xSemaphoreTake(data_mutex, portMAX_DELAY);
	memset(out, 0, sizeof(*out));
	memcpy(out, &latest_fix, sizeof(*out));
	xSemaphoreGive(data_mutex);

	return ESP_OK;
}
