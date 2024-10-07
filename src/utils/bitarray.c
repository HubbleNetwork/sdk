/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bitarray.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>

static int _append_bit(struct hubble_bitarray *bit_array, uint8_t value)
{
	size_t index = bit_array->index;

	if (value != 0) {
		bit_array->data[index / HUBBLE_CHAR_BITS] |=
			(1 << (index % HUBBLE_CHAR_BITS));
	} else {
		bit_array->data[index / HUBBLE_CHAR_BITS] &=
			~(1 << (index % HUBBLE_CHAR_BITS));
	}

	bit_array->index++;

	return 0;
}

int hubble_bitarray_set_bit(struct hubble_bitarray *bit_array, size_t index,
			    uint8_t value)
{
	if (index >= bit_array->index) {
		return -EINVAL;
	}

	if (value != 0) {
		bit_array->data[index / HUBBLE_CHAR_BITS] |=
			(1 << (index % HUBBLE_CHAR_BITS));
	} else {
		bit_array->data[index / HUBBLE_CHAR_BITS] &=
			~(1 << (index % HUBBLE_CHAR_BITS));
	}

	return 0;
}

int hubble_bitarray_get_bit(struct hubble_bitarray *bit_array, size_t index)
{
	if (index >= bit_array->index) {
		return -EINVAL;
	}

	return (bit_array->data[index / HUBBLE_CHAR_BITS] >>
		(index % HUBBLE_CHAR_BITS)) &
	       1;
}

int hubble_bitarray_append(struct hubble_bitarray *bit_array,
			   const uint8_t *input, size_t input_len_bits)
{
	size_t byte_index = input_len_bits / HUBBLE_CHAR_BITS;

	if ((bit_array->index + input_len_bits) >=
	    (HUBBLE_MAX_SYMBOLS * HUBBLE_CHAR_BITS)) {
		return -EINVAL;
	}

	for (ssize_t i = input_len_bits - 1, j = 0; i >= 0; i--, j++) {
		int err;
		uint8_t bit_value;

		if (((i + 1) % HUBBLE_CHAR_BITS) == 0) {
			byte_index--;
		}
		bit_value = (input[byte_index] >> (i % HUBBLE_CHAR_BITS)) &
			    1; // Extract the i-th bit from the character
		err = _append_bit(bit_array,
				  bit_value); // Set the bit in the array

		if (err != 0) {
			return err;
		}
	}

	return 0;
}

void hubble_bitarray_init(struct hubble_bitarray *bit_array)
{
	bit_array->index = 0;
}
