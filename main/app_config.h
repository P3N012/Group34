#pragma once
/* Pins, limits and shared constants. If the wiring in diagram.json changes, change it here and nowhere else. */

/* ---- SPI bus shared by both RFID readers and the SD card ---- */
#define SPI_MOSI_PIN      23
#define SPI_MISO_PIN      19
#define SPI_SCLK_PIN      18
#define RFID_ENTRY_CS_PIN  5
#define RFID_EXIT_CS_PIN  17
#define SD_CS_PIN         16
#define RFID_RST_PIN       4          /* shared reset wire: only the entry reader pulses it */
#define APP_SPI_HOST      SPI3_HOST   /* our pins are the default pins of SPI3 (VSPI) */

/* ---- doors: servo + green/red LED per door ---- */
#define SERVO_ENTRY_PIN   25
#define SERVO_EXIT_PIN    26
#define LED_ENTRY_GREEN_PIN 32
#define LED_ENTRY_RED_PIN   33
#define LED_EXIT_GREEN_PIN  27
#define LED_EXIT_RED_PIN    14
#define SERVO_CLOSED_US  500          /* door closed, arm at about 0 degrees  */
#define SERVO_OPEN_US   1470          /* door open, arm at about 90 degrees   */
#define DOOR_OPEN_MS    3000           /*keep ddor open for 3s*/
#define DOOR_DENY_MS    1000          /*keep the red led on for 1s*/

/* ---- lecture rooms ---- */
#define PIR1_PIN    34                /* input-only pins, fine for sensors */
#define PIR2_PIN    35
#define RELAY1_PIN  12                /* GPIO 12 is a boot-strapping pin: move this relay if the board will not boot */
#define RELAY2_PIN  13
#define BTN1_PIN     2
#define BTN2_PIN    15
#define RELAY_ACTIVE_LEVEL 1          /* flip to 0 if the relay switches the wrong way round */

/* ---- clock (DS1307 on I2C) ---- */
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define RTC_ADDR    0x68

/* ---- storage ---- */
#define SD_MOUNT  "/sdcard"
#define LOG_PATH  SD_MOUNT "/access.log"
#define NVS_NS    "cocis"

/* ---- limits ---- */
#define MAX_USERS   10
#define NAME_LEN    16
#define UID_LEN     30                /* 10-byte UID as "AA BB ..." + NUL (checked against the RC522 library in rfid.c) */
#define SESSION_MS  120000            /* admin session ends after 2 minutes without commands */
#define LOCKOUT_MS  30000             /* login locked for 30 s after 3 wrong passwords */

typedef enum { READER_ENTRY, READER_EXIT } reader_t;
