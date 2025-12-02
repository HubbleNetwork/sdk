/*
 * Copyright (c) 2025 Hubble Network, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <FreeRTOS.h>
#include <stdint.h>
#include <task.h>

#include <ti/drivers/Power.h>
#include <ti/devices/DeviceFamily.h>

#include "ti_ble_config.h"
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/stack_util/health_toolkit/assert.h"

#ifndef USE_DEFAULT_USER_CFG
#include "ti/ble/app_util/config/ble_user_config.h"
// BLE user defined configuration
icall_userCfg_t user0Cfg = BLE_USER_CFG;
#endif // USE_DEFAULT_U

#include "ti/ble/app_util/framework/bleapputil_api.h"

#include <hubble/hubble.h>

#include "key.c"
#include "utc.c"

#if !defined(HUBBLE_KEY_SET) || !defined(HUBBLE_UTC_SET)
#error "Key and UTC time must be set. Run ./embed_key_utc_.py first !"
#endif

BLEAppUtil_GeneralParams_t appMainParams = {
	.taskPriority = 1,
	.taskStackSize = 4096,
	.profileRole = (BLEAppUtil_Profile_Roles_e)(HOST_CONFIG),
	.addressMode = DEFAULT_ADDRESS_MODE,
	.deviceNameAtt = attDeviceName,
	.pDeviceRandomAddress = pRandomAddress,
};

bStatus_t hubble_ble_adv_start(void);

static BLEAppUtil_PeriCentParams_t appMainPeriCentParams;

void criticalErrorHandler(int32 errorCode, void *pInfo)
{
	(void)errorCode;
	(void)pInfo;
}

void App_StackInitDoneHandler(gapDeviceInitDoneEvent_t *deviceInitDoneData)
{
	int err;
	bStatus_t status;

	(void)deviceInitDoneData;

	err = hubble_init(utc_time, master_key);
	if (err != 0) {
		return;
	}

	status = hubble_ble_adv_start();
	if (status != SUCCESS) {
		/* TODO: Call Error Handler */
	}
}

int main()
{
	Board_init();

	/* Update User Configuration of the stack */
	user0Cfg.appServiceInfo->timerTickPeriod = ICall_getTickPeriod();
	user0Cfg.appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();

	BLEAppUtil_init(&criticalErrorHandler, &App_StackInitDoneHandler,
			&appMainParams, &appMainPeriCentParams);

	/* Start the FreeRTOS scheduler */
	vTaskStartScheduler();

	return 0;
}
