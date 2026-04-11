#include "echo/app.h"
#include "echo/manifest.h"
#include "echo/stego.h"

#include <stdlib.h>
#include <string.h>

echo_error_t echo_verify_file(const char *manifest_path,
                              echo_provider_t *provider) {
  echo_error_t err = ECHO_OK;
  echo_manifest_t manifest = {0};
  size_t i;

  if (!manifest_path || !provider) {
    return ECHO_ERR_INVALID_ARG;
  }

  err = echo_manifest_load(manifest_path, &manifest);
  if (err != ECHO_OK) {
    return err;
  }

  for (i = 0; i < manifest.total_chunks; i++) {
    int exists = 0;
    const echo_stego_codec_t *codec =
        echo_stego_codec_for_object_name(manifest.chunks[i].object_name);

    err =
        echo_provider_exists(provider, manifest.chunks[i].object_name, &exists);
    if (err != ECHO_OK) {
      echo_manifest_free(&manifest);
      return err;
    }

    if (!exists) {
      echo_manifest_free(&manifest);
      return ECHO_ERR_NOT_FOUND;
    }

    if (codec) {
      uint8_t *payload = NULL;
      size_t payload_len = 0;
      uint8_t *cipher = NULL;
      size_t cipher_len = 0;

      err = echo_provider_get(provider, manifest.chunks[i].object_name, &payload,
                              &payload_len);
      if (err != ECHO_OK) {
        echo_manifest_free(&manifest);
        return err;
      }

      err = codec->decode(payload, payload_len, &cipher, &cipher_len);
      free(payload);
      if (err != ECHO_OK) {
        free(cipher);
        echo_manifest_free(&manifest);
        return ECHO_ERR_CORRUPTED;
      }

      if ((uint64_t)cipher_len != manifest.chunks[i].cipher_size) {
        free(cipher);
        echo_manifest_free(&manifest);
        return ECHO_ERR_CORRUPTED;
      }

      free(cipher);
    }
  }

  echo_manifest_free(&manifest);
  return ECHO_OK;
}
