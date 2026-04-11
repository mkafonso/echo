#ifndef TEST_FAKE_CRYPTO_H
#define TEST_FAKE_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

typedef struct fake_crypto {
  uint8_t seed;
} fake_crypto_t;

void fake_crypto_init(fake_crypto_t *crypto, uint8_t seed);

void fake_crypto_random_bytes(fake_crypto_t *crypto, uint8_t *out, size_t len);

void fake_crypto_xor_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                             uint8_t key, uint8_t *out_ciphertext);

void fake_crypto_xor_decrypt(const uint8_t *ciphertext, size_t ciphertext_len,
                             uint8_t key, uint8_t *out_plaintext);

#endif
