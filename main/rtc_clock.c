#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "app_config.h"
#include "rtc_clock.h"
#include "util.h"

static const char *TAG = "CLOCK";
static i2c_master_dev_handle_t rtc_dev;
static bool rtc_ok;

static esp_err_t rtc_setup(void)
{
    i2c_master_bus_handle_t bus;
    i2c_master_bus_config_t bc = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    esp_err_t err = i2c_new_master_bus(&bc, &bus);
    if (err != ESP_OK) return err;
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_ADDR,
        .scl_speed_hz = 100000,
    };
    return i2c_master_bus_add_device(bus, &dc, &rtc_dev);
}

static esp_err_t rtc_read(struct tm *out)
{
    uint8_t reg = 0, b[7];
    esp_err_t err = i2c_master_transmit_receive(rtc_dev, &reg, 1, b, sizeof b, 200);
    if (err != ESP_OK) return err;
    if (b[0] & 0x80) return ESP_ERR_INVALID_STATE;          /* clock-halt bit set: RTC never started */
    memset(out, 0, sizeof *out);
    out->tm_sec  = util_bcd2bin(b[0] & 0x7F);
    out->tm_min  = util_bcd2bin(b[1] & 0x7F);
    out->tm_hour = util_bcd2bin(b[2] & 0x3F);               /* 24-hour mode */
    out->tm_mday = util_bcd2bin(b[4] & 0x3F);
    out->tm_mon  = util_bcd2bin(b[5] & 0x1F) - 1;
    out->tm_year = util_bcd2bin(b[6]) + 100;                /* years since 1900 */
    return ESP_OK;
}

static esp_err_t rtc_write(const struct tm *t)
{
    uint8_t b[8];
    b[0] = 0;                                               /* start at register 0 */
    b[1] = util_bin2bcd((uint8_t)t->tm_sec) & 0x7F;         /* bit 7 = 0 starts the clock */
    b[2] = util_bin2bcd((uint8_t)t->tm_min);
    b[3] = util_bin2bcd((uint8_t)t->tm_hour);
    b[4] = util_bin2bcd((uint8_t)(t->tm_wday + 1));
    b[5] = util_bin2bcd((uint8_t)t->tm_mday);
    b[6] = util_bin2bcd((uint8_t)(t->tm_mon + 1));
    b[7] = util_bin2bcd((uint8_t)(t->tm_year - 100));
    return i2c_master_transmit(rtc_dev, b, sizeof b, 200);
}

static void set_system_clock(const struct tm *t)
{
    struct tm c = *t;
    c.tm_isdst = 0;
    struct timeval tv = { .tv_sec = mktime(&c), .tv_usec = 0 };
    settimeofday(&tv, NULL);
}

void clock_init(void)
{
    struct tm t;
    rtc_ok = (rtc_setup() == ESP_OK);
    if (rtc_ok && rtc_read(&t) == ESP_OK && t.tm_year + 1900 >= 2024) {
        set_system_clock(&t);
        ESP_LOGI(TAG, "System clock set from RTC");
    } else {
        ESP_LOGW(TAG, "RTC time not valid - set it with: settime YYYY-MM-DD HH:MM");
    }
}

bool clock_rtc_present(void) { return rtc_ok; }

bool clock_is_valid(void)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year + 1900 >= 2024;
}

bool clock_minutes_now(uint16_t *minutes)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    if (t.tm_year + 1900 < 2024) return false;
    *minutes = (uint16_t)(t.tm_hour * 60 + t.tm_min);
    return true;
}

bool clock_set(int y, int mo, int d, int h, int mi, int s)
{
    struct tm t = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d,
                    .tm_hour = h, .tm_min = mi, .tm_sec = s, .tm_isdst = 0 };
    time_t tt = mktime(&t);                 /* normalises the struct and fills in tm_wday */
    struct tm n;
    localtime_r(&tt, &n);
    set_system_clock(&n);
    return !(rtc_ok && rtc_write(&n) != ESP_OK);
}

void time_string(char *out, size_t n)
{
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    size_t r = strftime(out, n, "%Y-%m-%d %H:%M:%S", &t);
    if (r == 0 && n) out[0] = '\0';
}
