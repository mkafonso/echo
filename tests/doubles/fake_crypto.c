#include "fake_crypto.h"

void fake_crypto_init(fake_crypto_t *crypto, uint8_t seed) {
  if (!crypto) {
    return;
  }
  crypto->seed = seed;
}

void fake_crypto_random_bytes(fake_crypto_t *crypto, uint8_t *out, size_t len) {
  size_t i;

  if (!crypto || (!out && len > 0)) {
    return;
  }

  for (i = 0; i < len; i++) {
    crypto->seed = (uint8_t)(crypto->seed * 33u + 17u);
    out[i] = crypto->seed;
  }
}

void fake_crypto_xor_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                             uint8_t key, uint8_t *out_ciphertext) {
  size_t i;

  if ((!plaintext && plaintext_len > 0) || !out_ciphertext) {
    return;
  }

  for (i = 0; i < plaintext_len; i++) {
    out_ciphertext[i] = (uint8_t)(plaintext[i] ^ key);
  }
}

void fake_crypto_xor_decrypt(const uint8_t *ciphertext, size_t ciphertext_len,
                             uint8_t key, uint8_t *out_plaintext) {
  fake_crypto_xor_encrypt(ciphertext, ciphertext_len, key, out_plaintext);
}
