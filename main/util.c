#include "util.h"
#include <stdio.h>
#include <ctype.h>

bool util_parse_hhmm(const char *s, uint16_t *minutes)
{
    int h, m;
    char extra;
    if (!s || !minutes) return false;
    if (sscanf(s, "%d:%d%c", &h, &m, &extra) != 2) return false;   /* exactly "H:M", nothing after */
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
    *minutes = (uint16_t)(h * 60 + m);
    return true;
}

void util_format_hhmm(uint16_t minutes, char *out, size_t n)
{
    int len = snprintf(out, n, "%02u:%02u", (unsigned)(minutes / 60), (unsigned)(minutes % 60));
    (void)len;
}

/* start < end : open from start up to (not including) end
 * start > end : overnight window, e.g. 22:00 -> 06:00
 * start == end: treated as "always open" (24 hours) */
bool util_window_contains(uint16_t start, uint16_t end, uint16_t now)
{
    if (start == end) return true;
    if (start < end)  return now >= start && now < end;
    return now >= start || now < end;
}

bool util_normalize_uid(const char *in, char *out, size_t n)
{
    char hex[21];
    size_t cnt = 0;
    if (!in || !out) return false;
    for (const char *p = in; *p; p++) {
        if (*p == ' ' || *p == ':' || *p == '-') continue;
        if (!isxdigit((unsigned char)*p)) return false;
        if (cnt >= 20) return false;
        hex[cnt++] = (char)toupper((unsigned char)*p);
    }
    if (cnt != 8 && cnt != 14 && cnt != 20) return false;          /* 4, 7 or 10 byte UIDs */
    size_t bytes = cnt / 2;
    if (n < bytes * 3) return false;
    size_t o = 0;
    for (size_t i = 0; i < bytes; i++) {
        if (i) out[o++] = ' ';
        out[o++] = hex[2 * i];
        out[o++] = hex[2 * i + 1];
    }
    out[o] = '\0';
    return true;
}

uint8_t util_bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
uint8_t util_bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool util_parse_datetime(const char *date, const char *time_str,
                         int *y, int *mo, int *d, int *h, int *mi, int *s)
{
    char extra;
    int yy, mm, dd, hh, mn, ss = 0;
    if (!date || !time_str) return false;
    if (sscanf(date, "%d-%d-%d%c", &yy, &mm, &dd, &extra) != 3) return false;
    if (sscanf(time_str, "%d:%d:%d%c", &hh, &mn, &ss, &extra) != 3) {
        ss = 0;
        if (sscanf(time_str, "%d:%d%c", &hh, &mn, &extra) != 2) return false;
    }
    if (yy < 2000 || yy > 2099 || mm < 1 || mm > 12 || dd < 1 || dd > 31) return false;
    if (hh < 0 || hh > 23 || mn < 0 || mn > 59 || ss < 0 || ss > 59) return false;
    *y = yy; *mo = mm; *d = dd; *h = hh; *mi = mn; *s = ss;
    return true;
}

void util_copy_str(char *dst, size_t n, const char *src)
{
    if (n == 0) return;
    size_t l = 0;
    while (src[l] && l < n - 1) { dst[l] = src[l]; l++; }
    dst[l] = '\0';
}
