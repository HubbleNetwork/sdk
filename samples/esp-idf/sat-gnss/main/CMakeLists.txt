set(SOURCES
    "main.c"
    "gnss_manager.c"
    "nmea_parser.c"
)

idf_component_register(
    SRCS "${SOURCES}"
    INCLUDE_DIRS "."
)
