#pragma once
/* The two doors: servo + green/red LED each. One task and one queue per door. */
#include <stdbool.h>
#include "app_config.h"

void doors_init(void);                                         /* LEDs, servos (both start closed), queues, door tasks */
bool doors_request(reader_t door, const char *uid, bool open); /* never waits; false if the door's queue is full */
void doors_release_all(void);                                  /* emergency: open both doors */
