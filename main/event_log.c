#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "app_config.h"
#include "event_log.h"
#include "rtc_clock.h"

static const char *TAG = "LOG";

typedef struct { char line[120]; } log_rec_t;

static QueueHandle_t log_queue;
static SemaphoreHandle_t sd_mutex;          /* protects the log file: logger writes, console reads */
static bool sd_ok;
static uint32_t dropped;

static esp_err_t sd_mount(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mc = {
        .format_if_mount_failed = true,       /* simulator card starts blank */
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = APP_SPI_HOST;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = SD_CS_PIN;
    slot.host_id = APP_SPI_HOST;
    sdmmc_card_t *card;
    return esp_vfs_fat_sdspi_mount(SD_MOUNT, &host, &slot, &mc, &card);
}

/* Anyone may call this. It only queues a line and never waits. */
void log_event(const char *type, const char *uid, const char *name, const char *detail)
{
    log_rec_t r;
    char ts[24];
    time_string(ts, sizeof ts);
    int n = snprintf(r.line, sizeof r.line, "%s,%s,%s,%s,%s", ts, type,
                     uid ? uid : "", name ? name : "", detail ? detail : "");
    (void)n;
    if (xQueueSend(log_queue, &r, 0) != pdTRUE) dropped++;
}

static void logger_task(void *arg)
{
    (void)arg;
    log_rec_t r;
    for (;;) {
        xQueueReceive(log_queue, &r, portMAX_DELAY);
        if (!sd_ok) { dropped++; continue; }
        if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) { dropped++; continue; }
        FILE *f = fopen(LOG_PATH, "a");
        if (f) { fprintf(f, "%s\n", r.line); fclose(f); }
        else   { dropped++; ESP_LOGE(TAG, "cannot open %s", LOG_PATH); }
        xSemaphoreGive(sd_mutex);
    }
}

void log_init(void)
{
    sd_mutex = xSemaphoreCreateMutex();
    log_queue = xQueueCreate(16, sizeof(log_rec_t));
    sd_ok = (sd_mount() == ESP_OK);
    if (sd_ok) ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT);
    else       ESP_LOGE(TAG, "SD card NOT available - events are not being stored");
    xTaskCreate(logger_task, "logger", 6144, NULL, 3, NULL);
}

bool log_sd_ok(void) { return sd_ok; }
uint32_t log_dropped_count(void) { return dropped; }

void log_print_last(int n)
{
    static char ring[20][120];
    if (n < 1) n = 1;
    if (n > 20) n = 20;
    if (!sd_ok) { printf("SD card not available\n"); return; }
    if (xSemaphoreTake(sd_mutex, pdMS_TO_TICKS(2000)) != pdTRUE) { printf("busy\n"); return; }
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) { xSemaphoreGive(sd_mutex); printf("(log is empty)\n"); return; }
    int total = 0;
    while (fgets(ring[total % 20], sizeof ring[0], f)) total++;
    fclose(f);
    xSemaphoreGive(sd_mutex);
    int start = total > n ? total - n : 0;
    for (int i = start; i < total; i++) printf("  %s", ring[i % 20]);
    if (total == 0) printf("(log is empty)\n");
}
