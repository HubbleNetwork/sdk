zephyr_include_directories(
	../
	../../../
)

zephyr_library_sources(radio.c)

if(CONFIG_SOC_SILABS_XG24)
    zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/xg24)

    zephyr_library_sources_ifdef(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1
        ${CMAKE_CURRENT_LIST_DIR}/xg24/rail_config_v1.c
    )
elseif(CONFIG_SOC_SILABS_XG26)
    zephyr_library_include_directories(${CMAKE_CURRENT_LIST_DIR}/xg26)

    zephyr_library_sources_ifdef(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1
        ${CMAKE_CURRENT_LIST_DIR}/xg26/rail_config_v1.c
    )
else()
    message(FATAL_ERROR "SOC is not supported.")
endif()
