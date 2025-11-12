/*
 * Copyright (c) 2025 Hubble Network
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/ble.h>
#include <hubble/port/sys.h>

#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

struct test_nonce {
	uint16_t nonce;
	bool valid;
};

#define TEST_ADV_BUFFER_SZ 31

static uint8_t ble_nonce_key[CONFIG_HUBBLE_KEY_SIZE] = {};
static uint64_t ble_nonce_utc = 1760383551803;
static uint16_t nonce_idx;
static bool testing_adv;

struct test_nonce invalid_nonce_sequence[] = {
	{10, true},    /* Let start with something != 0 to check wraps */
	{10, false},   /* repeated nonce, should fail */
	{11, true},    /* Working nonce */
	{1023, true},  /* Test the boundary */
	{1024, false}, /* Test the boundary + 1 */
	{0, true}, /* Wrap is allowed, for the current time counter we start with 10 */
	{8, true},   /* still valid for the same reason */
	{10, false}, /* Already used */
};

uint16_t hubble_sequence_counter_get(void)
{
	if (!testing_adv) {
		return invalid_nonce_sequence[nonce_idx++].nonce;
	}

	return nonce_idx++ % HUBBLE_BLE_MAX_SEQ_COUNTER;
}

ZTEST(ble_nonce_test, test_ble_nonce_invalid)
{
	uint16_t idx;

	for (idx = 0; idx < ARRAY_SIZE(invalid_nonce_sequence); idx++) {
		uint8_t buf[TEST_ADV_BUFFER_SZ];
		size_t out_len = sizeof(buf);
		int status = hubble_ble_advertise_get(NULL, 0, buf, &out_len);

		if (invalid_nonce_sequence[idx].valid) {
			zassert_ok(status);
		} else {
			zassert_not_ok(status);
		}
	}
}

static void *ble_nonce_test_setup(void)
{
	(void)hubble_ble_init(ble_nonce_utc, ble_nonce_key);

	testing_adv = false;
	nonce_idx = 0;

	return NULL;
}

ZTEST_SUITE(ble_nonce_test, NULL, ble_nonce_test_setup, NULL, NULL, NULL);

/* Adv test section */

static uint64_t ble_adv_utc = 1760210751803;
/* zRWlq8BgtnKIph5E6ZW6d9FAvUZWS4jeQcFaknOwzoU= */
static uint8_t ble_adv_key[CONFIG_HUBBLE_KEY_SIZE] = {
	0xcd, 0x15, 0xa5, 0xab, 0xc0, 0x60, 0xb6, 0x72, 0x88, 0xa6, 0x1e,
	0x44, 0xe9, 0x95, 0xba, 0x77, 0xd1, 0x40, 0xbd, 0x46, 0x56, 0x4b,
	0x88, 0xde, 0x41, 0xc1, 0x5a, 0x92, 0x73, 0xb0, 0xce, 0x85};

#define ADV_TEST_BUFFER_SIZE 32

struct test_adv {
	uint8_t input[ADV_TEST_BUFFER_SIZE];
	uint8_t input_len;
	uint8_t output[ADV_TEST_BUFFER_SIZE];
};

struct test_adv test_adv_data[] = {
	{{},
	 0,
	 {0xa6, 0xfc, 0x00, 0x00, 0xc0, 0x48, 0xb6, 0x33, 0x7f, 0x4f, 0x35, 0xbb}},
	{{0xde, 0xad, 0xbe, 0xef},
	 4,
	 {0xa6, 0xfc, 0x00, 0x01, 0xc0, 0x48, 0xb6, 0x33, 0x45, 0xa8, 0xae,
	  0xc6, 0xc0, 0x2e, 0xac, 0xf0}},
};

ZTEST(ble_adv_test, test_ble_adv)
{
	uint16_t idx;

	for (idx = 0; idx < ARRAY_SIZE(test_adv_data); idx++) {
		uint8_t buf[TEST_ADV_BUFFER_SZ];
		size_t out_len = sizeof(buf);
		int status = hubble_ble_advertise_get(
			test_adv_data[idx].input, test_adv_data[idx].input_len,
			buf, &out_len);
		zassert_ok(status);
		zassert_mem_equal(buf, test_adv_data[idx].output, out_len);
	}
}

static void *ble_adv_test_setup(void)
{
	(void)hubble_ble_init(ble_adv_utc, ble_adv_key);

	testing_adv = true;
	nonce_idx = 0;

	return NULL;
}

ZTEST_SUITE(ble_adv_test, NULL, ble_adv_test_setup, NULL, NULL, NULL);
