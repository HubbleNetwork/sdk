#include <psa/crypto.h>

#ifdef CONFIG_PSA_CRYPTO_DRIVER_CRACEN
#include <cracen_psa_kmu.h>

static const void *master_key = &(psa_key_id_t){PSA_KEY_ID_FROM_CRACEN_KMU_SLOT(
	CRACEN_KMU_KEY_USAGE_SCHEME_PROTECTED, PSA_KEY_ID_USER_MIN)};
#else
#error "Not supported key storage backend."
#endif
