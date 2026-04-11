#ifndef ECHO_UTIL_H
#define ECHO_UTIL_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

echo_error_t echo_read_file(const char *path, uint8_t **out_data,
                            size_t *out_len);

echo_error_t echo_write_file(const char *path, const uint8_t *data, size_t len);

void echo_secure_zero(void *ptr, size_t len);

#endif
