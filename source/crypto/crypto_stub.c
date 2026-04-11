#include "echo/crypto.h"

#include <stdlib.h>
#include <string.h>

echo_error_t echo_hash_sha256(const uint8_t *data, size_t len,
                              uint8_t out_hash[32]) {
  size_t i;

  if (!data || !out_hash) {
    return ECHO_ERR_INVALID_ARG;
  }

  memset(out_hash, 0, 32);
  for (i = 0; i < len; i++) {
    out_hash[i % 32] ^= data[i];
  }

  return ECHO_OK;
}

echo_error_t echo_random_bytes(uint8_t *out, size_t len) {
  size_t i;

  if (!out) {
    return ECHO_ERR_INVALID_ARG;
  }

  for (i = 0; i < len; i++) {
    out[i] = (uint8_t)(rand() % 256);
  }

  return ECHO_OK;
}

echo_error_t echo_encrypt_chunk(const uint8_t *plaintext, size_t plaintext_len,
                                const char *password, uint8_t nonce[24],
                                uint8_t **out_ciphertext,
                                size_t *out_ciphertext_len) {
  size_t i;
  size_t pass_len;

  if (!plaintext || !password || !nonce || !out_ciphertext ||
      !out_ciphertext_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  pass_len = strlen(password);

  *out_ciphertext = (uint8_t *)malloc(plaintext_len);
  if (!*out_ciphertext) {
    return ECHO_ERR_NOMEM;
  }

  if (echo_random_bytes(nonce, 24) != ECHO_OK) {
    free(*out_ciphertext);
    return ECHO_ERR_CRYPTO;
  }

  for (i = 0; i < plaintext_len; i++) {
    (*out_ciphertext)[i] = plaintext[i] ^ password[i % pass_len];
  }

  *out_ciphertext_len = plaintext_len;
  return ECHO_OK;
}

echo_error_t echo_decrypt_chunk(const uint8_t *ciphertext,
                                size_t ciphertext_len, const char *password,
                                const uint8_t nonce[24],
                                uint8_t **out_plaintext,
                                size_t *out_plaintext_len) {
  size_t i;
  size_t pass_len;

  (void)nonce;

  if (!ciphertext || !password || !out_plaintext || !out_plaintext_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  pass_len = strlen(password);

  *out_plaintext = (uint8_t *)malloc(ciphertext_len);
  if (!*out_plaintext) {
    return ECHO_ERR_NOMEM;
  }

  for (i = 0; i < ciphertext_len; i++) {
    (*out_plaintext)[i] = ciphertext[i] ^ password[i % pass_len];
  }

  *out_plaintext_len = ciphertext_len;
  return ECHO_OK;
}
