/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_bt_defs.h"
#include "freertos/FreeRTOS.h"

/* Hubble */
#include <hubble/hubble.h>

#define BLE_ADV_LEN     31
#define ADV_CONFIG_FLAG (1 << 0)

#define USEC_PER_SEC    1000000ULL
#define SEC_PER_HOUR    (60ULL * 60ULL)
#define HOUR_TO_US(_x)  ((_x) * SEC_PER_HOUR * USEC_PER_SEC)

#include "utc.c"
#include "key.c"

static void esp_gap_cb(esp_gap_ble_cb_event_t event,
		       esp_ble_gap_cb_param_t *param);

static const char *DEMO_TAG = "BLE_BEACON";

static uint8_t adv_config_done = 0U;
static esp_bd_addr_t local_addr;
static uint8_t local_addr_type;

static esp_ble_adv_params_t adv_params = {
	.adv_int_min = 0x20, // 20ms
	.adv_int_max = 0x20, // 20ms
	.adv_type = ADV_TYPE_SCAN_IND,
	.own_addr_type = BLE_ADDR_TYPE_RANDOM,
	.channel_map = ADV_CHNL_ALL,
	.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define _ADV_BUFFER_BASE_LEN         6
#define _ADV_BUFFER_POS              6
#define _ADV_BUFFER_SERVICE_DATA_POS 4

/* configure raw data for advertising packet */
static uint8_t adv_raw_data[BLE_ADV_LEN] = {
	0x03, ESP_BLE_AD_TYPE_16SRV_CMPL,   0xa6, 0xfc,
	0x01, ESP_BLE_AD_TYPE_SERVICE_DATA,
};
static esp_bd_addr_t adv_rand_addr;

static esp_err_t _get_hubble_adv(void)
{
	esp_err_t ret;
	size_t out_len = sizeof(adv_raw_data) - _ADV_BUFFER_BASE_LEN;

	memset(&adv_raw_data[_ADV_BUFFER_POS], 0,
	       sizeof(adv_raw_data) - _ADV_BUFFER_BASE_LEN);
	ret = hubble_ble_advertise_get(NULL, 0, &adv_raw_data[_ADV_BUFFER_POS],
				       &out_len);
	if (ret != 0) {
		ESP_LOGE(DEMO_TAG, "Failed to get adv data");
		return ESP_FAIL;
	}

	adv_raw_data[_ADV_BUFFER_SERVICE_DATA_POS] =
		out_len + 1; /* +1 because of service data byte */

	ret = esp_ble_gap_config_adv_data_raw(adv_raw_data,
					      _ADV_BUFFER_BASE_LEN + out_len);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "config adv data failed, error code = %x",
			 ret);
	}

	return ret;
}

static void _timer_cb(void *arg)
{
	(void)_get_hubble_adv();
}

void app_main(void)
{
	esp_err_t ret;
	esp_timer_handle_t adv_timer;

	ret = hubble_init(utc_time, master_key);
	if (ret != 0) {
		ESP_LOGE(DEMO_TAG, "Failed to initialize Hubble BLE Network");
		return;
	}

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
	    ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
	ret = esp_bt_controller_init(&bt_cfg);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s initialize controller failed: %s",
			 __func__, esp_err_to_name(ret));
		return;
	}

	ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s enable controller failed: %s", __func__,
			 esp_err_to_name(ret));
		return;
	}

	esp_bluedroid_config_t cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
	ret = esp_bluedroid_init_with_cfg(&cfg);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s init bluetooth failed: %s", __func__,
			 esp_err_to_name(ret));
		return;
	}

	ret = esp_bluedroid_enable();
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s enable bluetooth failed: %s", __func__,
			 esp_err_to_name(ret));
		return;
	}

	ret = esp_ble_gap_register_callback(esp_gap_cb);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "gap register error, error code = %x", ret);
		return;
	}

	adv_config_done |= ADV_CONFIG_FLAG;

	esp_ble_gap_addr_create_static(adv_rand_addr);
	esp_ble_gap_set_rand_addr((uint8_t *)adv_rand_addr);

	ret = _get_hubble_adv();
	if (ret) {
		ESP_LOGE(DEMO_TAG, "config adv data failed, error code = %x",
			 ret);
		return;
	}

	ret = esp_ble_gap_get_local_used_addr(local_addr, &local_addr_type);
	if (ret) {
		ESP_LOGE(DEMO_TAG,
			 "get local used address failed, error code = %x", ret);
		return;
	}

	ret = esp_timer_create(
		&(esp_timer_create_args_t){.callback = _timer_cb}, &adv_timer);
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s timer create failed: %s", __func__,
			 esp_err_to_name(ret));
		return;
	}

	/* Let's generate new messages every hour */
	ret = esp_timer_start_periodic(adv_timer, HOUR_TO_US(1));
	if (ret) {
		ESP_LOGE(DEMO_TAG, "%s timer start failed: %s", __func__,
			 esp_err_to_name(ret));
	}
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		ESP_LOGI(DEMO_TAG, "Advertising data set, status %d",
			 param->adv_data_cmpl.status);
		adv_config_done &= (~ADV_CONFIG_FLAG);
		esp_ble_gap_set_rand_addr((uint8_t *)adv_rand_addr);
		if (adv_config_done == 0U) {
			esp_ble_gap_start_advertising(&adv_params);
		}
		break;
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
		ESP_LOGI(DEMO_TAG, "Advertising data raw set, status %d",
			 param->adv_data_raw_cmpl.status);
		adv_config_done &= (~ADV_CONFIG_FLAG);
		if (adv_config_done == 0U) {
			esp_ble_gap_start_advertising(&adv_params);
		}
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
			ESP_LOGE(DEMO_TAG, "Advertising start failed, status %d",
				 param->adv_start_cmpl.status);
			break;
		}
		ESP_LOGI(DEMO_TAG, "Advertising start successfully");
		break;
	default:
		break;
	}
}
