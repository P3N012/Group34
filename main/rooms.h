#pragma once
/* The two lecture rooms: PIR sensor -> relay -> lights, a "hold" button, and an energy-saving estimate. */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    const char *name;
    int pir, relay, btn;                       /* GPIO numbers */
    bool occupied, light_on, hold, btn_prev; /*state flags***/
    int64_t last_motion_ms, on_since_ms, on_total_ms, btn_ms;
} room_t;

void    rooms_init(void);                      /* GPIO setup, mutex, and the room task */
void    rooms_print(void);                     /* status */

/* Exposed so the logic can be unit tested on a PC. The caller holds the rooms mutex (or is a single-threaded test). */
void    room_update(room_t *r, int64_t now, int motion, int btn, int64_t timeout_ms, bool *toggled, bool *changed);
room_t *rooms_get(int index);
