zephyr_include_directories(../../include)

zephyr_library()

zephyr_library_sources(hubble_zephyr.c)

zephyr_library_sources_ifdef(CONFIG_HUBBLE_SAT_NETWORK
	../../src/utils/bitarray.c
	../../src/hubble_sat.c
	../../src/hubble_sat_packet.c
	../../src/reed_solomon_encoder.c
	hubble_sat_zephyr.c
)

if(CONFIG_HUBBLE_BLE_NETWORK)
	zephyr_library_sources(../../src/hubble_ble.c)
	zephyr_library_sources_ifdef(CONFIG_HUBBLE_BLE_NETWORK_MBEDTLS hubble_ble_zephyr.c)
	zephyr_library_sources_ifdef(CONFIG_HUBBLE_BLE_NETWORK_PSA hubble_ble_zephyr_psa.c)
	zephyr_library_link_libraries(mbedTLS)
endif()
