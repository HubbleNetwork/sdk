/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Master key: cd:15:a5:ab:c0:60:b6:72:88:a6:1e:44:e9:95:ba:77:
 *             d1:40:bd:46:56:4b:88:de:41:c1:5a:92:73:b0:ce:85
 */

#include "test_vectors.h"

/* Test Vector 1: Empty payload */
static const uint8_t tv1_payload[] = {};

static const uint8_t tv1_expected[] = {0xa6, 0xfc, 0x00, 0x00, 0x60, 0xdb,
				       0x85, 0x95, 0x8f, 0xd7, 0x43, 0x9c};

/* Test Vector 2: Single byte */
static const uint8_t tv2_payload[] = {0xaa};

static const uint8_t tv2_expected[] = {0xa6, 0xfc, 0x00, 0x01, 0x60, 0xdb, 0x85,
				       0x95, 0xd2, 0x1b, 0xb5, 0x71, 0x82};

/* Test Vector 3: Hello world */
static const uint8_t tv3_payload[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};

static const uint8_t tv3_expected[] = {0xa6, 0xfc, 0x00, 0x64, 0x60, 0xdb,
				       0x85, 0x95, 0xa2, 0xa4, 0xc7, 0x70,
				       0x8a, 0x6d, 0xc7, 0x2a, 0x6b};

/* Test Vector 4: Binary pattern */
static const uint8_t tv4_payload[] = {0xde, 0xad, 0xbe, 0xef};

static const uint8_t tv4_expected[] = {0xa6, 0xfc, 0x00, 0xff, 0x60, 0xdb,
				       0x85, 0x95, 0x75, 0xe6, 0x93, 0xea,
				       0x75, 0x6f, 0x58, 0x7d};

/* Test Vector 5: All zeros */
static const uint8_t tv5_payload[] = {0x00, 0x00, 0x00, 0x00,
				      0x00, 0x00, 0x00, 0x00};

static const uint8_t tv5_expected[] = {0xa6, 0xfc, 0x01, 0x00, 0x60, 0xdb, 0x85,
				       0x95, 0xff, 0x87, 0x32, 0xc0, 0x65, 0x0e,
				       0x09, 0x37, 0x25, 0x84, 0x70, 0x61};

/* Test Vector 6: All ones */
static const uint8_t tv6_payload[] = {0xff, 0xff, 0xff, 0xff,
				      0xff, 0xff, 0xff, 0xff};

static const uint8_t tv6_expected[] = {0xa6, 0xfc, 0x02, 0x00, 0x60, 0xdb, 0x85,
				       0x95, 0x8b, 0x85, 0x45, 0x1e, 0x22, 0x66,
				       0x39, 0xc4, 0x3f, 0x4a, 0x7c, 0x5f};

/* Test Vector 7: Max length ASCII */
static const uint8_t tv7_payload[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57,
				      0x6f, 0x72, 0x6c, 0x64, 0x21, 0x21};

static const uint8_t tv7_expected[] = {0xa6, 0xfc, 0x03, 0xff, 0x60, 0xdb, 0x85,
				       0x95, 0x8b, 0x21, 0x17, 0x2f, 0xb4, 0xb9,
				       0x85, 0x35, 0x9a, 0xe4, 0xce, 0x1a, 0xa0,
				       0x8b, 0xe5, 0xe3, 0x73};

/* Test Vector 8: Max length binary */
static const uint8_t tv8_payload[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
				      0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c};

static const uint8_t tv8_expected[] = {0xa6, 0xfc, 0x00, 0x00, 0xc9, 0xf3, 0x09,
				       0xbc, 0x4b, 0xeb, 0x66, 0xb6, 0xef, 0xf3,
				       0x09, 0x0d, 0xdc, 0x7b, 0x38, 0x94, 0x93,
				       0xf8, 0x40, 0x53, 0x28};

/* Test Vector 9: Mid-length */
static const uint8_t tv9_payload[] = {0x54, 0x65, 0x73, 0x74, 0x31, 0x32, 0x33};

static const uint8_t tv9_expected[] = {0xa6, 0xfc, 0x01, 0xf4, 0xa1, 0x08, 0x77,
				       0x49, 0x39, 0x8c, 0x87, 0x9d, 0x3e, 0xed,
				       0xb3, 0x9f, 0xb4, 0xdc, 0x79};

/* Test Vector 10: Numeric sequence */
static const uint8_t tv10_payload[] = {0x01, 0x02, 0x03, 0x04, 0x05,
				       0x06, 0x07, 0x08, 0x09, 0x0a};

static const uint8_t tv10_expected[] = {
	0xa6, 0xfc, 0x00, 0x2a, 0xd6, 0x1e, 0xa0, 0x75, 0xb2, 0x34, 0x5b,
	0xf5, 0x5f, 0xb7, 0x38, 0x5d, 0xe0, 0x56, 0x94, 0xce, 0x4f, 0x35};

const struct ble_adv_test_vector test_vectors[] = {
	{.description = "Empty payload",
	 .time_counter = 20,
	 .seq_no = 0,
	 .payload = tv1_payload,
	 .payload_len = 0,
	 .expected = tv1_expected,
	 .expected_len = sizeof(tv1_expected)},
	{.description = "Single byte",
	 .time_counter = 20,
	 .seq_no = 1,
	 .payload = tv2_payload,
	 .payload_len = sizeof(tv2_payload),
	 .expected = tv2_expected,
	 .expected_len = sizeof(tv2_expected)},
	{.description = "Hello world",
	 .time_counter = 20,
	 .seq_no = 100,
	 .payload = tv3_payload,
	 .payload_len = sizeof(tv3_payload),
	 .expected = tv3_expected,
	 .expected_len = sizeof(tv3_expected)},
	{.description = "Binary pattern",
	 .time_counter = 20,
	 .seq_no = 255,
	 .payload = tv4_payload,
	 .payload_len = sizeof(tv4_payload),
	 .expected = tv4_expected,
	 .expected_len = sizeof(tv4_expected)},
	{.description = "All zeros",
	 .time_counter = 20,
	 .seq_no = 256,
	 .payload = tv5_payload,
	 .payload_len = sizeof(tv5_payload),
	 .expected = tv5_expected,
	 .expected_len = sizeof(tv5_expected)},
	{.description = "All ones",
	 .time_counter = 20,
	 .seq_no = 512,
	 .payload = tv6_payload,
	 .payload_len = sizeof(tv6_payload),
	 .expected = tv6_expected,
	 .expected_len = sizeof(tv6_expected)},
	{.description = "Max length ASCII",
	 .time_counter = 20,
	 .seq_no = 1023,
	 .payload = tv7_payload,
	 .payload_len = sizeof(tv7_payload),
	 .expected = tv7_expected,
	 .expected_len = sizeof(tv7_expected)},
	{.description = "Max length binary",
	 .time_counter = 1,
	 .seq_no = 0,
	 .payload = tv8_payload,
	 .payload_len = sizeof(tv8_payload),
	 .expected = tv8_expected,
	 .expected_len = sizeof(tv8_expected)},
	{.description = "Mid-length",
	 .time_counter = 1000,
	 .seq_no = 500,
	 .payload = tv9_payload,
	 .payload_len = sizeof(tv9_payload),
	 .expected = tv9_expected,
	 .expected_len = sizeof(tv9_expected)},
	{.description = "Numeric sequence",
	 .time_counter = 5000,
	 .seq_no = 42,
	 .payload = tv10_payload,
	 .payload_len = sizeof(tv10_payload),
	 .expected = tv10_expected,
	 .expected_len = sizeof(tv10_expected)},
};

const size_t test_vectors_count = sizeof(test_vectors) / sizeof(test_vectors[0]);
