#ifndef ECHO_STEGO_H
#define ECHO_STEGO_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

typedef struct echo_stego_codec {
  const char *extension;

  echo_error_t (*encode)(const uint8_t *in, size_t in_len, uint8_t **out,
                         size_t *out_len);

  echo_error_t (*decode)(const uint8_t *in, size_t in_len, uint8_t **out,
                         size_t *out_len);
} echo_stego_codec_t;

const echo_stego_codec_t *echo_stego_codec_for_object_name(
    const char *object_name
);

#endif
