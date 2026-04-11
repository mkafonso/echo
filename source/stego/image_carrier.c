#include "echo/image_carrier.h"

#include "echo/errors.h"

#include <ctype.h>
#include <limits.h>
#include <sodium.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t ECHO_IMAGE_MAGIC[8] = {'E', 'C', 'H', 'O',
                                            'I', 'M', 'G', '1'};

static const uint8_t ECHO_IMAGE_LSB_MAGIC[8] = {'E', 'C', 'H', 'O',
                                                'L', 'S', 'B', '1'};

static const uint8_t ECHO_IMAGE_PNG_MAGIC[8] = {'E', 'C', 'H', 'O',
                                                'P', 'N', 'G', '1'};

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

  if (crypto_generichash(hash, sizeof(hash), in, (unsigned long long)in_len,
                         NULL, 0) != 0) {
    return ECHO_ERR_CRYPTO;
  }

  required = HEADER_BYTES + in_len;
  pixels = (required + 2) / 3;
  height = (int)((pixels + (size_t)width - 1) / (size_t)width);
  if (height <= 0) {
    height = 1;
  }

  pixel_data_len = (size_t)width * (size_t)height * 3;
  header_len =
      snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
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

static echo_error_t echo_ppm_lsb_encode(const uint8_t *in, size_t in_len,
                                        uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  unsigned char hash[16];
  uint8_t len_be[8];
  uint8_t header_buf[HEADER_BYTES];
  size_t payload_total;
  size_t required_bits;
  int width = 512;
  size_t row_bits;
  size_t rows;
  int height;
  size_t pixel_data_len;
  char header[64];
  int header_len;
  uint8_t *buf;
  uint8_t *pix;
  uint8_t *payload;
  size_t bit_i;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }

  if (crypto_generichash(hash, sizeof(hash), in, (unsigned long long)in_len,
                         NULL, 0) != 0) {
    return ECHO_ERR_CRYPTO;
  }

  memcpy(header_buf, ECHO_IMAGE_LSB_MAGIC, 8);
  echo_u64_to_be((uint64_t)in_len, len_be);
  memcpy(header_buf + 8, len_be, 8);
  memcpy(header_buf + 16, hash, sizeof(hash));

  if (in_len > SIZE_MAX - HEADER_BYTES) {
    return ECHO_ERR_NOMEM;
  }
  payload_total = HEADER_BYTES + in_len;

  if (payload_total > SIZE_MAX / 8) {
    return ECHO_ERR_NOMEM;
  }
  required_bits = payload_total * 8;

  row_bits = (size_t)width * 3;
  if (row_bits == 0) {
    return ECHO_ERR_INTERNAL;
  }
  rows = (required_bits + row_bits - 1) / row_bits;
  if (rows == 0) {
    rows = 1;
  }
  if (rows > (size_t)INT_MAX) {
    return ECHO_ERR_NOMEM;
  }
  height = (int)rows;

  if ((size_t)width > SIZE_MAX / (size_t)height / 3) {
    return ECHO_ERR_NOMEM;
  }
  pixel_data_len = (size_t)width * (size_t)height * 3;
  if (pixel_data_len < required_bits) {
    return ECHO_ERR_INTERNAL;
  }

  header_len =
      snprintf(header, sizeof(header), "P6\n%d %d\n255\n", width, height);
  if (header_len <= 0 || (size_t)header_len >= sizeof(header)) {
    return ECHO_ERR_INTERNAL;
  }

  buf = (uint8_t *)malloc((size_t)header_len + pixel_data_len);
  if (!buf) {
    return ECHO_ERR_NOMEM;
  }

  memcpy(buf, header, (size_t)header_len);
  pix = buf + (size_t)header_len;

  {
    size_t y;
    for (y = 0; y < (size_t)height; y++) {
      size_t x;
      for (x = 0; x < (size_t)width; x++) {
        size_t idx = (y * (size_t)width + x) * 3;
        uint8_t r =
            (uint8_t)((width > 1) ? ((x * 255u) / ((size_t)width - 1)) : 0);
        uint8_t g =
            (uint8_t)((height > 1) ? ((y * 255u) / ((size_t)height - 1)) : 0);
        uint8_t b = (uint8_t)(((size_t)width + (size_t)height > 2)
                                  ? (((x + y) * 255u) /
                                     ((size_t)width + (size_t)height - 2))
                                  : 0);
        pix[idx + 0] = (uint8_t)(r & 0xFE);
        pix[idx + 1] = (uint8_t)(g & 0xFE);
        pix[idx + 2] = (uint8_t)(b & 0xFE);
      }
    }
  }

  payload = (uint8_t *)malloc(payload_total);
  if (!payload) {
    free(buf);
    return ECHO_ERR_NOMEM;
  }
  memcpy(payload, header_buf, HEADER_BYTES);
  if (in_len > 0) {
    memcpy(payload + HEADER_BYTES, in, in_len);
  }

  for (bit_i = 0; bit_i < required_bits; bit_i++) {
    uint8_t byte = payload[bit_i / 8];
    uint8_t bit = (uint8_t)((byte >> (7 - (bit_i % 8))) & 1u);
    pix[bit_i] = (uint8_t)((pix[bit_i] & 0xFEu) | bit);
  }

  sodium_memzero(payload, payload_total);
  free(payload);

  *out = buf;
  *out_len = (size_t)header_len + pixel_data_len;
  return ECHO_OK;
}

static echo_error_t echo_ppm_lsb_decode(const uint8_t *in, size_t in_len,
                                        uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  char tok[64];
  size_t pos = 0;
  int width;
  int height;
  int maxval;
  size_t pixel_data_len;
  const uint8_t *pix;
  uint8_t header_buf[HEADER_BYTES];
  uint64_t payload_len_u64;
  size_t payload_len;
  size_t payload_total;
  size_t required_bits;
  uint8_t *payload;
  unsigned char hash[16];
  size_t bit_i;

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

  if ((size_t)width > SIZE_MAX / (size_t)height / 3) {
    return ECHO_ERR_CORRUPTED;
  }
  pixel_data_len = (size_t)width * (size_t)height * 3;
  if (in_len < pos || in_len - pos < pixel_data_len) {
    return ECHO_ERR_CORRUPTED;
  }
  pix = in + pos;

  if (pixel_data_len < HEADER_BYTES * 8) {
    return ECHO_ERR_CORRUPTED;
  }

  memset(header_buf, 0, sizeof(header_buf));
  for (bit_i = 0; bit_i < HEADER_BYTES * 8; bit_i++) {
    uint8_t bit = (uint8_t)(pix[bit_i] & 1u);
    header_buf[bit_i / 8] = (uint8_t)((header_buf[bit_i / 8] << 1) | bit);
  }

  if (memcmp(header_buf, ECHO_IMAGE_LSB_MAGIC, 8) != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  payload_len_u64 = echo_u64_from_be(header_buf + 8);
  if (payload_len_u64 > SIZE_MAX) {
    return ECHO_ERR_CORRUPTED;
  }
  payload_len = (size_t)payload_len_u64;

  if (payload_len > SIZE_MAX - HEADER_BYTES) {
    return ECHO_ERR_CORRUPTED;
  }
  payload_total = HEADER_BYTES + payload_len;
  if (payload_total > pixel_data_len / 8) {
    return ECHO_ERR_CORRUPTED;
  }

  required_bits = payload_total * 8;
  payload = (uint8_t *)calloc(1, payload_total);
  if (!payload) {
    return ECHO_ERR_NOMEM;
  }

  for (bit_i = 0; bit_i < required_bits; bit_i++) {
    uint8_t bit = (uint8_t)(pix[bit_i] & 1u);
    payload[bit_i / 8] = (uint8_t)((payload[bit_i / 8] << 1) | bit);
  }

  if (crypto_generichash(hash, sizeof(hash), payload + HEADER_BYTES,
                         (unsigned long long)payload_len, NULL, 0) != 0) {
    sodium_memzero(payload, payload_total);
    free(payload);
    return ECHO_ERR_CRYPTO;
  }
  if (sodium_memcmp(hash, payload + 16, sizeof(hash)) != 0) {
    sodium_memzero(payload, payload_total);
    free(payload);
    return ECHO_ERR_CORRUPTED;
  }

  if (payload_len == 0) {
    *out = NULL;
    *out_len = 0;
    sodium_memzero(payload, payload_total);
    free(payload);
    return ECHO_OK;
  }

  {
    uint8_t *buf = (uint8_t *)malloc(payload_len);
    if (!buf) {
      sodium_memzero(payload, payload_total);
      free(payload);
      return ECHO_ERR_NOMEM;
    }
    memcpy(buf, payload + HEADER_BYTES, payload_len);
    *out = buf;
    *out_len = payload_len;
  }

  sodium_memzero(payload, payload_total);
  free(payload);
  return ECHO_OK;
}

static const echo_stego_codec_t ECHO_STEGO_IMAGE_PPM = {
    .extension = ".ppm", .encode = echo_ppm_encode, .decode = echo_ppm_decode};

static const echo_stego_codec_t ECHO_STEGO_IMAGE_LSB = {
    .extension = ".pnm",
    .encode = echo_ppm_lsb_encode,
    .decode = echo_ppm_lsb_decode};

const echo_stego_codec_t *echo_image_carrier_codec(void) {
  return &ECHO_STEGO_IMAGE_PPM;
}

const echo_stego_codec_t *echo_image_carrier_lsb_codec(void) {
  return &ECHO_STEGO_IMAGE_LSB;
}

static echo_error_t echo_read_binary_file(const char *path, uint8_t **out,
                                          size_t *out_len) {
  FILE *fp;
  long size;
  uint8_t *buf;

  if (!path || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  fp = fopen(path, "rb");
  if (!fp) {
    return ECHO_ERR_NOT_FOUND;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  rewind(fp);

  buf = (uint8_t *)malloc((size_t)size);
  if (!buf && size > 0) {
    fclose(fp);
    return ECHO_ERR_NOMEM;
  }

  if (size > 0 && fread(buf, 1, (size_t)size, fp) != (size_t)size) {
    free(buf);
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  *out = buf;
  *out_len = (size_t)size;
  return ECHO_OK;
}

static uint32_t echo_be32(const uint8_t in[4]) {
  return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
         ((uint32_t)in[2] << 8) | (uint32_t)in[3];
}

static void echo_be32_write(uint8_t out[4], uint32_t v) {
  out[0] = (uint8_t)((v >> 24) & 0xff);
  out[1] = (uint8_t)((v >> 16) & 0xff);
  out[2] = (uint8_t)((v >> 8) & 0xff);
  out[3] = (uint8_t)(v & 0xff);
}

static uint32_t echo_crc32(const uint8_t *buf, size_t len) {
  uint32_t crc = 0xffffffffu;
  size_t i;

  for (i = 0; i < len; i++) {
    uint32_t k;
    crc ^= buf[i];
    for (k = 0; k < 8; k++) {
      if (crc & 1u) {
        crc = 0xedb88320u ^ (crc >> 1);
      } else {
        crc = crc >> 1;
      }
    }
  }

  return crc ^ 0xffffffffu;
}

static echo_error_t echo_load_default_png_cover(uint8_t **out,
                                                size_t *out_len) {
  const char *env = getenv("ECHO_STEGO_PNG_COVER");
  echo_error_t err;

  if (!out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (env && env[0]) {
    err = echo_read_binary_file(env, out, out_len);
    if (err == ECHO_OK) {
      return ECHO_OK;
    }
  }

#ifdef ECHO_ASSETS_DIR
  {
    char path[2048];
    int n = snprintf(path, sizeof(path), "%s/image.png", ECHO_ASSETS_DIR);
    if (n > 0 && (size_t)n < sizeof(path)) {
      err = echo_read_binary_file(path, out, out_len);
      if (err == ECHO_OK) {
        return ECHO_OK;
      }
    }
  }
#endif

  err = echo_read_binary_file("../assets/image.png", out, out_len);
  if (err == ECHO_OK) {
    return ECHO_OK;
  }

  err = echo_read_binary_file("../../assets/image.png", out, out_len);
  if (err == ECHO_OK) {
    return ECHO_OK;
  }

  *out = NULL;
  *out_len = 0;
  return ECHO_ERR_NOT_FOUND;
}

static echo_error_t echo_png_find_iend_offset(const uint8_t *png,
                                              size_t png_len,
                                              size_t *out_iend_offset) {
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  size_t pos = 8;

  if (!png || !out_iend_offset) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (png_len < 8 || memcmp(png, sig, 8) != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  while (pos + 12 <= png_len) {
    uint32_t len = echo_be32(png + pos);
    const uint8_t *type = png + pos + 4;
    size_t chunk_total = 12u + (size_t)len;

    if (chunk_total < 12 || pos + chunk_total > png_len) {
      return ECHO_ERR_CORRUPTED;
    }

    if (memcmp(type, "IEND", 4) == 0) {
      *out_iend_offset = pos;
      return ECHO_OK;
    }

    pos += chunk_total;
  }

  return ECHO_ERR_CORRUPTED;
}

static echo_error_t echo_png_chunk_encode(const uint8_t *in, size_t in_len,
                                          uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  unsigned char hash[16];
  uint8_t *cover = NULL;
  size_t cover_len = 0;
  size_t iend_offset = 0;
  uint8_t len_be[8];
  size_t data_len;
  uint8_t *buf;
  size_t buf_len;
  size_t out_pos;
  uint8_t chunk_len_be[4];
  uint8_t chunk_type[4] = {'e', 'C', 'h', 'O'};
  uint32_t crc;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }

  if (crypto_generichash(hash, sizeof(hash), in, (unsigned long long)in_len,
                         NULL, 0) != 0) {
    return ECHO_ERR_CRYPTO;
  }

  if (echo_load_default_png_cover(&cover, &cover_len) != ECHO_OK) {
    return ECHO_ERR_NOT_FOUND;
  }

  if (echo_png_find_iend_offset(cover, cover_len, &iend_offset) != ECHO_OK) {
    free(cover);
    return ECHO_ERR_CORRUPTED;
  }

  if (in_len > SIZE_MAX - HEADER_BYTES) {
    free(cover);
    return ECHO_ERR_NOMEM;
  }
  data_len = HEADER_BYTES + in_len;
  if (data_len > UINT32_MAX) {
    free(cover);
    return ECHO_ERR_NOMEM;
  }

  if (cover_len > SIZE_MAX - (12 + data_len)) {
    free(cover);
    return ECHO_ERR_NOMEM;
  }
  buf_len = cover_len + 12 + data_len;
  buf = (uint8_t *)malloc(buf_len);
  if (!buf) {
    free(cover);
    return ECHO_ERR_NOMEM;
  }

  memcpy(buf, cover, iend_offset);
  out_pos = iend_offset;

  echo_be32_write(chunk_len_be, (uint32_t)data_len);
  memcpy(buf + out_pos, chunk_len_be, 4);
  out_pos += 4;
  memcpy(buf + out_pos, chunk_type, 4);
  out_pos += 4;

  memcpy(buf + out_pos, ECHO_IMAGE_PNG_MAGIC, 8);
  out_pos += 8;
  echo_u64_to_be((uint64_t)in_len, len_be);
  memcpy(buf + out_pos, len_be, 8);
  out_pos += 8;
  memcpy(buf + out_pos, hash, sizeof(hash));
  out_pos += sizeof(hash);
  if (in_len > 0) {
    memcpy(buf + out_pos, in, in_len);
    out_pos += in_len;
  }

  {
    size_t crc_in_len = 4 + data_len;
    uint8_t *crc_in = (uint8_t *)malloc(crc_in_len);
    if (!crc_in) {
      free(cover);
      free(buf);
      return ECHO_ERR_NOMEM;
    }
    memcpy(crc_in, chunk_type, 4);
    memcpy(crc_in + 4, buf + iend_offset + 8, data_len);
    crc = echo_crc32(crc_in, crc_in_len);
    free(crc_in);
  }

  echo_be32_write(buf + out_pos, crc);
  out_pos += 4;

  memcpy(buf + out_pos, cover + iend_offset, cover_len - iend_offset);
  out_pos += cover_len - iend_offset;

  free(cover);

  *out = buf;
  *out_len = out_pos;
  return ECHO_OK;
}

static echo_error_t echo_png_chunk_decode(const uint8_t *in, size_t in_len,
                                          uint8_t **out, size_t *out_len) {
  enum { HEADER_BYTES = 32 };
  static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  size_t pos = 8;

  if ((!in && in_len > 0) || !out || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (in_len < 8 || memcmp(in, sig, 8) != 0) {
    return ECHO_ERR_CORRUPTED;
  }

  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }

  while (pos + 12 <= in_len) {
    uint32_t len = echo_be32(in + pos);
    const uint8_t *type = in + pos + 4;
    const uint8_t *data = in + pos + 8;
    size_t chunk_total = 12u + (size_t)len;

    if (chunk_total < 12 || pos + chunk_total > in_len) {
      return ECHO_ERR_CORRUPTED;
    }

    if (memcmp(type, "eChO", 4) == 0) {
      if (len >= HEADER_BYTES && memcmp(data, ECHO_IMAGE_PNG_MAGIC, 8) == 0) {
        uint64_t payload_len_u64 = echo_u64_from_be(data + 8);
        size_t payload_len;
        const uint8_t *expected_hash = data + 16;
        unsigned char hash[16];
        uint8_t *buf;

        if (payload_len_u64 > SIZE_MAX) {
          return ECHO_ERR_CORRUPTED;
        }
        payload_len = (size_t)payload_len_u64;
        if ((size_t)len < HEADER_BYTES + payload_len) {
          return ECHO_ERR_CORRUPTED;
        }

        if (crypto_generichash(hash, sizeof(hash), data + HEADER_BYTES,
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
          memcpy(buf, data + HEADER_BYTES, payload_len);
        }
        *out = buf;
        *out_len = payload_len;
        return ECHO_OK;
      }
    }

    pos += chunk_total;
  }

  return ECHO_ERR_CORRUPTED;
}

static const echo_stego_codec_t ECHO_STEGO_IMAGE_PNG = {
    .extension = ".png",
    .encode = echo_png_chunk_encode,
    .decode = echo_png_chunk_decode};

const echo_stego_codec_t *echo_image_carrier_png_codec(void) {
  return &ECHO_STEGO_IMAGE_PNG;
}
