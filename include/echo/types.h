#ifndef ECHO_TYPES_H
#define ECHO_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define ECHO_MAX_PATH 1024
#define ECHO_HASH_SIZE 32
#define ECHO_NONCE_SIZE 24
#define ECHO_OBJECT_NAME_SIZE 128

typedef struct echo_buffer {
  uint8_t *data;
  size_t len;
} echo_buffer_t;

typedef struct echo_chunk_info {
  uint32_t index;
  uint64_t plain_size;
  uint64_t cipher_size;
  uint8_t hash[ECHO_HASH_SIZE];
  uint8_t nonce[ECHO_NONCE_SIZE];
  char object_name[ECHO_OBJECT_NAME_SIZE];
} echo_chunk_info_t;

#endif
