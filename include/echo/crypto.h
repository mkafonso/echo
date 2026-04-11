#ifndef ECHO_CRYPTO_H
#define ECHO_CRYPTO_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

echo_error_t echo_hash_sha256(const uint8_t *data, size_t len,
                              uint8_t out_hash[32]);

echo_error_t echo_random_bytes(uint8_t *out, size_t len);

echo_error_t echo_encrypt_chunk(const uint8_t *plaintext, size_t plaintext_len,
                                const char *password, uint8_t nonce[24],
                                uint8_t **out_ciphertext,
                                size_t *out_ciphertext_len);

echo_error_t echo_decrypt_chunk(const uint8_t *ciphertext,
                                size_t ciphertext_len, const char *password,
                                const uint8_t nonce[24],
                                uint8_t **out_plaintext,
                                size_t *out_plaintext_len);

#endif
