/*
 * Smart Control and Monitoring System - CoCIS pilot (one ESP32, ESP-IDF, Wokwi)
 *
 * app_main only starts the modules, in dependency order. Where things live:
 *   app_config.h  pins and limits            settings.c   door hours, light timeout, admin password (flash)
 *   rtc_clock.c   DS1307 clock               access.c     who is allowed / who is inside (mutex)
 *   event_log.c   SD card log (1 writer)     doors.c      servos + LEDs, one task per door
 *   rooms.c       PIR -> relay -> lights     rfid.c       SPI bus + the two RC522 readers
 *   cli.c         serial commands + login    util.c       pure helpers (tested on a PC)
 */
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "app_config.h"
#include "settings.h"
#include "access.h"
#include "rtc_clock.h"
#include "event_log.h"
#include "doors.h"
#include "rooms.h"
#include "rfid.h"
#include "cli.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    /* 1. flash storage, then the data that lives in it */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    settings_init();
    access_init();

    /* 2. the shared SPI bus must exist before the SD card or a reader attaches to it */
    if (spi_bus_setup() != ESP_OK) { ESP_LOGE(TAG, "SPI bus failed"); 
        return; }
    ESP_LOGI(TAG, "SPI bus ready");
    esp_log_level_set("rc522", ESP_LOG_NONE);
    deselect_other_devices();

    /* 3. time, then the log (log lines carry timestamps), then everything that writes to the log */
    clock_init();
    log_init();
    doors_init();
    rooms_init();
    cli_init();
    log_event("SYSTEM", "", "", "BOOT");

    /* 4. readers last: they are the only things that produce scans */
    readers_start();
}
