#include "echo/chunker.h"

#include <stdlib.h>
#include <string.h>

echo_error_t echo_chunk_file(const uint8_t *data, size_t data_len,
                             size_t chunk_size,
                             echo_chunked_file_t *out_chunked) {
  size_t chunk_count = 0;
  size_t i = 0;

  if (!data || data_len == 0 || chunk_size == 0 || !out_chunked) {
    return ECHO_ERR_INVALID_ARG;
  }

  chunk_count = (data_len + chunk_size - 1) / chunk_size;

  out_chunked->chunks = (uint8_t **)calloc(chunk_count, sizeof(uint8_t *));
  out_chunked->chunk_sizes = (size_t *)calloc(chunk_count, sizeof(size_t));
  out_chunked->chunk_count = chunk_count;

  if (!out_chunked->chunks || !out_chunked->chunk_sizes) {
    echo_chunked_file_free(out_chunked);
    return ECHO_ERR_NOMEM;
  }

  for (i = 0; i < chunk_count; i++) {
    size_t offset = i * chunk_size;
    size_t remaining = data_len - offset;
    size_t current_size = remaining < chunk_size ? remaining : chunk_size;

    out_chunked->chunks[i] = (uint8_t *)malloc(current_size);
    if (!out_chunked->chunks[i]) {
      echo_chunked_file_free(out_chunked);
      return ECHO_ERR_NOMEM;
    }

    memcpy(out_chunked->chunks[i], data + offset, current_size);
    out_chunked->chunk_sizes[i] = current_size;
  }

  return ECHO_OK;
}

void echo_chunked_file_free(echo_chunked_file_t *chunked) {
  size_t i = 0;

  if (!chunked) {
    return;
  }

  if (chunked->chunks) {
    for (i = 0; i < chunked->chunk_count; i++) {
      free(chunked->chunks[i]);
    }
    free(chunked->chunks);
  }

  free(chunked->chunk_sizes);

  chunked->chunks = NULL;
  chunked->chunk_sizes = NULL;
  chunked->chunk_count = 0;
}
