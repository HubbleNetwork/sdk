#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include "ti_ble_config.h"
#include <ti/ble/app_util/framework/bleapputil_api.h>
#include <ti/drivers/AESCMAC.h>
#include <ti/drivers/AESCTR.h>
#include <ti/drivers/cryptoutils/cryptokey/CryptoKeyPlaintext.h>

#include <hubble/ble.h>
#include <hubble/hubble_port.h>
#include <hubble/port/crypto.h>

#define BLE_ADV_LEN      31
#define AESCMAC_INSTANCE 0
#define AESCTR_INSTANCE  0

void hubble_crypto_zeroize(void *buf, size_t len)
{
	memset(buf, 0, len);
}

int hubble_crypto_cmac(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
		       const uint8_t *input, size_t input_len,
		       uint8_t output[HUBBLE_AES_BLOCK_SIZE])
{
	int ret;
	AESCMAC_Handle handle;
	AESCMAC_Params params;
	AESCMAC_Operation operation;
	CryptoKey cryptoKey;
	int16_t verificationResult;
	uint8_t input_aligned[BLE_ADV_LEN] ALIGNED;

	AESCMAC_Params_init(&params);
	params.operationalMode = AESCMAC_OPMODE_CMAC;

	handle = AESCMAC_open(AESCMAC_INSTANCE, &params);
	if (handle == NULL) {
		/* TODO: Log errror here */
		return -1;
	}

	memcpy(input_aligned, input, input_len);

	AESCMAC_Operation_init(&operation);
	operation.input = (uint8_t *)input_aligned;
	operation.inputLength = input_len;
	operation.mac = output;
	operation.macLength = HUBBLE_AES_BLOCK_SIZE;

	CryptoKeyPlaintext_initKey(&cryptoKey, (uint8_t *)key,
				   CONFIG_HUBBLE_KEY_SIZE);
	ret = AESCMAC_oneStepSign(handle, &operation, &cryptoKey);
	AESCMAC_close(handle);

	hubble_crypto_zeroize(input_aligned, input_len);

	return ret;
}

int hubble_crypto_aes_ctr(const uint8_t key[CONFIG_HUBBLE_KEY_SIZE],
			  uint8_t nonce_counter[HUBBLE_BLE_NONCE_BUFFER_LEN],
			  const uint8_t *data, size_t len, uint8_t *output)
{
	int ret;
	CryptoKey cryptoKey;
	AESCTR_Handle handle;
	AESCTR_OneStepOperation operation;
	uint8_t output_aligned[BLE_ADV_LEN] ALIGNED;
	uint8_t data_aligned[BLE_ADV_LEN] ALIGNED;

	if (len == 0) {
		return 0;
	}

	handle = AESCTR_open(AESCTR_INSTANCE, NULL);
	if (handle == NULL) {
		return -1;
	}

	CryptoKeyPlaintext_initKey(&cryptoKey, (uint8_t *)key,
				   CONFIG_HUBBLE_KEY_SIZE);

	memcpy(data_aligned, data, len);

	AESCTR_OneStepOperation_init(&operation);
	operation.key = &cryptoKey;
	operation.input = data_aligned;
	operation.inputLength = len;
	operation.output = output_aligned;
	operation.initialCounter = nonce_counter;

	ret = AESCTR_oneStepEncrypt(handle, &operation);
	AESCTR_close(handle);

	memcpy(output, output_aligned, len);

	hubble_crypto_zeroize(data_aligned, len);

	return ret;
}

int hubble_crypto_init(void)
{
	AESCTR_init();
	AESCMAC_init();

	return 0;
}
