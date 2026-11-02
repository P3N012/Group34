idf_component_register(
    SRCS "main.c" "settings.c" "rtc_clock.c" "event_log.c" "access.c" "doors.c" "rooms.c" "rfid.c" "cli.c" "util.c" "sha256.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES abobija__rc522 driver nvs_flash fatfs sdmmc esp_driver_sdspi esp_driver_i2c esp_driver_ledc esp_driver_uart esp_timer
)
