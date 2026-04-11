#include "echo/app.h"
#include "echo/crypto.h"
#include "echo/manifest.h"
#include "echo/util.h"

#include <stdlib.h>
#include <string.h>

echo_error_t echo_download_file(const char *manifest_path,
                                const char *output_path, const char *password,
                                echo_provider_t *provider) {
  echo_error_t err = ECHO_OK;
  echo_manifest_t manifest = {0};
  uint8_t *final_data = NULL;
  size_t write_offset = 0;
  size_t i;

  if (!manifest_path || !output_path || !password || !provider) {
    return ECHO_ERR_INVALID_ARG;
  }

  err = echo_manifest_load(manifest_path, &manifest);
  if (err != ECHO_OK)
    goto cleanup;

  final_data = (uint8_t *)malloc((size_t)manifest.original_size);
  if (!final_data) {
    err = ECHO_ERR_NOMEM;
    goto cleanup;
  }

  for (i = 0; i < manifest.total_chunks; i++) {
    uint8_t *cipher = NULL;
    size_t cipher_len = 0;
    uint8_t *plain = NULL;
    size_t plain_len = 0;

    err = echo_provider_get(provider, manifest.chunks[i].object_name, &cipher,
                            &cipher_len);
    if (err != ECHO_OK)
      goto cleanup;

    err = echo_decrypt_chunk(cipher, cipher_len, password,
                             manifest.chunks[i].nonce, &plain, &plain_len);
    free(cipher);
    if (err != ECHO_OK) {
      free(plain);
      goto cleanup;
    }

    if (plain_len != manifest.chunks[i].plain_size) {
      free(plain);
      err = ECHO_ERR_CORRUPTED;
      goto cleanup;
    }

    memcpy(final_data + write_offset, plain, plain_len);
    write_offset += plain_len;
    free(plain);
  }

  err =
      echo_write_file(output_path, final_data, (size_t)manifest.original_size);

cleanup:
  if (final_data) {
    echo_secure_zero(final_data, (size_t)manifest.original_size);
    free(final_data);
  }

  echo_manifest_free(&manifest);
  return err;
}
