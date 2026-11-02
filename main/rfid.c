#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/rc522_spi.h"
#include "rc522.h"
#include "esp_log.h"
#include "app_config.h"
#include "rfid.h"
#include "access.h"
#include "doors.h"
#include "event_log.h"

_Static_assert(UID_LEN >= RC522_PICC_UID_STR_BUFFER_SIZE_MAX, "UID_LEN in app_config.h is too small for the RC522 library");

static const char *TAG = "RFID";

static rc522_spi_config_t entry_config = {
    .host_id = APP_SPI_HOST,
    .bus_config = NULL,                       /* NULL = the bus already exists, do not create it again */
    .dev_config = { .spics_io_num = RFID_ENTRY_CS_PIN, .clock_speed_hz = 1000000 },
    .rst_io_num = RFID_RST_PIN,
};
static rc522_spi_config_t exit_config = {
    .host_id = APP_SPI_HOST,
    .bus_config = NULL,
    .dev_config = { .spics_io_num = RFID_EXIT_CS_PIN, .clock_speed_hz = 1000000 },
    .rst_io_num = -1,                         /* the RST wire is shared: only the entry reader may pulse it */
};
static rc522_driver_handle_t entry_driver, exit_driver;
static rc522_handle_t entry_scanner, exit_scanner;

esp_err_t spi_bus_setup(void)
{
    spi_bus_config_t config = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    return spi_bus_initialize(APP_SPI_HOST, &config, SPI_DMA_CH_AUTO);
}

void deselect_other_devices(void)
{
    gpio_reset_pin(SD_CS_PIN);
    gpio_set_direction(SD_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(SD_CS_PIN, 1);
}

/* Runs inside the RFID library's own task: decide, log, hand off. Never wait here. */
static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    (void)base; (void)event_id;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;
    if (picc->state != RC522_PICC_STATE_ACTIVE) return;
    reader_t reader = (reader_t)(intptr_t)arg;

    char uid[UID_LEN], name[NAME_LEN];
    if (rc522_picc_uid_to_str(&picc->uid, uid, sizeof uid) != ESP_OK) {
        ESP_LOGE(TAG, "Reader %d: cannot convert UID", (int)reader);
        return;
    }
    decision_t d = access_decide(reader, uid, name, sizeof name);
    bool allowed = (d == DEC_ALLOW);
    ESP_LOGI(TAG, "%s scan %s (%s): %s | inside now: %d", reader == READER_ENTRY ? "ENTRY" : "EXIT",
             uid, name[0] ? name : "unknown", access_decision_text(d), access_count_inside());
    log_event(reader == READER_ENTRY ? "ENTRY" : "EXIT", uid, name, access_decision_text(d));

    if (!doors_request(reader, uid, allowed)) {
        ESP_LOGE(TAG, "Door queue full, scan dropped");
        if (allowed) access_revert(uid, reader);              /* undo the state change */
    }
}

static void reader_start(rc522_spi_config_t *spi_config, rc522_driver_handle_t *driver,
                         rc522_handle_t *scanner, reader_t id)
{
    ESP_ERROR_CHECK(rc522_spi_create(spi_config, driver));
    ESP_ERROR_CHECK(rc522_driver_install(*driver));
    rc522_config_t sc = { .driver = *driver };
    ESP_ERROR_CHECK(rc522_create(&sc, scanner));
    ESP_ERROR_CHECK(rc522_register_events(*scanner, RC522_EVENT_PICC_STATE_CHANGED,
                                          on_picc_state_changed, (void *)(intptr_t)id));
    ESP_ERROR_CHECK(rc522_start(*scanner));
}

void readers_start(void)
{
    reader_start(&entry_config, &entry_driver, &entry_scanner, READER_ENTRY);
    reader_start(&exit_config, &exit_driver, &exit_scanner, READER_EXIT);
}
