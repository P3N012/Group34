#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "app_config.h"
#include "settings.h"
#include "sha256.h"
#include "util.h"

static const char *TAG = "SETTINGS";
static settings_t cfg;
static SemaphoreHandle_t cfg_mutex;

static void store_u32(const char *key, uint32_t v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) { nvs_set_u32(h, key, v); nvs_commit(h); nvs_close(h); }
}

static void store_str(const char *key, const char *v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) { nvs_set_str(h, key, v); nvs_commit(h); nvs_close(h); }
}

static void hash_password(const char *salt, const char *pw, char out[65])
{
    char buf[96];
    int n = snprintf(buf, sizeof buf, "%s%s", salt, pw);
    (void)n;
    sha256_hex(buf, out);
}

settings_t settings_get(void)
{
    settings_t c;
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    c = cfg;
    xSemaphoreGive(cfg_mutex);
    return c;
}

void settings_set_password(const char *pw)
{
    char salt[17], hash[65];
    int n = snprintf(salt, sizeof salt, "%08x%08x", (unsigned)esp_random(), (unsigned)esp_random());
    (void)n;
    hash_password(salt, pw, hash);
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    util_copy_str(cfg.salt, sizeof cfg.salt, salt);
    util_copy_str(cfg.hash, sizeof cfg.hash, hash);
    xSemaphoreGive(cfg_mutex);
    store_str("salt", salt);
    store_str("hash", hash);
}

bool settings_check_password(const char *pw)
{
    settings_t c = settings_get();
    char h[65];
    hash_password(c.salt, pw, h);
    unsigned diff = 0;
    for (int i = 0; i < 64; i++) diff |= (unsigned)(h[i] ^ c.hash[i]);   /* compare without early exit */
    return diff == 0;
}

void settings_set_schedule(uint16_t start, uint16_t end)
{
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    cfg.sched_start = start;
    cfg.sched_end = end;
    xSemaphoreGive(cfg_mutex);
    store_u32("sched_start", start);
    store_u32("sched_end", end);
}

void settings_set_light_timeout(uint32_t seconds)
{
    xSemaphoreTake(cfg_mutex, portMAX_DELAY);
    cfg.light_timeout_s = seconds;
    xSemaphoreGive(cfg_mutex);
    store_u32("light_to", seconds);
}

void settings_init(void)
{
    cfg_mutex = xSemaphoreCreateMutex();
    nvs_handle_t h;
    uint32_t v;
    size_t len;
    cfg.sched_start = 6 * 60;       /* defaults: open 06:00 - 22:00 */
    cfg.sched_end = 22 * 60;
    cfg.light_timeout_s = 10;       /* short for the demo; use 600 (10 min) in a real building */
    cfg.salt[0] = cfg.hash[0] = '\0';
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_get_u32(h, "sched_start", &v) == ESP_OK) cfg.sched_start = (uint16_t)v;
        if (nvs_get_u32(h, "sched_end", &v) == ESP_OK) cfg.sched_end = (uint16_t)v;
        if (nvs_get_u32(h, "light_to", &v) == ESP_OK) cfg.light_timeout_s = v;
        len = sizeof cfg.salt;
        if (nvs_get_str(h, "salt", cfg.salt, &len) != ESP_OK) cfg.salt[0] = '\0';
        len = sizeof cfg.hash;
        if (nvs_get_str(h, "hash", cfg.hash, &len) != ESP_OK) cfg.hash[0] = '\0';
        nvs_close(h);
    }
    if (cfg.hash[0] == '\0') {      /* first boot: create the default admin password */
        settings_set_password("admin123");
        ESP_LOGW(TAG, "Default admin password is 'admin123' - change it with: passwd <new>");
    }
}
