/*
 * Copyright (c) 2024 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_REED_SOLOMON_ENCODER_H
#define SRC_REED_SOLOMON_ENCODER_H

#include <stdint.h>

/**
 * @brief Generates the Galois Field (GF) for Reed-Solomon encoding.
 *
 * This function initializes the lookup tables required for efficient arithmetic
 * operations in the Galois Field. It must be called before any other encoding
 * functions.
 *
 * @note This function does not take any parameters or return any values.
 */
void rse_gf_generate(void);

/**
 * @brief Generates the generator polynomial for the Reed-Solomon code.
 *
 * The generator polynomial is used to encode data and append parity symbols.
 *
 * @param[in] tt The number of error-correcting symbols (or the error-correcting
 *               capability of the code).
 *
 * @note Ensure that the Galois Field is generated using ~rse_gf_generate~
 *       before calling this function.
 */
void rse_poly_generate(int tt);

/**
 * @brief Encodes the input data using the Reed-Solomon algorithm.
 *
 * This function takes an array of input data symbols, appends parity symbols,
 * and returns the encoded message.
 *
 * @param[in] data Array of input data symbols to be encoded.
 * @param[in] kk Number of data symbols.
 * @param[in] tt Number of error-correcting symbols.
 *
 * @return A pointer to an array containing the encoded data, including
 *         parity symbols.
 *
 * @note The total length of the encoded message is ~kk + 2 * tt~.
 *       The caller is responsible for managing the memory of the returned array.
 */
int *rse_rs_encode(int data[], int kk, int tt);

#endif /* SRC_REED_SOLOMON_ENCODER_H */
