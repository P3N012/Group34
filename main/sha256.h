#pragma once
#include <stddef.h>
#include <stdint.h>

void sha256(const uint8_t *data, size_t len, uint8_t out[32]);
void sha256_hex(const char *str, char out[65]);   /* hex digest of a C string */
