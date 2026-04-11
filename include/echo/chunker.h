#ifndef ECHO_CHUNKER_H
#define ECHO_CHUNKER_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

typedef struct echo_chunked_file {
  uint8_t **chunks;
  size_t *chunk_sizes;
  size_t chunk_count;
} echo_chunked_file_t;

echo_error_t echo_chunk_file(const uint8_t *data, size_t data_len,
                             size_t chunk_size,
                             echo_chunked_file_t *out_chunked);

void echo_chunked_file_free(echo_chunked_file_t *chunked);

#endif
