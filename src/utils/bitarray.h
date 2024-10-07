/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_UTILS_BITARRAY_H
#define SRC_UTILS_BITARRAY_H

#include <stddef.h>
#include <stdint.h>

/* Number of bits in one char. */
#define HUBBLE_CHAR_BITS   8

/* Max number of symbols in a hubble sat packet. */
#define HUBBLE_MAX_SYMBOLS 77

/* A simple bitarary struct to pack the data in a way that is easy to encode / decode */
struct hubble_bitarray {
	size_t index;
	size_t len;
	uint8_t data[HUBBLE_MAX_SYMBOLS];
};
/**
 * @brief Initialize a hubble bit array.
 *
 * This function initializes the given hubble bit array structure.
 *
 * @param bit_array Pointer to the hubble bit array structure to initialize.
 **/
void hubble_bitarray_init(struct hubble_bitarray *bit_array);

/**
 * @brief Append bits to a hubble bit array.
 *
 * This function appends the specified number of bits from the input to the
 * given hubble bit array.
 *
 * @param bit_array Pointer to the hubble bit array structure.
 * @param input Pointer to the input data.
 * @param input_len_bits Number of bits to append from the input.
 * @return int 0 on success, non-zero on failure.
 **/
int hubble_bitarray_append(struct hubble_bitarray *bit_array,
			   const uint8_t *input, size_t input_len_bits);

/**
 * @brief Set a bit in the hubble bit array.
 *
 * This function sets the specified bit in the given hubble bit array.
 *
 * @param bit_array Pointer to the hubble bit array structure.
 * @param index The index of the bit to set.
 * @param value The value of the bit to set (0 or 1).
 * @return int 0 on success, non-zero on failure.
 **/
int hubble_bitarray_set_bit(struct hubble_bitarray *bit_array, size_t intdex,
			    uint8_t value);

/**
 * @brief Get the value of a bit in the hubble bit array.
 *
 * This function retrieves the value of the bit at the specified index in the
 * given hubble bit array.
 *
 * @param bit_array Pointer to the hubble bit array structure.
 * @param index The index of the bit to retrieve.
 * @return int The value of the bit (0 or 1). Negative value on failure.
 **/
int hubble_bitarray_get_bit(struct hubble_bitarray *bit_array, size_t index);

#endif /* SRC_UTILS_BITARRAY_H */
