#pragma once
/* Settings kept in flash (NVS): door hours, light timeout, admin password (salted SHA-256 hash). */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t sched_start;           /* minutes since midnight */
    uint16_t sched_end;
    uint32_t light_timeout_s;       /* lights off this long after the last motion */
    char salt[17];
    char hash[65];                  /* SHA-256 hex of salt+password */
} settings_t;

void       settings_init(void);                       /* creates the mutex, loads from flash, creates default admin password on first boot */
settings_t settings_get(void);                        /* a consistent copy (taken under the mutex) */
void       settings_set_schedule(uint16_t start, uint16_t end);
void       settings_set_light_timeout(uint32_t seconds);
void       settings_set_password(const char *pw);
bool       settings_check_password(const char *pw);
