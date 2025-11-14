set(SDK_BASE_DIR ../../../)

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
        "esp_timer"
    SRCS
        "${SDK_BASE_DIR}/port/esp-idf/hubble_esp_idf.c"
        "${SDK_BASE_DIR}/port/freertos/hubble_freertos.c"
        "${SDK_BASE_DIR}/src/crypto/mbedtls.c"
        "${SDK_BASE_DIR}/src/hubble_ble.c"
)

component_compile_options(${EXTRA_C_FLAGS_LIST})

target_compile_definitions(${COMPONENT_LIB} PRIVATE CONFIG_ESP_IDF_BUILD)
target_compile_definitions(${COMPONENT_LIB} PUBLIC WITH_POSIX)
