set(SDK_BASE_DIR ../../../)

set(SRCS
    "${SDK_BASE_DIR}/port/esp-idf/hubble_esp_idf.c"
    "${SDK_BASE_DIR}/port/freertos/hubble_freertos.c"
    "${SDK_BASE_DIR}/src/hubble.c"
)

if(CONFIG_HUBBLE_BLE_NETWORK)
    list(APPEND SRCS
        "${SDK_BASE_DIR}/src/hubble_ble.c"
        "${SDK_BASE_DIR}/src/crypto/mbedtls.c"
    )
endif()

if(CONFIG_HUBBLE_SAT_NETWORK)
    list(APPEND SRCS
        "${SDK_BASE_DIR}/src/hubble_sat.c"
        "${SDK_BASE_DIR}/src/hubble_sat_packet.c"
        "${SDK_BASE_DIR}/src/hubble_sat_ephemeris.c"
    )
endif()

idf_component_register(
    INCLUDE_DIRS
        "${SDK_BASE_DIR}/include"
        "${SDK_BASE_DIR}/port/esp-idf"
        "${SDK_BASE_DIR}/port/freertos"
    PRIV_INCLUDE_DIRS
    REQUIRES
    PRIV_REQUIRES
        "mbedtls"
        "freertos"
        "log"
    SRCS
        ${SRCS}
)

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_ESP_IDF_BUILD)
target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
