/*
 * Copyright (c) 2025 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/host/gap/gap_advertiser.h"
#include "ti/drivers/dpl/ClockP.h"
#include "ti/drivers/dpl/SemaphoreP.h"
#include "ti/drivers/dpl/TaskP.h"

#include <hubble/ble.h>

#define BLE_ADV_LEN 31
#define HUBBLE_BLE_ADV_HEADER                                                  \
	0x03, GAP_ADTYPE_16BIT_COMPLETE, LO_UINT16(0xfca6), HI_UINT16(0xfca6), \
		0x01, GAP_ADTYPE_SERVICE_DATA,
#define HUBBLE_BLE_ADV_HEADER_SIZE 6

/* Period to update adv packets in microseconds */
#define HUBBLE_ADV_PACKET_PERIOD   180000000UL

static uint8 bleAdvHandle;
static uint8_t advData[BLE_ADV_LEN] = {HUBBLE_BLE_ADV_HEADER};
static ClockP_Handle clockHandle;
static ClockP_Struct clockStruct;
static ClockP_Params clockParams;

/* Adv Task */
static TaskP_Struct taskStruct;
static TaskP_Handle taskHandle;
static uint8_t advStack[2048];
static TaskP_Params taskParams = {
	.stackSize = 2048,
	.stack = advStack,
};

static SemaphoreP_Struct semaphoreStruct;
static SemaphoreP_Handle semaphoreHandle;

static GapAdv_params_t advParams = {
	.eventProps = GAP_ADV_PROP_LEGACY | GAP_ADV_PROP_SCANNABLE,
	.primIntMin = 160,
	.primIntMax = 160,
	.primChanMap = GAP_ADV_CHAN_ALL,
	.peerAddrType = PEER_ADDRTYPE_RANDOM_OR_RANDOM_ID,
	.filterPolicy = GAP_ADV_AL_POLICY_ANY_REQ,
	.txPower = GAP_ADV_TX_POWER_NO_PREFERENCE,
	.primPhy = GAP_ADV_PRIM_PHY_1_MBPS,
	.secPhy = GAP_ADV_SEC_PHY_1_MBPS,
	.sid = 0};

const BLEAppUtil_AdvInit_t hubbleInitAdvSet = {
	/* Advertise data and length */
	.advDataLen = sizeof(advData),
	.advData = advData,

	/* Scan respond data and length */
	.scanRespDataLen = 0,
	.scanRespData = NULL,
	.advParam = &advParams};

const BLEAppUtil_AdvStart_t hubbleStartAdvSet = {
	/* Use the maximum possible value. This is the spec-defined maximum for */
	/* directed advertising and infinite advertising for all other types */
	.enableOptions = GAP_ADV_ENABLE_OPTIONS_USE_MAX,
	.durationOrMaxEvents = 0};

static void _timer_cb(uintptr_t arg)
{
	SemaphoreP_post(semaphoreHandle);
}

static int _adv_timer_setup(void)
{
	ClockP_Params_init(&clockParams);

	clockParams.period =
		HUBBLE_ADV_PACKET_PERIOD / ClockP_getSystemTickPeriod();

	clockHandle = ClockP_construct(&clockStruct, _timer_cb, 0, &clockParams);
	if (clockHandle == NULL) {
		return (FAILURE);
	}

	semaphoreHandle = SemaphoreP_constructBinary(&semaphoreStruct, 0);
	if (semaphoreHandle == NULL) {
		ClockP_destruct(&clockStruct);
		return (FAILURE);
	}

	return (SUCCESS);
}

static void hubble_ble_adv_update(void *arg)
{
	(void)arg;

	size_t len = BLE_ADV_LEN - HUBBLE_BLE_ADV_HEADER_SIZE;
	int status = hubble_ble_advertise_get(
		NULL, 0, &advData[HUBBLE_BLE_ADV_HEADER_SIZE], &len);

	if (status) {
		return;
	}

	advData[4] = len + 1; /* output len + BLE service data type */

	if (BLEAppUtil_advStop(bleAdvHandle) != SUCCESS) {
		return;
	}

	if (BLEAppUtil_initAdvSet(&bleAdvHandle, &hubbleInitAdvSet) != SUCCESS) {
		return;
	}

	(void)BLEAppUtil_advStart(bleAdvHandle, &hubbleStartAdvSet);
}

static void hubble_adv_entry(void *arg)
{
	while (1) {
		(void)SemaphoreP_pend(semaphoreHandle, SemaphoreP_WAIT_FOREVER);

		/* Start / Stop advertise must be done form BLEAppUtil module context */
		if (BLEAppUtil_invokeFunction(
			    (InvokeFromBLEAppUtilContext_t)hubble_ble_adv_update,
			    NULL) != SUCCESS) {
			break;
		}
	}
}

bStatus_t hubble_ble_adv_start(void)
{
	bStatus_t status = SUCCESS;
	size_t len;
	uint8_t *data;

	if (_adv_timer_setup() == FAILURE) {
		return (FAILURE);
	}

	len = BLE_ADV_LEN - HUBBLE_BLE_ADV_HEADER_SIZE;
	if (hubble_ble_advertise_get(
		    NULL, 0, &advData[HUBBLE_BLE_ADV_HEADER_SIZE], &len) != 0) {
		return (FAILURE);
	}

	memcpy(&advData[HUBBLE_BLE_ADV_HEADER_SIZE], data, len);
	advData[4] = len + 1; /* output len + BLE service data type */

	status = BLEAppUtil_initAdvSet(&bleAdvHandle, &hubbleInitAdvSet);
	if (status != SUCCESS) {
		return (status);
	}

	status = BLEAppUtil_advStart(bleAdvHandle, &hubbleStartAdvSet);
	if (status != SUCCESS) {
		return (status);
	}

	taskHandle = TaskP_construct(&taskStruct, hubble_adv_entry, &taskParams);
	if (taskHandle == NULL) {
		return (FAILURE);
	}

	ClockP_start(clockHandle);

	return (status);
}
