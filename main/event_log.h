#pragma once
/* Event log on the SD card. Any task may call log_event(); ONE logger task does all the SD writing. */
#include <stdbool.h>
#include <stdint.h>

void     log_init(void);                     /* mount the SD card (failure is not fatal) and start the logger task */
void     log_event(const char *type, const char *uid, const char *name, const char *detail);
bool     log_sd_ok(void);
uint32_t log_dropped_count(void);            /* records lost because the SD card was missing or the queue was full */
void     log_print_last(int n);              /* print the last n records (1..20) to the console */
