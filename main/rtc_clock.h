#pragma once
/* Time: DS1307 RTC on I2C feeds the ESP32 system clock. Everything else just asks for the time. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void clock_init(void);                       /* set up the RTC chip and copy its time into the system clock */
bool clock_rtc_present(void);
bool clock_is_valid(void);                   /* false until a sensible date/time has been set */
bool clock_minutes_now(uint16_t *minutes);   /* minutes since midnight; false if the clock is not set */
bool clock_set(int y, int mo, int d, int h, int mi, int s);   /* system clock + RTC; false if the RTC write failed */
void time_string(char *out, size_t n);       /* "YYYY-MM-DD HH:MM:SS" */
