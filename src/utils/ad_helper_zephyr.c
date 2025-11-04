/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/bluetooth/bluetooth.h>

#include <hubble/ad_helper.h>
#include <hubble/ble.h>
#include <hubble/hubble_port.h>

// Buffer used for Hubble data
// Encrypted data will go in here for the advertisement.
#define HUBBLE_USER_BUFFER_LEN 31
static uint8_t _hubble_user_buffer[HUBBLE_USER_BUFFER_LEN];

static uint16_t app_adv_uuids = HUBBLE_BLE_UUID;
static struct bt_data app_ad[2] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
};

struct bt_le_ext_adv *adv;

int hubble_ad_init(const hubble_ad_config_t* config)
{
	if (config == NULL) {
		return -EINVAL;
	}

	int err;

	err = bt_le_ext_adv_create(
		BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA,
						config->interval_min,
						config->interval_max,
						NULL),
		NULL, &adv);
	if (err != 0) {
		return err;
	}

	// Set some initial data
	err = hubble_ad_update_data(NULL, 0);
	if (err != 0) {
		return err;
	}

	return err;
}

int hubble_ad_update_config(const hubble_ad_config_t* config)
{
	return 0;
}

int hubble_ad_deinit(void)
{
	int err;
	// May already be stopped so ignore errors here
	hubble_ad_stop();

	err = bt_le_ext_adv_delete(adv);
	if (err != 0) {
		HUBBLE_LOG_WARNING("Failed to deinit (err=%d)", err);
		return err;
	}
	adv = NULL;
	return err;
}

int hubble_ad_start(void)
{
	int err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err != 0) {
		HUBBLE_LOG_WARNING("Failed to start advertising (err=%d)", err);
		return err;
	}
	return err;
}

int hubble_ad_stop(void)
{
	int err = bt_le_ext_adv_stop(adv);
	if (err != 0) {
		HUBBLE_LOG_WARNING("Failed to stop advertising (err=%d)", err);
		return err;
	}
	return err;
}

int hubble_ad_update_data(const uint8_t *data, size_t len) {
	int err;
	size_t out_len = sizeof(_hubble_user_buffer);
	err = hubble_ble_advertise_get(data, len, _hubble_user_buffer, &out_len);
	if (err != 0) {
		HUBBLE_LOG_WARNING("Failed to generate the advertisement data (err=%d)", err);
		return err;
	}
	app_ad[1].data_len = out_len;
	app_ad[1].type = BT_DATA_SVC_DATA16;
	app_ad[1].data = _hubble_user_buffer;

	err = bt_le_ext_adv_set_data(adv, app_ad, ARRAY_SIZE(app_ad), NULL, 0);
	if (err != 0) {
		HUBBLE_LOG_WARNING("Failed to set the advertisement data (err=%d)", err);
		return err;
	}

	return err;
}
