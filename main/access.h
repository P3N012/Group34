#pragma once
/* Who may pass, and who is inside. The decision (door hours + anti-passback) is one step under a mutex. */
#include <stdbool.h>
#include <stddef.h>
#include "app_config.h"

typedef enum {
    DEC_ALLOW, DEC_UNKNOWN, DEC_ALREADY_INSIDE, DEC_NOT_INSIDE, DEC_OUTSIDE_HOURS, DEC_BUSY
} decision_t;

typedef enum { USER_ADDED, USER_DUPLICATE, USER_TABLE_FULL } user_add_result_t;

void        access_init(void);                       /* mutex, load users from flash, start the flash-saving task */
decision_t  access_decide(reader_t reader, const char *uid, char *name_out, size_t name_n);
const char *access_decision_text(decision_t d);
void        access_revert(const char *uid, reader_t reader);   /* undo a decision whose door request could not be queued */
bool        access_schedule_open(void);              /* are the doors open right now? */
int         access_count_inside(void);
void        access_print_users(bool only_inside);
user_add_result_t access_add_user(const char *uid, const char *name);
bool        access_remove_user(const char *uid);
void        access_save_now(void);
