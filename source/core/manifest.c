#include "echo/manifest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

echo_error_t echo_manifest_init(echo_manifest_t *manifest,
                                uint64_t original_size, uint32_t chunk_size,
                                uint32_t total_chunks) {
  if (!manifest || total_chunks == 0 || chunk_size == 0) {
    return ECHO_ERR_INVALID_ARG;
  }

  memset(manifest, 0, sizeof(*manifest));
  manifest->version = 1;
  manifest->original_size = original_size;
  manifest->chunk_size = chunk_size;
  manifest->total_chunks = total_chunks;
  manifest->chunks =
      (echo_chunk_info_t *)calloc(total_chunks, sizeof(echo_chunk_info_t));

  if (!manifest->chunks) {
    return ECHO_ERR_NOMEM;
  }

  return ECHO_OK;
}

void echo_manifest_free(echo_manifest_t *manifest) {
  if (!manifest) {
    return;
  }

  free(manifest->chunks);
  manifest->chunks = NULL;
  manifest->total_chunks = 0;
}

echo_error_t echo_manifest_save(const char *path,
                                const echo_manifest_t *manifest) {
  FILE *fp = NULL;

  if (!path || !manifest || !manifest->chunks) {
    return ECHO_ERR_INVALID_ARG;
  }

  fp = fopen(path, "wb");
  if (!fp) {
    return ECHO_ERR_IO;
  }

  if (fwrite(&manifest->version, sizeof(manifest->version), 1, fp) != 1 ||
      fwrite(&manifest->original_size, sizeof(manifest->original_size), 1,
             fp) != 1 ||
      fwrite(&manifest->chunk_size, sizeof(manifest->chunk_size), 1, fp) != 1 ||
      fwrite(&manifest->total_chunks, sizeof(manifest->total_chunks), 1, fp) !=
          1 ||
      fwrite(manifest->file_hash, sizeof(manifest->file_hash), 1, fp) != 1 ||
      fwrite(manifest->chunks, sizeof(echo_chunk_info_t),
             manifest->total_chunks, fp) != manifest->total_chunks) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  return ECHO_OK;
}

echo_error_t echo_manifest_load(const char *path,
                                echo_manifest_t *out_manifest) {
  FILE *fp = NULL;

  if (!path || !out_manifest) {
    return ECHO_ERR_INVALID_ARG;
  }

  memset(out_manifest, 0, sizeof(*out_manifest));

  fp = fopen(path, "rb");
  if (!fp) {
    return ECHO_ERR_IO;
  }

  if (fread(&out_manifest->version, sizeof(out_manifest->version), 1, fp) !=
          1 ||
      fread(&out_manifest->original_size, sizeof(out_manifest->original_size),
            1, fp) != 1 ||
      fread(&out_manifest->chunk_size, sizeof(out_manifest->chunk_size), 1,
            fp) != 1 ||
      fread(&out_manifest->total_chunks, sizeof(out_manifest->total_chunks), 1,
            fp) != 1 ||
      fread(out_manifest->file_hash, sizeof(out_manifest->file_hash), 1, fp) !=
          1) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  out_manifest->chunks = (echo_chunk_info_t *)calloc(out_manifest->total_chunks,
                                                     sizeof(echo_chunk_info_t));
  if (!out_manifest->chunks) {
    fclose(fp);
    return ECHO_ERR_NOMEM;
  }

  if (fread(out_manifest->chunks, sizeof(echo_chunk_info_t),
            out_manifest->total_chunks, fp) != out_manifest->total_chunks) {
    fclose(fp);
    echo_manifest_free(out_manifest);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  return ECHO_OK;
}
