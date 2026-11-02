#pragma once
/* SPI bus + the two RC522 RFID readers. A scan goes: decide (access.c) -> log (event_log.c) -> open/deny (doors.c). */
#include "esp_err.h"

esp_err_t spi_bus_setup(void);            /* the shared bus: must be called ONCE, before anything attaches to it */
void      deselect_other_devices(void);   /* hold the SD card's chip-select high until it is mounted */
void      readers_start(void);            /* start both readers; call last, they are what produces scans */
