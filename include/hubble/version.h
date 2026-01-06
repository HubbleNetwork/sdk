/*
 * Copyright (c) 2025 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef INCLUDE_HUBBLE_VERSION_H
#define INCLUDE_HUBBLE_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

/** Major version number (X.x.x) */
#define HUBBLE_SDK_VERSION_MAJOR 1
/** Minor version number (x.X.x) */
#define HUBBLE_SDK_VERSION_MINOR 0
/** Patch version number (x.x.X) */
#define HUBBLE_SDK_VERSION_PATCH 0

/**
 * Macro to convert version number into an integer
 *
 * Applications can use this value to do comparisons like HUBBLE_SDK_VERSION >=
 * HUBBLE_SDK_VERSION_VAL(1, 3, 0)
 */
#define HUBBLE_SDK_VERSION_VAL(_major, _minor, _patch)                         \
	((_major << 16) | (_minor << 8) | (_patch))

/**
 * HubbleNetwork SDK version.
 */
#define HUBBLE_SDK_VERSION                                                     \
	HUBBLE_SDK_VERSION_VAL(HUBBLE_SDK_VERSION_MAJOR,                       \
			       HUBBLE_SDK_VERSION_MINOR,                       \
			       HUBBLE_SDK_VERSION_PATCH)

/**
 * HubbleNetwork SDK version - printable format.
 */
#define HUBBLE_SDK_VERSION_STRING "1.0.0"

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_HUBBLE_VERSION_H */
