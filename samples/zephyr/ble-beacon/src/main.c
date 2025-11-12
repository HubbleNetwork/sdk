/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/ble.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS
#include <zephyr/bluetooth/services/cts.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS */
#include <zephyr/logging/log.h>

#include <stdint.h>
#include <stdlib.h>

#include "key.c"
#include "utc.c"

LOG_MODULE_REGISTER(main);


// Buffer used for Hubble data
// Encrypted data will go in here for the advertisement.
#define HUBBLE_USER_BUFFER_LEN 31
static uint8_t _hubble_user_buffer[HUBBLE_USER_BUFFER_LEN];

#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV
static struct {
	uint16_t uuid;
	uint32_t data;
} __packed app_adv_data = {
	.uuid = CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_UUID,
	.data = CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_DATA,
};
#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS
static uint16_t app_adv_uuids[3] = {
	HUBBLE_BLE_UUID,
	CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_UUID,
	BT_UUID_CTS_VAL,
};
#else
static uint16_t app_adv_uuids[2] = {
	HUBBLE_BLE_UUID,
	CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV_UUID,
};
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS */
static struct bt_data app_ad[3] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
	BT_DATA(BT_DATA_SVC_DATA16, &app_adv_data, sizeof(app_adv_data)),
};
#else
#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS
static uint16_t app_adv_uuids[2] = {
	HUBBLE_BLE_UUID,
	BT_UUID_CTS_VAL,
};
#else
static uint16_t app_adv_uuids = HUBBLE_BLE_UUID;
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS */
static struct bt_data app_ad[2] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
};
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_ADDITIONAL_ADV */

K_SEM_DEFINE(timer_sem, 0, 1);

static void timer_cb(struct k_timer *timer)
{
	k_sem_give(&timer_sem);
}

K_TIMER_DEFINE(message_timer, timer_cb, NULL);


#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS

/* Lets use 0xFCA7 UUID for when it is connectable. Since this UUID is also
 * assigned to Hubble. This way it is easy to distinguish between a device
 * that is properly beaconing from one that needs time synchronization.
 */
#define HUBBLE_BLE_UUID_SYNC 0xFCA7

static int ble_adv_time_sync(void)
{
	int err;

	*(uint16_t *)&app_adv_uuids = HUBBLE_BLE_UUID_SYNC;

	app_ad[1].data_len = sizeof(uint16_t);
	app_ad[1].type = BT_DATA_SVC_DATA16;
	app_ad[1].data = (uint8_t *)&app_adv_uuids;

	err = bt_le_adv_start(
		BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA | BT_LE_ADV_OPT_CONN,
				BT_GAP_ADV_FAST_INT_MIN_2,
				BT_GAP_ADV_FAST_INT_MAX_2, NULL),
		app_ad, ARRAY_SIZE(app_ad), NULL, 0);

	return err;
}

static void ble_work_handler(struct k_work *work)
{
	ble_adv_time_sync();
}

K_WORK_DEFINE(ble_work, ble_work_handler);
K_SEM_DEFINE(time_sem, 0, 1);

static int bt_cts_cts_time_write(struct bt_cts_time_format *cts_time)
{
	return bt_cts_time_to_unix_ms(cts_time, &utc_time);
}

int bt_cts_fill_current_cts_time(struct bt_cts_time_format *cts_time)
{
	return bt_cts_time_from_unix_ms(cts_time, utc_time + k_uptime_get());
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (utc_time != 0) {
		k_sem_give(&time_sem);
	} else {
		k_work_submit(&ble_work);
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.disconnected = disconnected,
};

const struct bt_cts_cb cts_cb = {
	.cts_time_write = bt_cts_cts_time_write,
	.fill_current_cts_time = bt_cts_fill_current_cts_time,
};

static int hubble_ble_time_sync(void)
{
	int ret = 0;

	/* Lets ensure that utc_time is not set */
	utc_time = 0;

	bt_cts_init(&cts_cb);

	ret = ble_adv_time_sync();
	if (ret != 0) {
		return ret;
	}

	k_sem_take(&time_sem, K_FOREVER);

	/* Get back to 0xFCA6 for future regular advertisements */
	*(uint16_t *)&app_adv_uuids = HUBBLE_BLE_UUID;

	/* We no longer need to advertise CTS service */
	app_ad[0].data_len = sizeof(uint16_t) * (ARRAY_SIZE(app_adv_uuids) - 1);
	app_ad[0].type = BT_DATA_UUID16_ALL;
	app_ad[0].data = (uint8_t *)&app_adv_uuids;

	return ret;
}

#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS */

int main(void)
{
	int err = 0;
	size_t out_len;

	LOG_DBG("Hubble Network BLE Beacon started");

	/* Synchrounosly initialize the Bluetooth subsystem. */
	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

#ifdef CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS
	err = hubble_ble_time_sync();
	if (err != 0) {
		LOG_ERR("Could not get utc time synced !");
		goto end;
	}
#endif /* CONFIG_HUBBLE_BEACON_SAMPLE_USE_CTS */

	err = hubble_ble_init(utc_time, master_key);
	if (err != 0) {
		LOG_ERR("Failed to initialize Hubble BLE Network");
		goto end;
	}

	k_timer_start(&message_timer,
		      K_SECONDS(CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADV_PERIOD),
		      K_SECONDS(CONFIG_HUBBLE_BEACON_SAMPLE_UPDATE_ADV_PERIOD));

	for (;;) {
		out_len = HUBBLE_USER_BUFFER_LEN;
		err = hubble_ble_advertise_get(NULL, 0, _hubble_user_buffer,
					       &out_len);
		if (err != 0) {
			LOG_ERR("Failed to get the advertisement data (err=%d)",
				err);
			goto end;
		}
		app_ad[1].data_len = out_len;
		app_ad[1].type = BT_DATA_SVC_DATA16;
		app_ad[1].data = _hubble_user_buffer;

		LOG_DBG("Number of bytes in advertisement: %d", out_len);

		err = bt_le_adv_start(
			BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA,
					BT_GAP_ADV_FAST_INT_MIN_2,
					BT_GAP_ADV_FAST_INT_MAX_2, NULL),
			app_ad, ARRAY_SIZE(app_ad), NULL, 0);
		if (err != 0) {
			LOG_ERR("Bluetooth advertisement failed (err %d)", err);
			goto end;
		}

		k_sem_take(&timer_sem, K_FOREVER);

		err = bt_le_adv_stop();
		if (err != 0) {
			LOG_ERR("Bluetooth advertisement stop failed (err %d)",
				err);
			goto end;
		}
	}

end:
	(void)bt_disable();
	return err;
}
