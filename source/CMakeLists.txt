idf_component_register(SRCS "app_main.c" "adc.c" "application.c" "backends.c" "bigpacks.c" "bigpostman.c" "ble.c" "board.c" "devices.c" "enums.c" "framer.c" "i2c.c" "logs.c" "measurements.c" "onewire.c" "pbuf.c" "sha256.c" "hmac.c" "wifi.c" "yuarel.c" INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-error=unused-value")