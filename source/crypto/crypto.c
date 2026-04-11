#include "echo/crypto.h"

#include <limits.h>
#include <sodium.h>
#include <stdlib.h>
#include <string.h>

static echo_error_t echo_sodium_init(void) {
  if (sodium_init() < 0) {
    return ECHO_ERR_CRYPTO;
  }
  return ECHO_OK;
}

static echo_error_t echo_derive_key_from_password(
    const char *password, const uint8_t nonce[24],
    uint8_t out_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES]
) {
  uint8_t salt[crypto_pwhash_SALTBYTES];
  size_t password_len;

  if (!password || !nonce || !out_key) {
    return ECHO_ERR_INVALID_ARG;
  }

  password_len = strlen(password);
  if (password_len == 0 || password_len > crypto_pwhash_PASSWD_MAX) {
    return ECHO_ERR_INVALID_ARG;
  }

  memcpy(salt, nonce, sizeof(salt));

  if (crypto_pwhash(out_key, crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                    password, password_len, salt,
                    crypto_pwhash_OPSLIMIT_MODERATE,
                    crypto_pwhash_MEMLIMIT_MODERATE,
                    crypto_pwhash_ALG_ARGON2ID13) != 0) {
    return ECHO_ERR_NOMEM;
  }

  return ECHO_OK;
}

echo_error_t echo_hash_sha256(const uint8_t *data, size_t len,
                              uint8_t out_hash[32]) {
  if (!out_hash || (!data && len > 0)) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (echo_sodium_init() != ECHO_OK) {
    return ECHO_ERR_CRYPTO;
  }

  if (crypto_hash_sha256(out_hash, data, (unsigned long long)len) != 0) {
    return ECHO_ERR_CRYPTO;
  }

  return ECHO_OK;
}

echo_error_t echo_random_bytes(uint8_t *out, size_t len) {
  if (!out && len > 0) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (len == 0) {
    return ECHO_OK;
  }

  if (echo_sodium_init() != ECHO_OK) {
    return ECHO_ERR_CRYPTO;
  }

  randombytes_buf(out, len);
  return ECHO_OK;
}

echo_error_t echo_encrypt_chunk(const uint8_t *plaintext, size_t plaintext_len,
                                const char *password, uint8_t nonce[24],
                                uint8_t **out_ciphertext,
                                size_t *out_ciphertext_len) {
  uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  unsigned long long clen_ull = 0;
  unsigned long long mlen_ull;
  echo_error_t err;

  if ((!plaintext && plaintext_len > 0) || !password || !nonce ||
      !out_ciphertext || !out_ciphertext_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (echo_sodium_init() != ECHO_OK) {
    return ECHO_ERR_CRYPTO;
  }

  if (plaintext_len >
      (size_t)(ULLONG_MAX - crypto_aead_xchacha20poly1305_ietf_ABYTES)) {
    return ECHO_ERR_INVALID_ARG;
  }

  randombytes_buf(nonce, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);

  err = echo_derive_key_from_password(password, nonce, key);
  if (err != ECHO_OK) {
    sodium_memzero(key, sizeof(key));
    return err;
  }

  mlen_ull = (unsigned long long)plaintext_len;
  *out_ciphertext = (uint8_t *)malloc(
      plaintext_len + crypto_aead_xchacha20poly1305_ietf_ABYTES
  );
  if (!*out_ciphertext) {
    sodium_memzero(key, sizeof(key));
    return ECHO_ERR_NOMEM;
  }

  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
          *out_ciphertext, &clen_ull, plaintext, mlen_ull, NULL, 0, NULL, nonce,
          key) != 0) {
    free(*out_ciphertext);
    *out_ciphertext = NULL;
    sodium_memzero(key, sizeof(key));
    return ECHO_ERR_CRYPTO;
  }

  sodium_memzero(key, sizeof(key));

  if (clen_ull > SIZE_MAX) {
    free(*out_ciphertext);
    *out_ciphertext = NULL;
    return ECHO_ERR_INTERNAL;
  }

  *out_ciphertext_len = (size_t)clen_ull;
  return ECHO_OK;
}

echo_error_t echo_decrypt_chunk(const uint8_t *ciphertext,
                                size_t ciphertext_len, const char *password,
                                const uint8_t nonce[24],
                                uint8_t **out_plaintext,
                                size_t *out_plaintext_len) {
  uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  unsigned long long mlen_ull = 0;
  unsigned long long clen_ull;
  echo_error_t err;

  if ((!ciphertext && ciphertext_len > 0) || !password || !nonce ||
      !out_plaintext || !out_plaintext_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (echo_sodium_init() != ECHO_OK) {
    return ECHO_ERR_CRYPTO;
  }

  if (ciphertext_len < crypto_aead_xchacha20poly1305_ietf_ABYTES) {
    return ECHO_ERR_CORRUPTED;
  }

  err = echo_derive_key_from_password(password, nonce, key);
  if (err != ECHO_OK) {
    sodium_memzero(key, sizeof(key));
    return err;
  }

  clen_ull = (unsigned long long)ciphertext_len;
  *out_plaintext = (uint8_t *)malloc(
      ciphertext_len - crypto_aead_xchacha20poly1305_ietf_ABYTES
  );
  if (!*out_plaintext) {
    sodium_memzero(key, sizeof(key));
    return ECHO_ERR_NOMEM;
  }

  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
          *out_plaintext, &mlen_ull, NULL, ciphertext, clen_ull, NULL, 0, nonce,
          key) != 0) {
    free(*out_plaintext);
    *out_plaintext = NULL;
    sodium_memzero(key, sizeof(key));
    return ECHO_ERR_CORRUPTED;
  }

  sodium_memzero(key, sizeof(key));

  if (mlen_ull > SIZE_MAX) {
    free(*out_plaintext);
    *out_plaintext = NULL;
    return ECHO_ERR_INTERNAL;
  }

  *out_plaintext_len = (size_t)mlen_ull;
  return ECHO_OK;
}
