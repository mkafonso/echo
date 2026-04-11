#include "echo/crypto.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void test_crypto_sha256_known_vector(void) {
  const uint8_t msg[] = {'a', 'b', 'c'};
  uint8_t hash[32] = {0};

  const uint8_t expected[32] = {
      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
  };

  assert(echo_hash_sha256(msg, sizeof(msg), hash) == ECHO_OK);
  assert(memcmp(hash, expected, sizeof(expected)) == 0);
}

static void test_crypto_random_bytes_basic(void) {
  uint8_t out[16] = {0};

  assert(echo_random_bytes(NULL, 0) == ECHO_OK);
  assert(echo_random_bytes(NULL, 1) == ECHO_ERR_INVALID_ARG);
  assert(echo_random_bytes(out, sizeof(out)) == ECHO_OK);
}

static void test_crypto_encrypt_decrypt_roundtrip(void) {
  const uint8_t plaintext[] = "hello, crypto";
  const char *password = "password";

  uint8_t nonce[24] = {0};
  uint8_t *cipher = NULL;
  size_t cipher_len = 0;

  uint8_t *plain = NULL;
  size_t plain_len = 0;

  assert(echo_encrypt_chunk(plaintext, strlen((const char *)plaintext), password,
                            nonce, &cipher, &cipher_len) == ECHO_OK);
  assert(cipher != NULL);
  assert(cipher_len > strlen((const char *)plaintext));

  assert(echo_decrypt_chunk(cipher, cipher_len, password, nonce, &plain,
                            &plain_len) == ECHO_OK);
  assert(plain != NULL);
  assert(plain_len == strlen((const char *)plaintext));
  assert(memcmp(plain, plaintext, plain_len) == 0);

  free(cipher);
  free(plain);
}

static void test_crypto_decrypt_wrong_password_fails(void) {
  const uint8_t plaintext[] = "secret";
  uint8_t nonce[24] = {0};
  uint8_t *cipher = NULL;
  size_t cipher_len = 0;
  uint8_t *plain = NULL;
  size_t plain_len = 0;

  assert(echo_encrypt_chunk(plaintext, strlen((const char *)plaintext), "right",
                            nonce, &cipher, &cipher_len) == ECHO_OK);

  assert(echo_decrypt_chunk(cipher, cipher_len, "wrong", nonce, &plain,
                            &plain_len) == ECHO_ERR_CORRUPTED);

  free(cipher);
  free(plain);
}

static void test_crypto_invalid_args(void) {
  uint8_t hash[32] = {0};
  uint8_t nonce[24] = {0};
  uint8_t *out = NULL;
  size_t out_len = 0;

  assert(echo_hash_sha256(NULL, 1, hash) == ECHO_ERR_INVALID_ARG);
  assert(echo_hash_sha256((const uint8_t *)"x", 1, NULL) ==
         ECHO_ERR_INVALID_ARG);

  assert(echo_encrypt_chunk(NULL, 1, "p", nonce, &out, &out_len) ==
         ECHO_ERR_INVALID_ARG);
  assert(echo_encrypt_chunk((const uint8_t *)"x", 1, NULL, nonce, &out,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_encrypt_chunk((const uint8_t *)"x", 1, "p", NULL, &out,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_encrypt_chunk((const uint8_t *)"x", 1, "p", nonce, NULL,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_encrypt_chunk((const uint8_t *)"x", 1, "p", nonce, &out, NULL) ==
         ECHO_ERR_INVALID_ARG);

  assert(echo_decrypt_chunk(NULL, 1, "p", nonce, &out, &out_len) ==
         ECHO_ERR_INVALID_ARG);
  assert(echo_decrypt_chunk((const uint8_t *)"x", 1, NULL, nonce, &out,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_decrypt_chunk((const uint8_t *)"x", 1, "p", NULL, &out,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_decrypt_chunk((const uint8_t *)"x", 1, "p", nonce, NULL,
                            &out_len) == ECHO_ERR_INVALID_ARG);
  assert(echo_decrypt_chunk((const uint8_t *)"x", 1, "p", nonce, &out, NULL) ==
         ECHO_ERR_INVALID_ARG);
}

int main(void) {
  test_crypto_sha256_known_vector();
  test_crypto_random_bytes_basic();
  test_crypto_encrypt_decrypt_roundtrip();
  test_crypto_decrypt_wrong_password_fails();
  test_crypto_invalid_args();
  return 0;
}
