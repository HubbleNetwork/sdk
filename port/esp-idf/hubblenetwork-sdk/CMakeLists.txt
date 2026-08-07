set(SDK_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/../../..)

set(SRCS
    "${SDK_BASE_DIR}/port/esp-idf/hubble_esp_idf.c"
    "${SDK_BASE_DIR}/port/freertos/hubble_freertos.c"
    "${SDK_BASE_DIR}/src/hubble.c"
    "${SDK_BASE_DIR}/src/hubble_crypto.c"
)

if(IDF_VERSION_MAJOR LESS 6)
    list(APPEND SRCS
        "${SDK_BASE_DIR}/src/crypto/mbedtls.c"
    )
else()
    list(APPEND SRCS
        "${SDK_BASE_DIR}/src/crypto/psa.c"
    )
endif()

set(INC_DIRS
    "${SDK_BASE_DIR}/include"
    "${SDK_BASE_DIR}/src"
    "${SDK_BASE_DIR}/port/esp-idf"
    "${SDK_BASE_DIR}/port/freertos"
)

if(CONFIG_HUBBLE_BLE_NETWORK)
    list(APPEND SRCS
        "${SDK_BASE_DIR}/src/hubble_ble.c"
    )
endif()

if(CONFIG_HUBBLE_SAT_NETWORK)
    list(APPEND SRCS
        "${SDK_BASE_DIR}/port/freertos/hubble_sat_freertos.c"
        "${SDK_BASE_DIR}/src/hubble_sat.c"
        "${SDK_BASE_DIR}/src/hubble_sat_pass_prediction.c"
        "${SDK_BASE_DIR}/src/utils/bitarray.c"
        "${SDK_BASE_DIR}/src/reed_solomon_encoder.c"
    )

    if(CONFIG_HUBBLE_SAT_NETWORK_PROTOCOL_V1)
        list(APPEND SRCS
            "${SDK_BASE_DIR}/src/hubble_sat_packet.c"
        )
    endif()

    # add board support
    if(IDF_TARGET STREQUAL "esp32c6")
        list(APPEND INC_DIRS
            "${CMAKE_CURRENT_LIST_DIR}/boards/esp32c6"
        )
        list(APPEND SRCS
            "${CMAKE_CURRENT_LIST_DIR}/boards/esp32c6/radio.c"
        )
    endif()
endif()

idf_component_register(
    INCLUDE_DIRS
        ${INC_DIRS}
    PRIV_INCLUDE_DIRS
    REQUIRES
    PRIV_REQUIRES
        "mbedtls"
        "freertos"
        "log"
        "esp_phy"
        "esp_driver_gptimer"
    SRCS
        ${SRCS}
)

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_ESP_IDF_BUILD)
target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
