#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "app_config.h"
#include "cli.h"
#include "access.h"
#include "doors.h"
#include "event_log.h"
#include "rooms.h"
#include "rtc_clock.h"
#include "settings.h"
#include "util.h"

/* ---------- admin session ---------- */
static bool admin;
static int64_t admin_last_ms, lock_until_ms;
static int login_fails;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static bool is_admin(void)
{
    if (admin && now_ms() - admin_last_ms > SESSION_MS) { admin = false; printf("(admin session expired)\n"); }
    return admin;
}

/* Used by every command that changes settings. Refuses (and logs) when nobody is logged in. */
static bool require_admin(void)
{
    if (!is_admin()) {
        printf("Admin only. Use: login <password>\n");
        log_event("AUTH", "", "", "CONFIG_DENIED");
        return false;
    }
    admin_last_ms = now_ms();                 /* any admin command keeps the session alive */
    return true;
}

static void cmd_help(int argc, char **argv);

/* ---------- commands everyone may use ---------- */
static void cmd_status(int argc, char **argv)
{
    (void)argc; (void)argv;
    char ts[24], a[8], b[8];
    settings_t c = settings_get();
    time_string(ts, sizeof ts);
    util_format_hhmm(c.sched_start, a, sizeof a);
    util_format_hhmm(c.sched_end, b, sizeof b);
    printf("Time     : %s (%s)\n", ts, clock_is_valid() ? (clock_rtc_present() ? "RTC" : "software clock") : "NOT SET - use settime");
    printf("Schedule : %s - %s, doors %s now\n", a, b, access_schedule_open() ? "OPEN" : "LOCKED");
    printf("Inside   : %d person(s)\n", access_count_inside());
    printf("SD log   : %s, %u record(s) dropped\n", log_sd_ok() ? "OK" : "NOT AVAILABLE", (unsigned)log_dropped_count());
    printf("Lights   : off %u s after last motion\n", (unsigned)c.light_timeout_s);
    printf("Admin    : %s\n", is_admin() ? "logged in" : "not logged in");
}

static void cmd_users(int argc, char **argv)
{
    (void)argc;
    access_print_users(argv[0][0] == 'i');        /* "inside" shows only people in the building */
}

static void cmd_rooms(int argc, char **argv) { (void)argc; (void)argv; rooms_print(); }

static void cmd_log(int argc, char **argv) { log_print_last(argc > 1 ? atoi(argv[1]) : 10); }

static void cmd_time(int argc, char **argv)
{
    (void)argc; (void)argv;
    char ts[24];
    time_string(ts, sizeof ts);
    printf("%s\n", ts);
}

static void cmd_schedule(int argc, char **argv)
{
    if (argc == 1) {                              /* viewing is public */
        settings_t c = settings_get();
        char a[8], b[8];
        util_format_hhmm(c.sched_start, a, sizeof a);
        util_format_hhmm(c.sched_end, b, sizeof b);
        printf("Doors open %s - %s (start == end means always open)\n", a, b);
        return;
    }
    if (!require_admin()) return;                 /* changing it is not */
    uint16_t s, e;
    if (argc != 3 || !util_parse_hhmm(argv[1], &s) || !util_parse_hhmm(argv[2], &e)) {
        printf("usage: schedule HH:MM HH:MM\n");
        return;
    }
    settings_set_schedule(s, e);
    char detail[40];
    int n = snprintf(detail, sizeof detail, "SCHEDULE %s-%s", argv[1], argv[2]);
    (void)n;
    log_event("ADMIN", "", "admin", detail);
    printf("Schedule set to %s - %s\n", argv[1], argv[2]);
}

/* ---------- login ---------- */
static void cmd_login(int argc, char **argv)
{
    if (argc != 2) { printf("usage: login <password>\n"); return; }
    if (now_ms() < lock_until_ms) { printf("Locked. Try again in %d s\n", (int)((lock_until_ms - now_ms()) / 1000) + 1); return; }
    if (settings_check_password(argv[1])) {
        admin = true; admin_last_ms = now_ms(); login_fails = 0;
        log_event("AUTH", "", "admin", "LOGIN_OK");
        printf("Logged in. Session ends after 2 minutes without commands.\n");
    } else {
        log_event("AUTH", "", "admin", "LOGIN_FAILED");
        printf("Wrong password\n");
        if (++login_fails >= 3) { lock_until_ms = now_ms() + LOCKOUT_MS; login_fails = 0; printf("Too many attempts: locked for 30 s\n"); }
    }
}

static void cmd_logout(int argc, char **argv)
{
    (void)argc; (void)argv;
    admin = false;
    printf("Logged out\n");
}

/* ---------- admin-only commands ---------- */
static void cmd_settime(int argc, char **argv)
{
    int y, mo, d, h, mi, s;
    if (argc != 3 || !util_parse_datetime(argv[1], argv[2], &y, &mo, &d, &h, &mi, &s)) {
        printf("usage: settime YYYY-MM-DD HH:MM[:SS]\n");
        return;
    }
    if (!clock_set(y, mo, d, h, mi, s)) printf("(warning: could not write the RTC chip)\n");
    log_event("ADMIN", "", "admin", "TIME_SET");
    char ts[24];
    time_string(ts, sizeof ts);
    printf("Time set to %s\n", ts);
}

static void cmd_adduser(int argc, char **argv)
{
    char uid[UID_LEN];
    if (argc != 3 || !util_normalize_uid(argv[1], uid, sizeof uid)) { printf("usage: adduser <hex uid, e.g. AABBCCDD> <name>\n"); return; }
    user_add_result_t r = access_add_user(uid, argv[2]);
    if (r == USER_DUPLICATE) { printf("That tag is already registered\n"); return; }
    if (r == USER_TABLE_FULL) { printf("User table full (%d)\n", MAX_USERS); return; }
    log_event("ADMIN", uid, argv[2], "USER_ADDED");
    printf("Added %s (%s)\n", argv[2], uid);
}

static void cmd_deluser(int argc, char **argv)
{
    char uid[UID_LEN];
    if (argc != 2 || !util_normalize_uid(argv[1], uid, sizeof uid)) { printf("usage: deluser <hex uid>\n"); return; }
    if (!access_remove_user(uid)) { printf("No such tag\n"); return; }
    log_event("ADMIN", uid, "", "USER_REMOVED");
    printf("Removed %s\n", uid);
}

static void cmd_lighttimeout(int argc, char **argv)
{
    int s = argc == 2 ? atoi(argv[1]) : 0;
    if (s < 1 || s > 3600) { printf("usage: lighttimeout <1-3600 seconds>\n"); return; }
    settings_set_light_timeout((uint32_t)s);
    log_event("ADMIN", "", "admin", "LIGHT_TIMEOUT_CHANGED");
    printf("Lights now switch off %d s after the last motion\n", s);
}

static void cmd_passwd(int argc, char **argv)
{
    if (argc != 2 || strlen(argv[1]) < 6) { printf("usage: passwd <new password, at least 6 characters>\n"); return; }
    settings_set_password(argv[1]);
    log_event("ADMIN", "", "admin", "PASSWORD_CHANGED");
    printf("Password changed\n");
}

static void cmd_emergency(int argc, char **argv)
{
    (void)argc; (void)argv;
    doors_release_all();
    log_event("ADMIN", "", "admin", "EMERGENCY_RELEASE");
    printf("Both doors released\n");
}

static void cmd_reboot(int argc, char **argv)
{
    (void)argc; (void)argv;
    access_save_now();
    printf("Rebooting...\n");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}

/* ---------- command table and dispatcher ---------- */
typedef struct { const char *name; const char *usage; bool admin_only; void (*fn)(int, char **); } command_t;
static const command_t commands[] = {
    { "help",         "help",                               false, cmd_help },
    { "status",       "status",                             false, cmd_status },
    { "inside",       "inside                (who is in the building)", false, cmd_users },
    { "users",        "users                 (all registered tags)",    false, cmd_users },
    { "rooms",        "rooms                 (occupancy, lights, energy)", false, cmd_rooms },
    { "log",          "log [n]               (last n access records, max 20)", false, cmd_log },
    { "time",         "time",                               false, cmd_time },
    { "schedule",     "schedule [HH:MM HH:MM] (view; changing needs admin)", false, cmd_schedule },
    { "login",        "login <password>",                   false, cmd_login },
    { "logout",       "logout",                             false, cmd_logout },
    { "settime",      "settime YYYY-MM-DD HH:MM[:SS]",      true,  cmd_settime },
    { "adduser",      "adduser <hex uid> <name>",           true,  cmd_adduser },
    { "deluser",      "deluser <hex uid>",                  true,  cmd_deluser },
    { "lighttimeout", "lighttimeout <seconds>",             true,  cmd_lighttimeout },
    { "passwd",       "passwd <new password>",              true,  cmd_passwd },
    { "emergency",    "emergency             (open both doors)", true, cmd_emergency },
    { "reboot",       "reboot",                             true,  cmd_reboot },
};
#define N_COMMANDS (sizeof commands / sizeof commands[0])

static void cmd_help(int argc, char **argv)
{
    (void)argc; (void)argv;
    printf("Commands (* = admin only, use login first):\n");
    for (size_t i = 0; i < N_COMMANDS; i++) printf("  %s %s\n", commands[i].admin_only ? "*" : " ", commands[i].usage);
}

void cli_run_command(char *line)
{
    char *argv[6];
    int argc = 0;
    for (char *p = strtok(line, " \t"); p && argc < 6; p = strtok(NULL, " \t")) argv[argc++] = p;
    if (argc == 0) return;
    for (size_t i = 0; i < N_COMMANDS; i++) {
        if (strcmp(argv[0], commands[i].name) != 0) continue;
        if (commands[i].admin_only && !require_admin()) return;
        commands[i].fn(argc, argv);
        return;
    }
    printf("Unknown command '%s'. Type help\n", argv[0]);
}

/* ---------- console task ---------- */
static void cli_task(void *arg)
{
    (void)arg;
    char line[80];
    size_t len = 0;
    uint8_t c;
    printf("\nCoCIS access control ready. Type help\n> ");
    for (;;) {
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (c == '\r' || c == '\n') {
            printf("\n");
            line[len] = '\0';
            len = 0;
            cli_run_command(line);
            printf("> ");
        } else if ((c == 0x08 || c == 0x7F) && len > 0) {
            len--;
            printf("\b \b");
        } else if (c >= 0x20 && c < 0x7F && len < sizeof line - 1) {
            line[len++] = (char)c;
            printf("%c", c);
        }
        fflush(stdout);
    }
}

void cli_init(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0));
    xTaskCreate(cli_task, "cli", 6144, NULL, 3, NULL);
}
