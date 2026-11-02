#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "app_config.h"
#include "doors.h"
#include "util.h"

static const char *TAG = "DOORS";

typedef struct { int green_pin; int red_pin; int servo_pin; ledc_channel_t channel; } door_t;
static const door_t doors[2] = {
    { LED_ENTRY_GREEN_PIN, LED_ENTRY_RED_PIN, SERVO_ENTRY_PIN, LEDC_CHANNEL_0 },
    { LED_EXIT_GREEN_PIN,  LED_EXIT_RED_PIN,  SERVO_EXIT_PIN,  LEDC_CHANNEL_1 },
};

typedef struct { char uid[UID_LEN]; bool open; } door_event_t;
static QueueHandle_t door_queue[2];

static void leds_init(void)
{
    for (size_t i = 0; i < sizeof(doors) / sizeof(doors[0]); ++i) {
        gpio_reset_pin(doors[i].green_pin);
        gpio_reset_pin(doors[i].red_pin);
        gpio_set_direction(doors[i].green_pin, GPIO_MODE_OUTPUT);
        gpio_set_direction(doors[i].red_pin, GPIO_MODE_OUTPUT);
        gpio_set_level(doors[i].green_pin, 0);
        gpio_set_level(doors[i].red_pin, 0);
    }
}

static void servo_init(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    for (size_t i = 0; i < sizeof(doors) / sizeof(doors[0]); ++i) {
        ledc_channel_config_t cc = {
            .gpio_num = doors[i].servo_pin,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .channel = doors[i].channel,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&cc));
    }
}

static void servo_set_pulse_us(uint32_t pulse_us, reader_t door)
{
    uint32_t duty = (pulse_us * (1 << 14)) / 20000;           /* 20 ms period at 50 Hz */
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_HIGH_SPEED_MODE, doors[door].channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_HIGH_SPEED_MODE, doors[door].channel));
}

static void door_open(reader_t door)  { servo_set_pulse_us(SERVO_OPEN_US, door); }
static void door_close(reader_t door) { servo_set_pulse_us(SERVO_CLOSED_US, door); }

/* A denied scan at one door never waits behind an open door at the other. */
static void door_task(void *arg)
{
    reader_t id = (reader_t)(intptr_t)arg;
    door_event_t ev;
    for (;;) {
        if (xQueueReceive(door_queue[id], &ev, portMAX_DELAY) != pdTRUE) continue;
        if (ev.open) {
            ESP_LOGI(TAG, "Door %d opened for %s", (int)id, ev.uid);
            gpio_set_level(doors[id].green_pin, 1);
            door_open(id);
            vTaskDelay(pdMS_TO_TICKS(DOOR_OPEN_MS));
            door_close(id);
            gpio_set_level(doors[id].green_pin, 0);
        } else {
            gpio_set_level(doors[id].red_pin, 1);
            vTaskDelay(pdMS_TO_TICKS(DOOR_DENY_MS));
            gpio_set_level(doors[id].red_pin, 0);
        }
    }
}

bool doors_request(reader_t door, const char *uid, bool open)
{
    door_event_t ev;
    util_copy_str(ev.uid, sizeof ev.uid, uid);
    ev.open = open;
    return xQueueSend(door_queue[door], &ev, 0) == pdTRUE;
}

void doors_release_all(void)
{
    doors_request(READER_ENTRY, "EMERGENCY", true);
    doors_request(READER_EXIT, "EMERGENCY", true);
}

void doors_init(void)
{
    door_queue[READER_ENTRY] = xQueueCreate(4, sizeof(door_event_t));
    door_queue[READER_EXIT]  = xQueueCreate(4, sizeof(door_event_t));
    leds_init();
    servo_init();
    door_close(READER_ENTRY);
    door_close(READER_EXIT);
    xTaskCreate(door_task, "door0", 4096, (void *)(intptr_t)READER_ENTRY, 5, NULL);
    xTaskCreate(door_task, "door1", 4096, (void *)(intptr_t)READER_EXIT, 5, NULL);
}
