/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <pthread.h>

/* FreeRTOS */
#include <FreeRTOS.h>
#include <task.h>

/* TI Drivers */
#include <ti/drivers/Board.h>

/* Hubble Network */
#include <hubble/hubble.h>
#include <hubble/sat/packet.h>

/* Stack size in bytes */
#define THREADSTACKSIZE 2048

#define SLEEP_PERIOD_MS 1000

#if defined(HUBBLE_KEY_TIME_SET)
#include "key.c"
#include "time.c"
#else

#pragma message(                                                               \
	"Dummy key, replace with actual key by running ./embed_key_time.py -b <your-b64key> to set key")

static uint8_t master_key[CONFIG_HUBBLE_KEY_SIZE];

#endif

void *mainThread(void *arg0)
{
	struct hubble_sat_packet packet;
	int ret;

	/*
	 * This sample uses the device uptime counter source,
	 * so it is acceptable to give the initial time as 0.
	 */
	ret = hubble_init(0, master_key);
	if (ret != 0) {
		/* TODO: Call Error Handler */
		return NULL;
	}

	for (;;) {
		ret = hubble_sat_packet_get(&packet, NULL, 0);
		if (ret != 0) {
			/* TODO: Call Error Handler */
			return NULL;
		}

		ret = hubble_sat_packet_send(&packet,
					     HUBBLE_SAT_RELIABILITY_NORMAL);
		if (ret != 0) {
			/* TODO: Call Error Handler */
			return NULL;
		}
		vTaskDelay(pdMS_TO_TICKS(SLEEP_PERIOD_MS));
	}

	return NULL;
}

int main(void)
{
	pthread_t thread;
	pthread_attr_t attrs;
	struct sched_param priParam;
	int retc;

	/* initialize the system locks */
	Board_init();

	/* Initialize the attributes structure with default values */
	pthread_attr_init(&attrs);

	/* Set priority, detach state, and stack size attributes */
	priParam.sched_priority = 1;
	retc = pthread_attr_setschedparam(&attrs, &priParam);
	retc |= pthread_attr_setdetachstate(&attrs, PTHREAD_CREATE_DETACHED);
	retc |= pthread_attr_setstacksize(&attrs, THREADSTACKSIZE);
	if (retc != 0) {
		/* failed to set attributes */
		while (1) {
		}
	}

	retc = pthread_create(&thread, &attrs, mainThread, NULL);
	if (retc != 0) {
		/* pthread_create() failed */
		while (1) {
		}
	}

	/* Start the FreeRTOS scheduler */
	vTaskStartScheduler();

	return (0);
}
