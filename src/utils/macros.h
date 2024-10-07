/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SRC_UTILS_MACROS_H
#define SRC_UTILS_MACROS_H

#include <stddef.h>
#include <stdint.h>

#define HUBBLE_ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))


#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HUBBLE_CPU_TO_BE32(val)                                                \
	((uint32_t)((((val) >> 24) & 0xff) | (((val) >> 8) & 0xff00) |         \
		    (((val) & 0xff00) << 8) | (((val) & 0xff) << 24)))
#else
#define HUBBLE_CPU_TO_BE32(val) (val)
#endif


#endif /* SRC_UTILS_MACROS_H */
