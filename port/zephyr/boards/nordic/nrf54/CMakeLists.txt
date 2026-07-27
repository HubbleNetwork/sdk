zephyr_include_directories(
	../
	../../../
)

if(CONFIG_BT_LL_SOFTDEVICE)
	zephyr_library_sources(../nrf_softdevice.c)
else()
	zephyr_library_sources(nrf54_soc.c)
endif()

add_library(libhubble_sat_nrf54 STATIC IMPORTED GLOBAL)
set_target_properties(libhubble_sat_nrf54 PROPERTIES
	IMPORTED_LOCATION ${HUBBLE_BLOBS_DIR}/nrf54/${HUBBLE_BLOBS_ABI}/libhubble_sat_nrf54.a
)
zephyr_link_libraries(libhubble_sat_nrf54)

if(CONFIG_BOARD_NRF54L15DK)
	zephyr_library_sources(nrf54l15dk.c)
endif()

if(CONFIG_BOARD_NRF54L15TAG)
	zephyr_library_sources(nrf54l15tag.c)
endif()