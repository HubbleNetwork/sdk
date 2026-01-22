/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/port/sys.h>
#include <hubble/port/crypto.h>

#include <zephyr/sys/util.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>

#include "test_decrypt.h"

#define TEST_ADV_BUFFER_SZ      32
#define TIMER_COUNTER_FREQUENCY 86400000ULL

/* Test sequence counter override */
static uint16_t test_seq_override;

uint16_t hubble_sequence_counter_get(void)
{
	return test_seq_override;
}

/* Test keys */
static const uint8_t test_key_primary[CONFIG_HUBBLE_KEY_SIZE] = {
	0xcd, 0x15, 0xa5, 0xab, 0xc0, 0x60, 0xb6, 0x72, 0x88, 0xa6, 0x1e,
	0x44, 0xe9, 0x95, 0xba, 0x77, 0xd1, 0x40, 0xbd, 0x46, 0x56, 0x4b,
	0x88, 0xde, 0x41, 0xc1, 0x5a, 0x92, 0x73, 0xb0, 0xce, 0x85};

static const uint8_t test_key_zeros[CONFIG_HUBBLE_KEY_SIZE] = {0};

/*===========================================================================*/
/* Test Suite: ble_decrypt_test - Round-trip encryption/decryption           */
/*===========================================================================*/

ZTEST(ble_decrypt_test, test_roundtrip_empty_payload)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 0;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(NULL, 0, output, &output_len);
	zassert_ok(ret);

	/* Parse the advertisement */
	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Verify sequence number in output */
	zassert_equal(parsed.seq_no, 0);

	/* Verify encrypted length is 0 */
	zassert_equal(parsed.encrypted_len, 0);

	/* Verify auth tag (CMAC of empty data) */
	ret = test_verify_auth_tag(test_key_primary, time_counter, 0,
				   parsed.encrypted_data, 0, parsed.auth_tag);
	zassert_ok(ret, "Auth tag verification failed");
}

ZTEST(ble_decrypt_test, test_roundtrip_max_payload)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;
	uint8_t payload[HUBBLE_BLE_MAX_DATA_LEN];

	/* Fill with recognizable pattern */
	for (size_t i = 0; i < sizeof(payload); i++) {
		payload[i] = (uint8_t)(i * 17 + 3);
	}

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 100;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(payload, sizeof(payload), output,
					   &output_len);
	zassert_ok(ret);

	/* Parse and decrypt */
	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	zassert_equal(parsed.encrypted_len, sizeof(payload));

	uint8_t decrypted[HUBBLE_BLE_MAX_DATA_LEN];
	ret = test_decrypt_payload(test_key_primary, time_counter,
				   test_seq_override, parsed.encrypted_data,
				   parsed.encrypted_len, decrypted);
	zassert_ok(ret);

	/* Verify decrypted matches original */
	zassert_mem_equal(decrypted, payload, sizeof(payload),
			  "Decrypted data does not match original");

	/* Verify auth tag */
	ret = test_verify_auth_tag(test_key_primary, time_counter,
				   test_seq_override, parsed.encrypted_data,
				   parsed.encrypted_len, parsed.auth_tag);
	zassert_ok(ret, "Auth tag verification failed");
}

ZTEST(ble_decrypt_test, test_roundtrip_various_payloads)
{
	uint32_t time_counter = 1;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;

	static const uint8_t test_payloads[][13] = {
		{0xDE, 0xAD, 0xBE, 0xEF},
		{0x00},
		{0xFF},
		{0x01, 0x02, 0x03, 0x04, 0x05},
		{'H', 'e', 'l', 'l', 'o'},
	};
	static const size_t test_payload_lens[] = {4, 1, 1, 5, 5};

	hubble_init(utc_time, test_key_primary);

	for (size_t i = 0; i < ARRAY_SIZE(test_payloads); i++) {
		test_seq_override = (uint16_t)(i * 50);

		uint8_t output[TEST_ADV_BUFFER_SZ];
		size_t output_len = sizeof(output);

		int ret = hubble_ble_advertise_get(test_payloads[i],
						   test_payload_lens[i], output,
						   &output_len);
		zassert_ok(ret, "Failed for payload %zu", i);

		struct test_ble_adv_parsed parsed;
		ret = test_parse_ble_adv(output, output_len, &parsed);
		zassert_ok(ret);

		uint8_t decrypted[HUBBLE_BLE_MAX_DATA_LEN];
		ret = test_decrypt_payload(
			test_key_primary, time_counter, test_seq_override,
			parsed.encrypted_data, parsed.encrypted_len, decrypted);
		zassert_ok(ret);

		zassert_mem_equal(decrypted, test_payloads[i],
				  test_payload_lens[i],
				  "Roundtrip failed for payload %zu", i);
	}
}

ZTEST(ble_decrypt_test, test_auth_tag_validates)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;
	uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 42;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(payload, sizeof(payload), output,
					   &output_len);
	zassert_ok(ret);

	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Auth tag should validate */
	ret = test_verify_auth_tag(test_key_primary, time_counter,
				   test_seq_override, parsed.encrypted_data,
				   parsed.encrypted_len, parsed.auth_tag);
	zassert_ok(ret, "Valid auth tag should verify successfully");
}

ZTEST(ble_decrypt_test, test_tampered_data_fails)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;
	uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 42;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(payload, sizeof(payload), output,
					   &output_len);
	zassert_ok(ret);

	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Tamper with encrypted data */
	uint8_t tampered_data[HUBBLE_BLE_MAX_DATA_LEN];
	memcpy(tampered_data, parsed.encrypted_data, parsed.encrypted_len);
	tampered_data[0] ^= 0xFF;

	/* Auth tag should fail verification */
	ret = test_verify_auth_tag(test_key_primary, time_counter,
				   test_seq_override, tampered_data,
				   parsed.encrypted_len, parsed.auth_tag);
	zassert_equal(ret, -EBADMSG,
		      "Tampered data should fail auth verification");
}

ZTEST(ble_decrypt_test, test_cross_key_decrypt_fails)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;
	uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};

	/* Encrypt with primary key */
	hubble_init(utc_time, test_key_primary);
	test_seq_override = 50;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(payload, sizeof(payload), output,
					   &output_len);
	zassert_ok(ret);

	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Try to verify with wrong key - should fail */
	ret = test_verify_auth_tag(test_key_zeros, time_counter,
				   test_seq_override, parsed.encrypted_data,
				   parsed.encrypted_len, parsed.auth_tag);
	zassert_equal(ret, -EBADMSG, "Wrong key should fail auth verification");

	/* Decrypt with wrong key produces garbage (not original) */
	uint8_t decrypted[HUBBLE_BLE_MAX_DATA_LEN];
	ret = test_decrypt_payload(test_key_zeros, time_counter,
				   test_seq_override, parsed.encrypted_data,
				   parsed.encrypted_len, decrypted);
	zassert_ok(ret, "Decryption operation should succeed");

	/* But decrypted data should NOT match original */
	bool matches = (memcmp(decrypted, payload, sizeof(payload)) == 0);
	zassert_false(matches, "Wrong key should not decrypt to original");
}

static void *ble_decrypt_test_setup(void)
{
	test_seq_override = 0;
	return NULL;
}

ZTEST_SUITE(ble_decrypt_test, NULL, ble_decrypt_test_setup, NULL, NULL, NULL);

/*===========================================================================*/
/* Test Suite: ble_derive_test - Key derivation function tests               */
/*===========================================================================*/

ZTEST(ble_derive_test, test_device_id_derivation)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 0;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(NULL, 0, output, &output_len);
	zassert_ok(ret);

	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Verify device_id using test helper */
	uint32_t expected_device_id;
	ret = test_derive_device_id(test_key_primary, time_counter,
				    &expected_device_id);
	zassert_ok(ret);

	zassert_equal(parsed.device_id, expected_device_id,
		      "Device ID derivation mismatch");
}

ZTEST(ble_derive_test, test_nonce_derivation_consistency)
{
	/* Verify nonce derivation produces consistent results */
	uint8_t nonce1[12];
	uint8_t nonce2[12];

	int ret = test_derive_nonce(test_key_primary, 20, 100, nonce1);
	zassert_ok(ret);

	ret = test_derive_nonce(test_key_primary, 20, 100, nonce2);
	zassert_ok(ret);

	zassert_mem_equal(nonce1, nonce2, sizeof(nonce1),
			  "Same inputs should produce same nonce");

	/* Different seq_no should produce different nonce */
	ret = test_derive_nonce(test_key_primary, 20, 101, nonce2);
	zassert_ok(ret);

	zassert_false(memcmp(nonce1, nonce2, sizeof(nonce1)) == 0,
		      "Different seq_no should produce different nonce");
}

ZTEST(ble_derive_test, test_encryption_key_derivation_consistency)
{
	/* Verify encryption key derivation produces consistent results */
	uint8_t enc_key1[CONFIG_HUBBLE_KEY_SIZE];
	uint8_t enc_key2[CONFIG_HUBBLE_KEY_SIZE];

	int ret = test_derive_encryption_key(test_key_primary, 20, 100, enc_key1);
	zassert_ok(ret);

	ret = test_derive_encryption_key(test_key_primary, 20, 100, enc_key2);
	zassert_ok(ret);

	zassert_mem_equal(enc_key1, enc_key2, sizeof(enc_key1),
			  "Same inputs should produce same encryption key");

	/* Different inputs should produce different keys */
	ret = test_derive_encryption_key(test_key_primary, 20, 101, enc_key2);
	zassert_ok(ret);

	zassert_false(
		memcmp(enc_key1, enc_key2, sizeof(enc_key1)) == 0,
		"Different seq_no should produce different encryption key");

	ret = test_derive_encryption_key(test_key_primary, 21, 100, enc_key2);
	zassert_ok(ret);

	zassert_false(memcmp(enc_key1, enc_key2, sizeof(enc_key1)) == 0,
		      "Different time_counter should produce different "
		      "encryption key");
}

ZTEST(ble_derive_test, test_derive_null_checks)
{
	uint32_t device_id;
	uint8_t enc_key[CONFIG_HUBBLE_KEY_SIZE];
	uint8_t nonce[12];

	/* NULL master key should fail */
	int ret = test_derive_device_id(NULL, 20, &device_id);
	zassert_equal(ret, -EINVAL, "NULL key should fail");

	ret = test_derive_encryption_key(NULL, 20, 100, enc_key);
	zassert_equal(ret, -EINVAL, "NULL key should fail");

	ret = test_derive_nonce(NULL, 20, 100, nonce);
	zassert_equal(ret, -EINVAL, "NULL key should fail");

	/* NULL output should fail */
	ret = test_derive_device_id(test_key_primary, 20, NULL);
	zassert_equal(ret, -EINVAL, "NULL output should fail");

	ret = test_derive_encryption_key(test_key_primary, 20, 100, NULL);
	zassert_equal(ret, -EINVAL, "NULL output should fail");

	ret = test_derive_nonce(test_key_primary, 20, 100, NULL);
	zassert_equal(ret, -EINVAL, "NULL output should fail");
}

static void *ble_derive_test_setup(void)
{
	test_seq_override = 0;
	return NULL;
}

ZTEST_SUITE(ble_derive_test, NULL, ble_derive_test_setup, NULL, NULL, NULL);

/*===========================================================================*/
/* Test Suite: ble_parse_test - Advertisement parsing tests                  */
/*===========================================================================*/

ZTEST(ble_parse_test, test_parse_null_checks)
{
	uint8_t dummy[16] = {0};
	struct test_ble_adv_parsed parsed;

	/* NULL adv should fail */
	int ret = test_parse_ble_adv(NULL, sizeof(dummy), &parsed);
	zassert_equal(ret, -EINVAL, "NULL adv should fail");

	/* NULL parsed should fail */
	ret = test_parse_ble_adv(dummy, sizeof(dummy), NULL);
	zassert_equal(ret, -EINVAL, "NULL parsed should fail");
}

ZTEST(ble_parse_test, test_parse_too_short)
{
	uint8_t short_adv[8] = {0};
	struct test_ble_adv_parsed parsed;

	/* Buffer too short for minimum advertisement */
	int ret = test_parse_ble_adv(short_adv, sizeof(short_adv), &parsed);
	zassert_equal(ret, -EINVAL, "Too short buffer should fail");
}

ZTEST(ble_parse_test, test_parse_service_uuid)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;

	hubble_init(utc_time, test_key_primary);
	test_seq_override = 0;

	uint8_t output[TEST_ADV_BUFFER_SZ];
	size_t output_len = sizeof(output);

	int ret = hubble_ble_advertise_get(NULL, 0, output, &output_len);
	zassert_ok(ret);

	struct test_ble_adv_parsed parsed;
	ret = test_parse_ble_adv(output, output_len, &parsed);
	zassert_ok(ret);

	/* Service UUID should be HUBBLE_BLE_UUID */
	zassert_equal(parsed.service_uuid, HUBBLE_BLE_UUID,
		      "Service UUID should be 0x%04X, got 0x%04X",
		      HUBBLE_BLE_UUID, parsed.service_uuid);
}

ZTEST(ble_parse_test, test_parse_seq_no_encoding)
{
	uint32_t time_counter = 20;
	uint64_t utc_time = (uint64_t)time_counter * TIMER_COUNTER_FREQUENCY;

	hubble_init(utc_time, test_key_primary);

	uint16_t test_seq_nos[] = {0, 1, 255, 256, 512, 1023};

	for (size_t i = 0; i < ARRAY_SIZE(test_seq_nos); i++) {
		test_seq_override = test_seq_nos[i];

		uint8_t output[TEST_ADV_BUFFER_SZ];
		size_t output_len = sizeof(output);

		int ret = hubble_ble_advertise_get(NULL, 0, output, &output_len);
		zassert_ok(ret);

		struct test_ble_adv_parsed parsed;
		ret = test_parse_ble_adv(output, output_len, &parsed);
		zassert_ok(ret);

		zassert_equal(parsed.seq_no, test_seq_nos[i],
			      "Seq_no mismatch: got %u, expected %u",
			      parsed.seq_no, test_seq_nos[i]);
	}
}

static void *ble_parse_test_setup(void)
{
	test_seq_override = 0;
	return NULL;
}

ZTEST_SUITE(ble_parse_test, NULL, ble_parse_test_setup, NULL, NULL, NULL);
