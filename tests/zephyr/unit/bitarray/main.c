/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test the bitarray functions */

#include <zephyr/ztest.h>

#include <utils/bitarray.h>

#include <limits.h>

ZTEST(bitarray, test_overflow)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xf;

	hubble_bitarray_init(&bit_array);

	for (int i = 0; i < (HUBBLE_MAX_SYMBOLS * 8) - 1; i++) {
		zassert_ok(hubble_bitarray_append(&bit_array, &data, 1));
	}

	/* Ok, any attempt to append more data should fail */
	zassert_not_ok(hubble_bitarray_append(&bit_array, &data, sizeof(data) * 8));
}

ZTEST(bitarray, test_invalid_access)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xff;

	hubble_bitarray_init(&bit_array);

	zassert_equal(hubble_bitarray_get_bit(&bit_array, 1), -EINVAL);
}

ZTEST(bitarray, test_regular_usage)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xff;
	uint16_t test = 0x0;

	hubble_bitarray_init(&bit_array);

	zassert_ok(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 8)));

	/* Let's check some data and play with it */
	zassert_equal(hubble_bitarray_get_bit(&bit_array, 1), 1);
	zassert_equal(hubble_bitarray_get_bit(&bit_array, 0), 1);
	zassert_equal(hubble_bitarray_get_bit(&bit_array, (sizeof(data) * 8) - 1), 1);
	zassert_equal(hubble_bitarray_set_bit(&bit_array, 1, 0), 0);
	zassert_equal(hubble_bitarray_get_bit(&bit_array, 1), 0);

	data = 0x0;
	zassert_ok(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 4)));
	data = 0xff;
	zassert_ok(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 4)));

	/* Now lest check if the 2 bytes we have added are correct */
	for (int i = 0; i < 16; i++) {
		int bit = hubble_bitarray_get_bit(&bit_array, i);

		zassert_true((bit == 0) || (bit == 1));

		test |= (bit << i);
	}
	/* We have changed the bit in index 1 to 0 */

	zassert_equal(test, 0xf0fd);
}

ZTEST_SUITE(bitarray, NULL, NULL, NULL, NULL, NULL);
