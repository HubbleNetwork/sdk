zephyr_include_directories(
	../
	../../../
)

if(CONFIG_BT_LL_SOFTDEVICE)
	zephyr_library_sources(../nrf_softdevice.c)
else()
	zephyr_library_sources(nrf53_soc.c)
endif()

add_library(libhubble_sat_nrf53 STATIC IMPORTED GLOBAL)
set_target_properties(libhubble_sat_nrf53 PROPERTIES
	IMPORTED_LOCATION ${HUBBLE_BLOBS_DIR}/nrf53/${HUBBLE_BLOBS_ABI}/libhubble_sat_nrf53.a
)
zephyr_link_libraries(libhubble_sat_nrf53)

if(CONFIG_BOARD_THINGY53_NRF5340_CPUNET)

	zephyr_include_directories(${ZEPHYR_BASE}/subsys/bluetooth/controller/ll_sw/nordic/hal/nrf5/radio/)
	zephyr_include_directories(../nrf52/)
	# Thingy53 uses nrf21540 fem. Let's just use the
	# implementation in nrf52 for now, but if it is
	# probably worth moving it to a common location.
	zephyr_library_sources_ifndef(CONFIG_MPSL_FEM_API_AVAILABLE ../nrf52/fem.c)
	zephyr_library_sources(thingy53.c)
endif()


