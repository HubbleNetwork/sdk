/*
 * Copyright (c) 2025 HubbleNetwork
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Standard C Libraries */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

/* FreeRTOS */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

/* TODO: check this config */
/* TI Drivers */
#include <ti/drivers/RCL.h>
#include <ti/drivers/rcl/RCL_Scheduler.h>
#include <ti/drivers/rcl/commands/generic.h>

/* Hubble */
#include <hubble/port/sat_radio.h>
#include <hubble/sat/packet.h>

/* DMM */
#if defined(USE_DMM_OVRDE)
#include <dmm_scheduler.h>
#include <dmm_policy.h>
#include <ti_dmm_application_policy.h>
#include "dmm_priority_ble_custom.h"
#endif

/* SysConfig Generated */
#include "ti_drivers_config.h"
#include "ti_radio_config.h"

#include "pa.h"
#include "sat_board.h"

#define TI_STEP_SIZE_HZ                 366.2119

/* The center frequency for channel 0 is 2482208625
 * -> f_base = 2482208625 - (32 * 366.2119) = 2482196906
 * Each channel is 25.75 kHz
 * --> offset = 25.75k / 366.2119 ~= 70 steps (round)
 */
#define HUBBLE_BASE_FREQUENCY           2482196906UL
#define HUBBLE_CHANNEL_OFFSET(_channel) ((_channel) * 70)

#define HUBBLE_SYMBOL_PERIOD                                                   \
	RCL_SCHEDULER_SYSTIM_US(                                               \
		HUBBLE_WAIT_SYMBOL_US + HUBBLE_WAIT_SYMBOL_OFF_US)

/**
 * This semaphore is used to protect a packet transmission and avoid
 * race conditions.
 */
static SemaphoreHandle_t _transmit_sem;

/* Current sat power */
static int8_t _sat_power = TI_PA_DEFAULT_DBM;

/* RCL */
#if defined(USE_DMM_OVRDE)
/* BLE RCL Client Object */
extern RCL_Client rfClient;
extern RCL_CmdGenericTxTest rclPacketTxCmdGenericTxTest_ble_gen_3;
#define rclPacketTxCmdGenericTxTest_ble_custom                                 \
	rclPacketTxCmdGenericTxTest_ble_gen_3
#else
extern RCL_CmdGenericTxTest rclPacketTxCmdGenericTxTest_ble_gen_0;
#define rclPacketTxCmdGenericTxTest_ble_custom                                 \
	rclPacketTxCmdGenericTxTest_ble_gen_0
#endif

static RCL_Client rcl_client;
static RCL_Handle rcl_handle;

static void _radio_cw_start(int16_t step, uint32_t abs_start, uint32_t duration_us)
{
	/* On time */
	rclPacketTxCmdGenericTxTest_ble_custom.txPower.dBm = _sat_power;
	rclPacketTxCmdGenericTxTest_ble_custom.rfFrequency =
		(uint32_t)(HUBBLE_BASE_FREQUENCY + step * TI_STEP_SIZE_HZ);
	rclPacketTxCmdGenericTxTest_ble_custom.common.timing.relHardStopTime =
		RCL_SCHEDULER_SYSTIM_US(duration_us);
	rclPacketTxCmdGenericTxTest_ble_custom.common.allowDelay = false;

	/* Submit command & pend on completion */
	rclPacketTxCmdGenericTxTest_ble_custom.common.timing.absStartTime =
		abs_start;
#if defined(USE_DMM_OVRDE)
	DMMSch_RCL_Command_submit(rcl_handle,
				  &rclPacketTxCmdGenericTxTest_ble_custom);
	DMMSch_RCL_Command_pend(&rclPacketTxCmdGenericTxTest_ble_custom);
#else
	RCL_Command_submit(rcl_handle, &rclPacketTxCmdGenericTxTest_ble_custom);
	RCL_Command_pend(&rclPacketTxCmdGenericTxTest_ble_custom);
#endif
}

#if defined(USE_DMM_OVRDE)
static int hubble_dmm_init(void)
{
	/* Initialize DMM Scheduler */
	DMMPolicy_Params dmm_policy_params;
	DMMPolicy_Status policy_ret;

	/* Initial Policy Manager for tests that require priority handling */
	DMMPolicy_init();
	DMMPolicy_Params_init(&dmm_policy_params);

	dmm_policy_params.numPolicyTableEntries = DMMPolicy_ApplicationPolicySize;
	dmm_policy_params.policyTable = DMMPolicy_ApplicationPolicyTable;
	dmm_policy_params.globalPriorityTable = globalPriorityTable_bleCustom;

	policy_ret = DMMPolicy_open(&dmm_policy_params);
	if (policy_ret != DMMPolicy_StatusSuccess) {
		return -EAGAIN;
	}

	/* Initialize DMM scheduler */
	if (!DMMSch_init()) {
		return -EAGAIN;
	}

	return ((DMMSch_registerClient(&rcl_client, DMMPolicy_StackRole_Custom1,
				       DMMPolicy_Id_Custom1) == true) &&
		(DMMSch_registerClient(&rfClient, DMMPolicy_StackRole_BlePeripheral,
				       DMMPolicy_Id_Ble) == true))
		       ? 0
		       : -EAGAIN;
}
#endif

static inline int _board_init(void)
{
	int ret = 0;

#if defined(USE_DMM_OVRDE)
	DMMSch_RCL_init();
	ret = hubble_dmm_init();
	if (ret != 0) {
		return ret;
	}

	rcl_handle = DMMSch_RCL_open(&rcl_client, &LRF_config_ble_gen_3);
#else
	RCL_init();
	rcl_handle = RCL_open(&rcl_client, &LRF_config_ble_gen_0);
#endif

	if (rcl_handle == NULL) {
		return -EIO;
	}

	/* Set RF frequency */
	rclPacketTxCmdGenericTxTest_ble_custom.rfFrequency =
		HUBBLE_BASE_FREQUENCY;
	rclPacketTxCmdGenericTxTest_ble_custom.txPower.dBm = _sat_power;

	/* Start command as soon as possible */
	rclPacketTxCmdGenericTxTest_ble_custom.common.scheduling =
		RCL_Schedule_AbsTime;
	rclPacketTxCmdGenericTxTest_ble_custom.common.status =
		RCL_CommandStatus_Idle;

	/* No whitening, on repeated word, freq synth off after cmd, enable cw */
	rclPacketTxCmdGenericTxTest_ble_custom.config.whitenMode = 0U;
	rclPacketTxCmdGenericTxTest_ble_custom.config.txWord = 0U;
	rclPacketTxCmdGenericTxTest_ble_custom.config.fsOff = 0U;
	rclPacketTxCmdGenericTxTest_ble_custom.config.sendCw = 1U;

	return ret;
}

#ifdef CONFIG_HUBBLE_FREERTOS_DAEMON_HOOK
void vApplicationDaemonTaskStartupHook(void)
{
	(void)_board_init();
}
#endif

/* TODO: make sure that the init is only called once during a lifetime of the application */
int hubble_sat_board_init(void)
{
	_transmit_sem = xSemaphoreCreateBinary();
	if (_transmit_sem == NULL) {
		return -ENOMEM;
	}

	/* Make count to 1 initially */
	if (xSemaphoreGive(_transmit_sem) != pdTRUE) {
		vSemaphoreDelete(_transmit_sem);

		_transmit_sem = NULL;

		return -EAGAIN;
	}

#ifndef CONFIG_HUBBLE_FREERTOS_DAEMON_HOOK
	return _board_init();
#else
	/* If the radio was properly configured, we must have a valid RCL handle here. */
	return rcl_handle != NULL ? 0 : -EIO;
#endif
}

int hubble_sat_board_enable(void)
{
#if defined(USE_DMM_OVRDE)
	DMMSch_setBlockModeOn(DMMPolicy_StackRole_BlePeripheral);
	DMMSch_setBlockModeOff(DMMPolicy_StackRole_Custom1);

	/* Block wait for completion */
	while (DMMSch_getBlockModeStatus(DMMPolicy_StackRole_Custom1)) {
	};
#endif

	return 0;
}

int hubble_sat_board_disable(void)
{
#if defined(USE_DMM_OVRDE)
	DMMSch_setBlockModeOn(DMMPolicy_StackRole_Custom1);
	DMMSch_setBlockModeOff(DMMPolicy_StackRole_BlePeripheral);

	/* Block wait for completion */
	while (DMMSch_getBlockModeStatus(DMMPolicy_StackRole_BlePeripheral)) {
	};
#endif

	/* TODO: should we close and open rcl every time or just once during init? */

	return 0;
}

int hubble_sat_board_packet_send(const struct hubble_sat_packet_frames *packet)
{
	uint32_t symbol_start;
	int8_t frame = -1;

	xSemaphoreTake(_transmit_sem, portMAX_DELAY);

	symbol_start = RCL_Scheduler_getCurrentTime() +
		       RCL_SCHEDULER_SYSTIM_US(HUBBLE_WAIT_SYMBOL_OFF_US);

	for (size_t i = 0; i < packet->total_number_of_symbols; i++) {
		int16_t step;
		uint8_t data_pos = i % HUBBLE_PACKET_FRAME_PAYLOAD_MAX_SIZE;

		if (data_pos == 0) {
			frame++;
		}

		step = packet->frame[frame].data[data_pos] +
		       HUBBLE_CHANNEL_OFFSET(packet->frame[frame].channel);

		_radio_cw_start(step, symbol_start, HUBBLE_WAIT_SYMBOL_US);
		symbol_start += HUBBLE_SYMBOL_PERIOD;
	}

	xSemaphoreGive(_transmit_sem);
	return 0;
}

#ifdef CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE

int hubble_sat_board_power_set(int8_t power)
{
	_sat_power = power;
	return 0;
}

int hubble_sat_board_cw_start(uint8_t channel)
{
	int16_t step = 32 + HUBBLE_CHANNEL_OFFSET(channel);
	rclPacketTxCmdGenericTxTest_ble_custom.rfFrequency =
		(uint32_t)(HUBBLE_BASE_FREQUENCY + step * TI_STEP_SIZE_HZ);

	/* Because TI is a bit special, let's have the cw run for 10s */
	rclPacketTxCmdGenericTxTest_ble_custom.common.timing.relHardStopTime =
		RCL_SCHEDULER_SYSTIM_US(10000000);

	rclPacketTxCmdGenericTxTest_ble_custom.txPower.dBm = _sat_power;

	/* We don't need abs time here */
	rclPacketTxCmdGenericTxTest_ble_custom.common.scheduling =
		RCL_Schedule_Now;

	/* Submit command & pend on completion */
	RCL_Command_submit(rcl_handle, &rclPacketTxCmdGenericTxTest_ble_custom);
	RCL_Command_pend(&rclPacketTxCmdGenericTxTest_ble_custom);

	/* Reset back to abs time in case the next ops is packet tx */
	rclPacketTxCmdGenericTxTest_ble_custom.common.scheduling =
		RCL_Schedule_AbsTime;

	return 0;
}

int hubble_sat_board_cw_stop(void)
{
	return -ENOTSUP;
}

#endif /* CONFIG_HUBBLE_SAT_NETWORK_DTM_MODE */
