#pragma once
/* Pure helper functions: no ESP-IDF dependencies, so they can be unit tested on a PC. */
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool    util_parse_hhmm(const char *s, uint16_t *minutes);                /* "08:30" -> 510            */
void    util_format_hhmm(uint16_t minutes, char *out, size_t n);          /* 510 -> "08:30"           */
bool    util_window_contains(uint16_t start, uint16_t end, uint16_t now); /* door-hours check         */
bool    util_normalize_uid(const char *in, char *out, size_t n);          /* "aabbccdd" -> "AA BB CC DD" */
uint8_t util_bcd2bin(uint8_t v);
uint8_t util_bin2bcd(uint8_t v);
bool    util_parse_datetime(const char *date, const char *time_str,
                            int *y, int *mo, int *d, int *h, int *mi, int *s);
void    util_copy_str(char *dst, size_t n, const char *src);              /* always NUL-terminated copy */
