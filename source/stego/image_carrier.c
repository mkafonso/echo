#include "echo/image_carrier.h"

#include "echo/errors.h"

#include <ctype.h>
#include <sodium.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t ECHO_IMAGE_MAGIC[8] = {'E', 'C', 'H', 'O',
                                           'I', 'M', 'G', '1'};

static void echo_u64_to_be(uint64_t v, uint8_t out[8]) {
  out[0] = (uint8_t)((v >> 56) & 0xff);
  out[1] = (uint8_t)((v >> 48) & 0xff);
  out[2] = (uint8_t)((v >> 40) & 0xff);
  out[3] = (uint8_t)((v >> 32) & 0xff);
  out[4] = (uint8_t)((v >> 24) & 0xff);
  out[5] = (uint8_t)((v >> 16) & 0xff);
  out[6] = (uint8_t)((v >> 8) & 0xff);
  out[7] = (uint8_t)(v & 0xff);
}

static uint64_t echo_u64_from_be(const uint8_t in[8]) {
  return ((uint64_t)in[0] << 56) | ((uint64_t)in[1] << 48) |
         ((uint64_t)in[2] << 40) | ((uint64_t)in[3] << 32) |
         ((uint64_t)in[4] << 24) | ((uint64_t)in[5] << 16) |
         ((uint64_t)in[6] << 8) | (uint64_t)in[7];
}

static void echo_skip_ppm_ws_and_comments(const uint8_t *in, size_t in_len,
                                         size_t *io_pos) {
  size_t p = *io_pos;

  for (;;) {
    while (p < in_len && isspace((unsigned char)in[p])) {
      p++;
    }

    if (p < in_len && in[p] == '#') {
      while (p < in_len && in[p] != '\n') {
        p++;
      }
      continue;
    }

    break;
  }

  *io_pos = p;
}

static int echo_read_ppm_token(const uint8_t *in, size_t in_len, size_t *io_pos,
                               char out[64]) {
  size_t p = *io_pos;
  size_t j = 0;

  echo_skip_ppm_ws_and_comments(in, in_len, &p);
  if (p >= in_len) {
    return 0;
  }

  while (p < in_len && !isspace((unsigned char)in[p]) && in[p] != '#' &&
         j + 1 < 64) {
    out[j++] = (char)in[p++];
  }
  out[j] = '\0';

  if (j == 0) {
    return 0;
  }

  *io_pos = p;
  return 1;
}

static echo_error_t echo_ppm_encode(const uint8_t *in, size_t in_len,
                                   uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  unsigned char hash[16];
  uint8_t len_be[8];
  size_t required;
  size_t pixels;
  int width = 256;
  int height;
  size_t pixel_data_len;
  char header[64];
  int header_len;
  uint8_t *buf;
  uint8_t *pix;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }

  if (crypto_generichash(hash, sizeof(hash), in, (unsigned long long)in_len, NULL,
                         0) != 0) {
    return ECHO_ERR_CRYPTO;
  }

  required = HEADER_BYTES + in_len;
  pixels = (required + 2) / 3;
  height = (int)((pixels + (size_t)width - 1) / (size_t)width);
  if (height <= 0) {
    height = 1;
  }

  pixel_data_len = (size_t)width * (size_t)height * 3;
  header_len = snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
  if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
    return ECHO_ERR_INTERNAL;
  }

  buf = (uint8_t *)malloc((size_t)header_len + pixel_data_len);
  if (!buf) {
    return ECHO_ERR_NOMEM;
  }

  memcpy(buf, header, (size_t)header_len);
  pix = buf + (size_t)header_len;
  memset(pix, 0, pixel_data_len);

  memcpy(pix, ECHO_IMAGE_MAGIC, sizeof(ECHO_IMAGE_MAGIC));
  echo_u64_to_be((uint64_t)in_len, len_be);
  memcpy(pix + 8, len_be, 8);
  memcpy(pix + 16, hash, sizeof(hash));
  if (in_len > 0) {
    memcpy(pix + HEADER_BYTES, in, in_len);
  }

  *out = buf;
  *out_len = (size_t)header_len + pixel_data_len;
  return ECHO_OK;
}

static echo_error_t echo_ppm_decode(const uint8_t *in, size_t in_len,
                                   uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  char tok[64];
  size_t pos = 0;
  int width;
  int height;
  int maxval;
  size_t pixel_data_len;
  const uint8_t *pix;
  uint64_t payload_len_u64;
  size_t payload_len;
  const uint8_t *expected_hash;
  unsigned char hash[16];
  uint8_t *buf;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }

  if (!echo_read_ppm_token(in, in_len, &pos, tok)) {
    return ECHO_ERR_CORRUPTED;
  }
  if (strcmp(tok, "P6") != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  if (!echo_read_ppm_token(in, in_len, &pos, tok)) {
    return ECHO_ERR_CORRUPTED;
  }
  width = atoi(tok);
  if (!echo_read_ppm_token(in, in_len, &pos, tok)) {
    return ECHO_ERR_CORRUPTED;
  }
  height = atoi(tok);
  if (!echo_read_ppm_token(in, in_len, &pos, tok)) {
    return ECHO_ERR_CORRUPTED;
  }
  maxval = atoi(tok);

  if (width <= 0 || height <= 0 || maxval != 255) {
    return ECHO_ERR_CORRUPTED;
  }

  echo_skip_ppm_ws_and_comments(in, in_len, &pos);

  pixel_data_len = (size_t)width * (size_t)height * 3;
  if (in_len < pos || in_len - pos < pixel_data_len) {
    return ECHO_ERR_CORRUPTED;
  }

  pix = in + pos;
  if (pixel_data_len < HEADER_BYTES) {
    return ECHO_ERR_CORRUPTED;
  }

  if (memcmp(pix, ECHO_IMAGE_MAGIC, sizeof(ECHO_IMAGE_MAGIC)) != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  payload_len_u64 = echo_u64_from_be(pix + 8);
  if (payload_len_u64 > SIZE_MAX) {
    return ECHO_ERR_CORRUPTED;
  }
  payload_len = (size_t)payload_len_u64;
  if (payload_len > pixel_data_len - HEADER_BYTES) {
    return ECHO_ERR_CORRUPTED;
  }

  expected_hash = pix + 16;
  if (crypto_generichash(hash, sizeof(hash), pix + HEADER_BYTES,
                         (unsigned long long)payload_len, NULL, 0) != 0) {
    return ECHO_ERR_CRYPTO;
  }
  if (sodium_memcmp(hash, expected_hash, sizeof(hash)) != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  buf = (uint8_t *)malloc(payload_len);
  if (!buf && payload_len > 0) {
    return ECHO_ERR_NOMEM;
  }
  if (payload_len > 0) {
    memcpy(buf, pix + HEADER_BYTES, payload_len);
  }

  *out = buf;
  *out_len = payload_len;
  return ECHO_OK;
}

static const echo_stego_codec_t ECHO_STEGO_IMAGE_PPM = {
    .extension = ".ppm", .encode = echo_ppm_encode, .decode = echo_ppm_decode};

const echo_stego_codec_t *echo_image_carrier_codec(void) {
  return &ECHO_STEGO_IMAGE_PPM;
}
