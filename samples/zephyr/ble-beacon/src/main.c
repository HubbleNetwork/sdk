/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble_ble.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>

#include <stdint.h>
#include <stdlib.h>

#include "key.c"
#include "utc.c"

LOG_MODULE_REGISTER(main);

#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV
static uint64_t app_adv_data = CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_DATA;
static uint16_t app_adv_uuids[2] = {
	HUBBLE_BLE_UUID,
	CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_UUID,
};
static struct bt_data app_ad[3] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
	BT_DATA(BT_DATA_SVC_DATA16, &app_adv_data, sizeof(app_adv_data)),
};
#else
static uint16_t app_adv_uuids = HUBBLE_BLE_UUID;
static struct bt_data app_ad[2] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
};
#endif

#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS

K_SEM_DEFINE(timer_sem, 0, 1);

static void timer_cb(struct k_timer *timer)
{
	k_sem_give(&timer_sem);
}

K_TIMER_DEFINE(mac_timer, timer_cb, NULL);

#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS */


int main(void)
{
	void *data;
	int err = 0;
	size_t out_len;

	LOG_DBG("Hubble Network BLE Beacon started");

	err = hubble_ble_init(utc_time);
	if (err != 0) {
		LOG_ERR("Failed to initialize Hubble BLE Network");
		return err;
	}

	err = hubble_ble_key_set(master_key);
	if (err != 0) {
		LOG_ERR("Failed to set the Hubble key");
		return err;
	}

	/* Synchrounosly initialize the Bluetooth subsystem. */
	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	data = hubble_ble_advertise_get(NULL, 0, &out_len);
	if (data == NULL) {
		LOG_ERR("Failed to get the advertisement data");
		return -1;
	}
	app_ad[1].data_len = out_len;
	app_ad[1].type = BT_DATA_SVC_DATA16;
	app_ad[1].data = data;

	LOG_DBG("Number of bytes in advertisement: %d", out_len);

	err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA,
					      BT_GAP_ADV_FAST_INT_MIN_2,
					      BT_GAP_ADV_FAST_INT_MAX_2, NULL),
			      app_ad, ARRAY_SIZE(app_ad), NULL, 0);
	if (err != 0) {
		LOG_ERR("Bluetooth advertisement failed (err %d)", err);
		goto end;
	}

#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS
	k_timer_start(
		&mac_timer,
		K_SECONDS(CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS_PERIOD),
		K_SECONDS(CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS_PERIOD));

	for (;;) {
		k_sem_take(&timer_sem, K_FOREVER);

		err = bt_le_adv_stop();
		if (err != 0) {
			LOG_ERR("Bluetooth advertisement stop failed (err %d)",
				err);
			goto end;
		}

		data = hubble_ble_advertise_get(NULL, 0, &out_len);
		if (data == NULL) {
			LOG_ERR("Failed to get the advertisement data");
			goto end;
		}

		app_ad[1].data_len = out_len;
		app_ad[1].type = BT_DATA_SVC_DATA16;
		app_ad[1].data = data;

		err = bt_le_adv_start(
			BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA,
					BT_GAP_ADV_FAST_INT_MIN_2,
					BT_GAP_ADV_FAST_INT_MAX_2, NULL),
			app_ad, ARRAY_SIZE(app_ad), NULL, 0);
		if (err != 0) {
			LOG_ERR("Bluetooth advertisement failed (err %d)", err);
			goto end;
		}
	}
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADDRESS */

	return 0;

end:
	(void)bt_disable();
	return err;
}
