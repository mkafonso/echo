#include "echo/app.h"
#include "echo/chunker.h"
#include "echo/crypto.h"
#include "echo/manifest.h"
#include "echo/stego.h"
#include "echo/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void build_object_name(uint32_t index,
                              char out_name[ECHO_OBJECT_NAME_SIZE]) {
  snprintf(out_name, ECHO_OBJECT_NAME_SIZE, "chunk_%06u.bin", index);
}

static void build_object_name_text(uint32_t index,
                                   char out_name[ECHO_OBJECT_NAME_SIZE]) {
  snprintf(out_name, ECHO_OBJECT_NAME_SIZE, "chunk_%06u.txt", index);
}

static void build_object_name_image(uint32_t index,
                                    char out_name[ECHO_OBJECT_NAME_SIZE]) {
  snprintf(out_name, ECHO_OBJECT_NAME_SIZE, "chunk_%06u.ppm", index);
}

static echo_error_t echo_upload_file_impl(const char *input_path,
                                          const char *manifest_path,
                                          const char *password,
                                          size_t chunk_size,
                                          echo_provider_t *provider,
                                          const echo_stego_codec_t *stego) {
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
    uint8_t *payload = NULL;
    size_t payload_len = 0;

    manifest.chunks[i].index = (uint32_t)i;
    if (stego) {
      if (stego->extension && strcmp(stego->extension, ".txt") == 0) {
        build_object_name_text((uint32_t)i, manifest.chunks[i].object_name);
      } else if (stego->extension && strcmp(stego->extension, ".ppm") == 0) {
        build_object_name_image((uint32_t)i, manifest.chunks[i].object_name);
      } else {
        build_object_name_text((uint32_t)i, manifest.chunks[i].object_name);
      }
    } else {
      build_object_name((uint32_t)i, manifest.chunks[i].object_name);
    }

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

    if (stego) {
      err = stego->encode(cipher, cipher_len, &payload, &payload_len);
      free(cipher);
      cipher = NULL;
      if (err != ECHO_OK)
        goto cleanup;
    } else {
      payload = cipher;
      payload_len = cipher_len;
    }

    err = echo_provider_put(provider, manifest.chunks[i].object_name, payload,
                            payload_len);

    if (err != ECHO_OK) {
      free(payload);
      goto cleanup;
    }

    free(payload);
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

echo_error_t echo_upload_file(const char *input_path, const char *manifest_path,
                              const char *password, size_t chunk_size,
                              echo_provider_t *provider) {
  return echo_upload_file_impl(input_path, manifest_path, password, chunk_size,
                               provider, NULL);
}

echo_error_t echo_upload_file_text(const char *input_path,
                                   const char *manifest_path,
                                   const char *password, size_t chunk_size,
                                   echo_provider_t *provider) {
  const echo_stego_codec_t *codec =
      echo_stego_codec_for_object_name("chunk_000000.txt");
  if (!codec) {
    return ECHO_ERR_INTERNAL;
  }

  return echo_upload_file_impl(input_path, manifest_path, password, chunk_size,
                               provider, codec);
}

echo_error_t echo_upload_file_image(const char *input_path,
                                    const char *manifest_path,
                                    const char *password, size_t chunk_size,
                                    echo_provider_t *provider) {
  const echo_stego_codec_t *codec =
      echo_stego_codec_for_object_name("chunk_000000.ppm");
  if (!codec) {
    return ECHO_ERR_INTERNAL;
  }

  return echo_upload_file_impl(input_path, manifest_path, password, chunk_size,
                               provider, codec);
}
