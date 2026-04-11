#include "echo/app.h"
#include "echo/chunker.h"
#include "echo/crypto.h"
#include "echo/manifest.h"
#include "echo/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void build_object_name(uint32_t index,
                              char out_name[ECHO_OBJECT_NAME_SIZE]) {
  snprintf(out_name, ECHO_OBJECT_NAME_SIZE, "chunk_%06u.bin", index);
}

echo_error_t echo_upload_file(const char *input_path, const char *manifest_path,
                              const char *password, size_t chunk_size,
                              echo_provider_t *provider) {
  echo_error_t err = ECHO_OK;
  uint8_t *file_data = NULL;
  size_t file_len = 0;
  echo_chunked_file_t chunked = {0};
  echo_manifest_t manifest = {0};
  size_t i;

  if (!input_path || !manifest_path || !password || !provider ||
      chunk_size == 0) {
    return ECHO_ERR_INVALID_ARG;
  }

  err = echo_read_file(input_path, &file_data, &file_len);
  if (err != ECHO_OK)
    goto cleanup;

  err = echo_chunk_file(file_data, file_len, chunk_size, &chunked);
  if (err != ECHO_OK)
    goto cleanup;

  err = echo_manifest_init(&manifest, file_len, (uint32_t)chunk_size,
                           (uint32_t)chunked.chunk_count);
  if (err != ECHO_OK)
    goto cleanup;

  err = echo_hash_sha256(file_data, file_len, manifest.file_hash);
  if (err != ECHO_OK)
    goto cleanup;

  for (i = 0; i < chunked.chunk_count; i++) {
    uint8_t *cipher = NULL;
    size_t cipher_len = 0;

    manifest.chunks[i].index = (uint32_t)i;
    build_object_name((uint32_t)i, manifest.chunks[i].object_name);

    err = echo_hash_sha256(chunked.chunks[i], chunked.chunk_sizes[i],
                           manifest.chunks[i].hash);
    if (err != ECHO_OK)
      goto cleanup;

    err =
        echo_encrypt_chunk(chunked.chunks[i], chunked.chunk_sizes[i], password,
                           manifest.chunks[i].nonce, &cipher, &cipher_len);
    if (err != ECHO_OK)
      goto cleanup;

    manifest.chunks[i].plain_size = (uint64_t)chunked.chunk_sizes[i];
    manifest.chunks[i].cipher_size = (uint64_t)cipher_len;

    err = echo_provider_put(provider, manifest.chunks[i].object_name, cipher,
                            cipher_len);

    free(cipher);

    if (err != ECHO_OK)
      goto cleanup;
  }

  err = echo_manifest_save(manifest_path, &manifest);

cleanup:
  if (file_data) {
    echo_secure_zero(file_data, file_len);
    free(file_data);
  }

  echo_chunked_file_free(&chunked);
  echo_manifest_free(&manifest);
  return err;
}
