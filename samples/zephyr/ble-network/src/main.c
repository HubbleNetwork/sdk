/*
 * Copyright (c) 2024 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/ble.h>

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include <zephyr/shell/shell.h>

#include <app_version.h>

#include <stdlib.h>

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);
K_SEM_DEFINE(app_sem, 0, 1);
K_SEM_DEFINE(key_sem, 0, 1);

static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];
static uint64_t utc_time;
static int sum;
static int chunk_element;
static char chunk[2];
static uint16_t app_adv_uuids = HUBBLE_BLE_UUID;
static struct bt_data app_ad[2] = {
	BT_DATA(BT_DATA_UUID16_ALL, &app_adv_uuids, sizeof(app_adv_uuids)),
	{},
};


static inline bool is_hexchar(uint8_t data)
{
	return (data >= 0x30 && data <= 0x39) ||
	       (data >= 0x61 && data <= 0x66) || (data >= 0x41 && data <= 0x46);
}

static void bypass_cb(const struct shell *sh, uint8_t *recv, size_t len)
{
	bool escape = false;
	static uint8_t tail;
	uint8_t byte;

#define CHAR_CAN 0x18
#define CHAR_DC1 0x11
	if (tail == CHAR_CAN && recv[0] == CHAR_DC1) {
		escape = true;
	} else {
		for (int i = 0; i < (len - 1); i++) {
			if (recv[i] == CHAR_CAN && recv[i + 1] == CHAR_DC1) {
				escape = true;
				break;
			}
		}
	}
#undef CHAR_CAN
#undef CHAR_DC1

	if (sum > sizeof(master_key)) {
		shell_print(sh, "Given key is too big !");
		shell_set_bypass(sh, NULL);
		if (hubble_ble_key_set((void *)master_key) != 0) {
			LOG_WRN("Failed to set the encryption key");
		}

		return;
	}

	if (escape) {
		shell_print(sh, "Number of bytes read: %d", sum);
		shell_set_bypass(sh, NULL);
		if (hubble_ble_key_set((void *)master_key) != 0) {
			LOG_WRN("Failed to set the encryption key");
		}
		k_sem_give(&key_sem);

		return;
	}

	tail = recv[len - 1];

	if (is_hexchar(*recv)) {
		chunk[chunk_element] = *recv;
		chunk_element++;
	}

	if (chunk_element == 2) {
		byte = (uint8_t)strtoul(chunk, NULL, 16);
		master_key[sum] = byte;
		sum++;
		chunk_element = 0;
	}
}

static int cmd_key(const struct shell *sh, size_t argc, char **argv, void *data)
{
	shell_print(sh, "Please transmit the key through the serial. e.g: "
			"xxd -p key > /dev/ttyX\n"
			"Loading...\npress ctrl-x ctrl-q to escape");

	chunk_element = 0;
	sum = 0;

	shell_set_bypass(sh, bypass_cb);

	return 0;
}

static int cmd_utc(const struct shell *sh, size_t argc, char **argv, void *data)
{
	int ret = 0;
	static bool give_sem = true;

	utc_time = atoll(argv[1]);

	if (give_sem) {
		k_sem_give(&app_sem);
		give_sem = false;
	} else {
		ret = hubble_ble_utc_set(utc_time);
	}

	return ret;
}

static int cmd_data(const struct shell *sh, size_t argc, char **argv,
		    void *data)
{
	void *ad;
	size_t out_len;

	ad = hubble_ble_advertise_get((const uint8_t *)argv[1], strlen(argv[1]),
				      &out_len);
	if (ad == NULL) {
		LOG_ERR("Failed to get the advertisement data");
		return -1;
	}

	app_ad[1].data_len = out_len;
	app_ad[1].type = BT_DATA_SVC_DATA16;
	app_ad[1].data = ad;

	LOG_DBG("Updating advertisement data (len %d bytes)", out_len);

	return bt_le_adv_update_data(app_ad, ARRAY_SIZE(app_ad), NULL, 0);
}


SHELL_CMD_REGISTER(key, NULL, "Set Hubble Network key", cmd_key);
SHELL_CMD_ARG_REGISTER(utc, NULL, "Set UTC time", cmd_utc, 2, 0);
SHELL_CMD_ARG_REGISTER(data, NULL, "Set data to advertise", cmd_data, 2, 0);

int main(void)
{
	int err = 0;

	LOG_DBG("Hubble Network Sample %s", APP_VERSION_STRING);

	printk("Insert key and utc time to start. Type help for more "
	       "information.\n");

	/* Lets wait for the user to set UTC time and Key*/
	k_sem_take(&key_sem, K_FOREVER);
	k_sem_take(&app_sem, K_FOREVER);

	LOG_DBG("Key and UTC time set");

	err = hubble_ble_init(utc_time);
	if (err != 0) {
		LOG_ERR("Failed to initialize Hubble BLE Network");
		return err;
	}

	/* Synchrounosly initialize the Bluetooth subsystem. */
	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}

	err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_NRPA,
					      BT_GAP_ADV_FAST_INT_MIN_2,
					      BT_GAP_ADV_FAST_INT_MAX_2, NULL),
			      NULL, 0, NULL, 0);
	if (err != 0) {
		LOG_ERR("Bluetooth advertisement failed (err %d)", err);
		goto end;
	}

	return 0;

end:
	(void)bt_disable();
	return err;
}
