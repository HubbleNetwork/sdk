zephyr_include_directories(../../include)

zephyr_library()

if (CONFIG_HUBBLE_SAT_NETWORK OR CONFIG_HUBBLE_BLE_NETWORK)
	zephyr_library_sources(hubble_zephyr.c)
	zephyr_library_sources(../../src/hubble.c)
	zephyr_library_sources_ifdef(CONFIG_HUBBLE_BLE_NETWORK_MBEDTLS ../../src/crypto/mbedtls.c)
	zephyr_library_sources_ifdef(CONFIG_HUBBLE_BLE_NETWORK_PSA ../../src/crypto/psa.c)
	if (CONFIG_HUBBLE_BLE_NETWORK_PSA OR CONFIG_HUBBLE_BLE_NETWORK_MBEDTLS)
		zephyr_library_link_libraries(mbedTLS)
	endif()
endif()

if(CONFIG_HUBBLE_SAT_NETWORK)
	zephyr_library_sources(../../src/utils/bitarray.c)
	zephyr_library_sources(../../src/hubble_sat_ephemeris.c)
	zephyr_library_sources(../../src/hubble_sat.c)
	zephyr_library_sources(../../src/hubble_sat_packet.c)
	zephyr_library_sources(../../src/reed_solomon_encoder.c)
	zephyr_library_sources(hubble_sat_zephyr.c)
	zephyr_include_directories(.)
endif()


if(CONFIG_HUBBLE_BLE_NETWORK)
	zephyr_library_sources(../../src/hubble_ble.c)
endif()
