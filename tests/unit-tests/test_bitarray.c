/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test the bitarray functions */

#include <utils/bitarray.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

void overflow(void)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xf;

	hubble_bitarray_init(&bit_array);

	for (int i = 0; i < (HUBBLE_MAX_SYMBOLS * 8) - 1; i++) {
		assert(hubble_bitarray_append(&bit_array, &data, 1) == 0);
	}

	/* Ok, any attempt to append more data should fail */
	assert(hubble_bitarray_append(&bit_array, &data, sizeof(data) * 8) != 0);
}

void invalid_access(void)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xff;

	hubble_bitarray_init(&bit_array);

	assert(hubble_bitarray_get_bit(&bit_array, 1) == -EINVAL);
	assert(hubble_bitarray_get_bit(&bit_array, INT_MAX) == -EINVAL);
	assert(hubble_bitarray_append(&bit_array, &data, sizeof(data) * 8) == 0);
	assert(hubble_bitarray_get_bit(&bit_array, INT_MAX) == -EINVAL);
}

void regular_usage(void)
{
	struct hubble_bitarray bit_array;
	uint8_t data = 0xff;
	uint16_t test = 0x0;

	hubble_bitarray_init(&bit_array);

	assert(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 8)) == 0);
	/* Let's check some data and play with it */
	assert(hubble_bitarray_get_bit(&bit_array, 1) == 1);
	assert(hubble_bitarray_get_bit(&bit_array, 0) == 1);
	assert(hubble_bitarray_get_bit(&bit_array, (sizeof(data) * 8) - 1) == 1);
	assert(hubble_bitarray_set_bit(&bit_array, 1, 0) == 0);
	assert(hubble_bitarray_get_bit(&bit_array, 1) == 0);

	data = 0x0;
	assert(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 4)) == 0);
	data = 0xff;
	assert(hubble_bitarray_append(&bit_array, &data, (sizeof(data) * 4)) == 0);

	/* Now lest check if the 2 bytes we have added are correct */
	for (int i = 0; i < 16; i++) {
		int bit = hubble_bitarray_get_bit(&bit_array, i);

		assert((bit == 0) || (bit == 1));

		test |= (bit << i);
	}
	/* We have changed the bit in index 1 to 0 */
	assert(test == 0xf0fd);
}

int main(void)
{
	overflow();
	invalid_access();
	regular_usage();

	return 0;
}
