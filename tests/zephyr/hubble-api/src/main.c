/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/port/crypto.h>

#include <zephyr/ztest.h>

#include <stdint.h>
#include <string.h>

static uint64_t _unix_time = 1760210751803ULL;
static uint8_t _key[CONFIG_HUBBLE_KEY_SIZE];

static bool _board_init_fail = false;
static bool _crypto_init_fail = false;

/* Implement sat board support. */
int hubble_sat_board_init(void)
{
	return _board_init_fail ? -EINVAL : 0;
}

int hubble_sat_board_enable(void)
{
	return 0;
}

int hubble_sat_board_disable(void)
{
	return 0;
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	ARG_UNUSED(packet);

	return 0;
}

/* Custom crypto for testing */

void hubble_crypto_zeroize(void *buf, size_t len)
{
	ARG_UNUSED(buf);
	ARG_UNUSED(len);
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	ARG_UNUSED(key);
	ARG_UNUSED(input);
	ARG_UNUSED(input_len);
	ARG_UNUSED(output);

	return 0;
}

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_NONCE_BUFFER_SIZE],
			  const uint8_t *data, size_t len, uint8_t *output)
{
	ARG_UNUSED(key);
	ARG_UNUSED(nonce_counter);
	ARG_UNUSED(data);
	ARG_UNUSED(len);
	ARG_UNUSED(output);

	return 0;
}

int hubble_crypto_init(void)
{
	return _crypto_init_fail ? -EINVAL : 0;
}

ZTEST(hubble_api_test, test_init)
{
	int err;

	/* Valid init */
	err = hubble_init(_unix_time, _key);
	zassert_ok(err);

	/* NULL key is accepted; key must be provisioned separately */
	err = hubble_init(_unix_time, NULL);
	zassert_ok(err);

	/* It fails when board initialization fails */
	_board_init_fail = true;
	err = hubble_init(_unix_time, _key);
	zassert_not_ok(err);
	_board_init_fail = false;

	/* It fails when crypto initialization fails */
	_crypto_init_fail = true;
	err = hubble_init(_unix_time, _key);
	zassert_not_ok(err);
	_crypto_init_fail = false;
}

ZTEST(hubble_api_test, test_init_invalid_time)
{
#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME
	int err;

	/* Time smaller than current uptime must be rejected */
	err = hubble_init(1U, _key);
	zassert_not_ok(err);
#else
	ztest_test_skip();
#endif
}

ZTEST(hubble_api_test, test_time_set)
{
	int err;

	err = hubble_init(_unix_time, _key);
	zassert_ok(err);

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME
	/* Update to a later time */
	err = hubble_time_set(_unix_time + 60000U);
	zassert_ok(err);

	/* Time smaller than current uptime must be rejected */
	err = hubble_time_set(1U);
	zassert_not_ok(err);

	/* Prior valid time must be preserved after the failed call */
	zassert_not_equal(0, hubble_time_get());
#else
	ztest_test_skip();
#endif
}

ZTEST(hubble_api_test, test_time_get)
{
	int err;

	err = hubble_init(_unix_time, _key);
	zassert_ok(err);

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_UNIX_TIME
	/* Returned time must be at least the value passed to hubble_init */
	zassert_true(hubble_time_get() >= _unix_time);
#else
	/* In device uptime mode hubble_time_get is not meaningful */
	ztest_test_skip();
#endif
}

ZTEST(hubble_api_test, test_key_set)
{
	int err;

	/* NULL key must be rejected */
	err = hubble_key_set(NULL);
	zassert_not_ok(err);

	/* Valid key must be accepted */
	err = hubble_key_set(_key);
	zassert_ok(err);
}

ZTEST(hubble_api_test, test_counter_get)
{
	int err;
	uint32_t counter;

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
	err = hubble_counter_get(&counter);
	zassert_ok(err);
#else
	/**
	 * When using Unix time, trying to get a counter
	 * without properly set time fails.
	 */
	err = hubble_counter_get(&counter);
	zassert_not_ok(err);
#endif

	err = hubble_init(_unix_time, _key);
	zassert_ok(err);

	/* NULL output pointer must be rejected */
	err = hubble_counter_get(NULL);
	zassert_not_ok(err);

	/* Valid call must succeed and return a counter */
	err = hubble_counter_get(&counter);
	zassert_ok(err);

#ifdef CONFIG_HUBBLE_COUNTER_SOURCE_DEVICE_UPTIME
	/* Device uptime counter wraps at HUBBLE_EID_POOL_SIZE (128) */
	zassert_true(counter < 128U);
#endif
}

ZTEST(hubble_api_test, test_config_vector_get)
{
	const char *hdcv = hubble_config_vector_get();

	/* Must return a non-NULL, versioned HDCV string. */
	zassert_not_null(hdcv);
	zassert_true(strncmp(hdcv, "HDCV:1.0/", strlen("HDCV:1.0/")) == 0,
		     "unexpected HDCV prefix: %s", hdcv);
}

ZTEST(hubble_api_test, test_config_vector_check_self)
{
	/* The build's own vector must be recognized as matching. */
	zassert_true(hubble_config_vector_check(hubble_config_vector_get()));
}

ZTEST(hubble_api_test, test_config_vector_check_null)
{
	/* NULL is never a match. */
	zassert_false(hubble_config_vector_check(NULL));
}

ZTEST(hubble_api_test, test_config_vector_check_mismatch)
{
	/* An empty string does not match. */
	zassert_false(hubble_config_vector_check(""));

	/* Garbage does not match. */
	zassert_false(hubble_config_vector_check("not-an-hdcv"));

	/*
	 * A well-formed but different vector must not match.  This build is
	 * satellite-only (N:S), so a terrestrial vector can never equal it.
	 */
	zassert_false(hubble_config_vector_check(
		"HDCV:1.0/E:128/CS:UT/RP:S1/N:T/TV:0"));
}

ZTEST(hubble_api_test, test_config_vector_check_is_exact)
{
	const char *hdcv = hubble_config_vector_get();
	size_t len = strlen(hdcv);
	char buf[64];

	zassert_true(len + sizeof("/X:1") <= sizeof(buf),
		     "HDCV string too long for test buffer");

	/* A truncated prefix of the real vector must not match. */
	memcpy(buf, hdcv, len + 1);
	buf[len - 1] = '\0';
	zassert_false(hubble_config_vector_check(buf));

	/* The real vector with an extra trailing field must not match. */
	memcpy(buf, hdcv, len + 1);
	strcat(buf, "/X:1");
	zassert_false(hubble_config_vector_check(buf));
}

static void *hubble_api_test_setup(void)
{
	/* Lets ensure that uptime is different from 0 */
	k_sleep(K_SECONDS(2));
	return NULL;
}

ZTEST_SUITE(hubble_api_test, NULL, hubble_api_test_setup, NULL, NULL, NULL);
