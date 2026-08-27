/*
 * Copyright (c) 2026 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>
#include <hubble/sat/pass_prediction.h>
#include <hubble/port/sys.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/base64.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "satellites.h"

LOG_MODULE_REGISTER(main);

#if defined(CONFIG_SAMPLE_SAT_RELIABILITY_HIGH)
#define SAMPLE_RELIABILITY HUBBLE_SAT_RELIABILITY_HIGH
#else
#define SAMPLE_RELIABILITY HUBBLE_SAT_RELIABILITY_NORMAL
#endif

/*
 * Decoded master key. The SDK stores this pointer directly, so the buffer has
 * static storage duration and outlives all SDK calls. Zero-initialised, which
 * also serves as the all-zero placeholder when no key is provided.
 */
static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];

/* Device location, parsed once at startup; read by main() and shell commands. */
static struct hubble_sat_device_pos device_pos;

/*
 * Decode the base64 master key (CONFIG_SAMPLE_HUBBLE_KEY, provisioned at build
 * time) into master_key, validating its length.
 */
static int master_key_provision(void)
{
	const char *key = CONFIG_SAMPLE_HUBBLE_KEY;
	size_t olen;
	int err;

	if (strlen(key) == 0) {
		LOG_WRN("CONFIG_SAMPLE_HUBBLE_KEY not set; using an all-zero "
			"placeholder key. Pass your base64 key with "
			"-DCONFIG_SAMPLE_HUBBLE_KEY=\"<key>\".");
		return 0;
	}

	err = base64_decode(master_key, sizeof(master_key), &olen, key,
			    strlen(key));
	if (err != 0 || olen != sizeof(master_key)) {
		LOG_ERR("Invalid CONFIG_SAMPLE_HUBBLE_KEY: expected a base64-"
			"encoded %u-byte key (matching CONFIG_HUBBLE_KEY_SIZE)",
			(unsigned int)sizeof(master_key));
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT

int hubble_sat_board_init(void)
{
	return 0;
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

#endif /* CONFIG_SAMPLE_PROVIDE_SAT_BOARD_SUPPORT */

#ifdef CONFIG_SHELL

/*
 * Telemetry published by the main loop and read by the shell commands. The
 * shell runs on its own thread, so a mutex guards against torn reads of the
 * 64-bit fields. The commands only read this cached state (and the read-only
 * hubble_time_get()/hubble_uptime_get()); they never call the non-reentrant
 * pass-prediction or transmit APIs, so they cannot race the main loop's SDK use.
 */
static struct {
	uint32_t tx_count;
	uint64_t last_tx_unix_s;          /* 0 = no transmission yet */
	struct hubble_sat_pass_info pass; /* most recently computed pass */
	bool pass_valid;
} telemetry;

static K_MUTEX_DEFINE(telemetry_lock);

static void telemetry_publish_pass(const struct hubble_sat_pass_info *pass)
{
	k_mutex_lock(&telemetry_lock, K_FOREVER);
	telemetry.pass = *pass;
	telemetry.pass_valid = true;
	k_mutex_unlock(&telemetry_lock);
}

static void telemetry_record_tx(void)
{
	k_mutex_lock(&telemetry_lock, K_FOREVER);
	telemetry.tx_count++;
	telemetry.last_tx_unix_s = hubble_time_get() / 1000ULL;
	k_mutex_unlock(&telemetry_lock);
}

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	uint32_t tx_count;
	uint64_t last_tx;
	uint64_t now_s = hubble_time_get() / 1000ULL;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	k_mutex_lock(&telemetry_lock, K_FOREVER);
	tx_count = telemetry.tx_count;
	last_tx = telemetry.last_tx_unix_s;
	k_mutex_unlock(&telemetry_lock);

	shell_print(sh, "Current time:  %llu (unix s)",
		    (unsigned long long)now_s);
	shell_print(sh, "Uptime:        %llu s",
		    (unsigned long long)(hubble_uptime_get() / 1000ULL));
	shell_print(sh, "Location:      lat=%s lon=%s deg",
		    CONFIG_SAMPLE_DEVICE_LAT, CONFIG_SAMPLE_DEVICE_LON);
	shell_print(sh, "Satellites:    %u", (unsigned int)sats_count);
	shell_print(sh, "Transmissions: %u", (unsigned int)tx_count);
	if (last_tx != 0U) {
		shell_print(sh, "Last TX:       %llu (unix s, %llu s ago)",
			    (unsigned long long)last_tx,
			    (unsigned long long)(now_s - last_tx));
	} else {
		shell_print(sh, "Last TX:       never");
	}

	return 0;
}

static int cmd_next(const struct shell *sh, size_t argc, char **argv)
{
	struct hubble_sat_pass_info pass;
	bool valid;
	uint64_t now_s = hubble_time_get() / 1000ULL;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	k_mutex_lock(&telemetry_lock, K_FOREVER);
	valid = telemetry.pass_valid;
	pass = telemetry.pass;
	k_mutex_unlock(&telemetry_lock);

	if (!valid) {
		shell_print(sh, "Next pass not yet computed");
		return 0;
	}

	shell_print(sh, "Next pass:");
	shell_print(sh, "  start:       %llu (unix s, in %llds)",
		    (unsigned long long)pass.start,
		    (long long)((int64_t)pass.start - (int64_t)now_s));
	shell_print(sh, "  culmination: %llu (unix s)",
		    (unsigned long long)pass.culmination);
	shell_print(sh, "  duration:    %u s", (unsigned int)pass.duration);
	shell_print(sh, "  max elev:    %.1f deg", pass.max_elevation_angle);
	shell_print(sh, "  longitude:   %.1f deg", pass.lon);

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sat_cmds,
	SHELL_CMD(status, NULL, "Show telemetry: time, location, transmissions",
		  cmd_status),
	SHELL_CMD(next, NULL, "Show the next predicted pass", cmd_next),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(sat, &sat_cmds, "Hubble satellite telemetry", NULL);

#else  /* !CONFIG_SHELL */

static inline void telemetry_publish_pass(const struct hubble_sat_pass_info *pass)
{
	ARG_UNUSED(pass);
}

static inline void telemetry_record_tx(void)
{
}

#endif /* CONFIG_SHELL */

int main(void)
{
	int err;

	device_pos.lat = strtod(CONFIG_SAMPLE_DEVICE_LAT, NULL);
	device_pos.lon = strtod(CONFIG_SAMPLE_DEVICE_LON, NULL);

	LOG_INF("Hubble Network Sat Pass-Prediction Sample started");

	err = master_key_provision();
	if (err != 0) {
		return err;
	}

	/*
	 * Device uptime counter source: pass 0 as the initial EID counter; it
	 * advances with uptime. No UTC needed for crypto.
	 */
	err = hubble_init(0, master_key);
	if (err != 0) {
		LOG_ERR("hubble_init failed (%d)", err);
		return err;
	}

	/*
	 * Seed the SDK's wall clock from the build timestamp (captured by CMake
	 * into SAMPLE_BUILD_UNIX_TIME, in seconds). hubble_time_set() takes
	 * milliseconds and tracks time against uptime from here, so pass
	 * prediction has absolute time even though crypto uses the uptime
	 * counter source.
	 */
	err = hubble_time_set((uint64_t)SAMPLE_BUILD_UNIX_TIME * 1000ULL);
	if (err != 0) {
		LOG_ERR("hubble_time_set failed (%d)", err);
		return err;
	}

	err = hubble_sat_satellites_set(sats, sats_count);
	if (err != 0) {
		LOG_ERR("hubble_sat_satellites_set failed (%d)", err);
		return err;
	}

	for (;;) {
		struct hubble_sat_pass_info pass;
		struct hubble_sat_packet pkt;
		/* hubble_time_get() is milliseconds; pass prediction wants seconds. */
		uint64_t now = hubble_time_get() / 1000ULL;
		int64_t wait;

		err = hubble_sat_next_pass_get(now, &device_pos, &pass);
		if (err != 0) {
			LOG_ERR("hubble_sat_next_pass_get failed (%d)", err);
			return err;
		}
		telemetry_publish_pass(&pass);

		wait = (int64_t)pass.start - (int64_t)now;
		LOG_INF("Next pass: start=%llu culmination=%llu duration=%us "
			"(in %llds)",
			(unsigned long long)pass.start,
			(unsigned long long)pass.culmination,
			(unsigned int)pass.duration, (long long)wait);

		if (wait > 0) {
			k_sleep(K_SECONDS(wait));
		}

		err = hubble_sat_packet_get(&pkt, NULL, 0);
		if (err != 0) {
			LOG_ERR("hubble_sat_packet_get failed (%d)", err);
			return err;
		}

		LOG_INF("Pass window open; transmitting packet");
		err = hubble_sat_packet_send(&pkt, SAMPLE_RELIABILITY);
		if (err != 0) {
			LOG_ERR("hubble_sat_packet_send failed (%d)", err);
			return err;
		}
		telemetry_record_tx();
	}

	return 0;
}
