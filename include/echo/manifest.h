#ifndef ECHO_MANIFEST_H
#define ECHO_MANIFEST_H

#include "errors.h"
#include "types.h"
#include <stddef.h>
#include <stdint.h>

typedef struct echo_manifest {
  uint32_t version;
  uint64_t original_size;
  uint32_t chunk_size;
  uint32_t total_chunks;
  uint8_t file_hash[ECHO_HASH_SIZE];
  echo_chunk_info_t *chunks;
} echo_manifest_t;

echo_error_t echo_manifest_init(echo_manifest_t *manifest,
                                uint64_t original_size, uint32_t chunk_size,
                                uint32_t total_chunks);

void echo_manifest_free(echo_manifest_t *manifest);

echo_error_t echo_manifest_save(const char *path,
                                const echo_manifest_t *manifest);

echo_error_t echo_manifest_load(const char *path,
                                echo_manifest_t *out_manifest);

#endif
