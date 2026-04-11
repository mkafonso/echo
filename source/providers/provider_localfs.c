#include "echo/provider_localfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct echo_localfs_ctx {
  char base_dir[1024];
} echo_localfs_ctx_t;

static echo_error_t localfs_build_path(echo_localfs_ctx_t *ctx,
                                       const char *object_name, char *out_path,
                                       size_t out_path_size) {
  int written;

  if (!ctx || !object_name || !out_path) {
    return ECHO_ERR_INVALID_ARG;
  }

  written =
      snprintf(out_path, out_path_size, "%s/%s", ctx->base_dir, object_name);
  if (written < 0 || (size_t)written >= out_path_size) {
    return ECHO_ERR_IO;
  }

  return ECHO_OK;
}

static echo_error_t localfs_put(void *ctx_void, const char *object_name,
                                const uint8_t *data, size_t len) {
  FILE *fp = NULL;
  char path[1200];
  echo_localfs_ctx_t *ctx = (echo_localfs_ctx_t *)ctx_void;

  if (!ctx || !object_name || (!data && len > 0)) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (localfs_build_path(ctx, object_name, path, sizeof(path)) != ECHO_OK) {
    return ECHO_ERR_IO;
  }

  fp = fopen(path, "wb");
  if (!fp) {
    return ECHO_ERR_IO;
  }

  if (len > 0 && fwrite(data, 1, len, fp) != len) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  return ECHO_OK;
}

static echo_error_t localfs_get(void *ctx_void, const char *object_name,
                                uint8_t **out_data, size_t *out_len) {
  FILE *fp = NULL;
  long size;
  uint8_t *buffer = NULL;
  char path[1200];
  echo_localfs_ctx_t *ctx = (echo_localfs_ctx_t *)ctx_void;

  if (!ctx || !object_name || !out_data || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (localfs_build_path(ctx, object_name, path, sizeof(path)) != ECHO_OK) {
    return ECHO_ERR_IO;
  }

  fp = fopen(path, "rb");
  if (!fp) {
    return ECHO_ERR_NOT_FOUND;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  size = ftell(fp);
  if (size < 0) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  rewind(fp);

  buffer = (uint8_t *)malloc((size_t)size);
  if (!buffer) {
    fclose(fp);
    return ECHO_ERR_NOMEM;
  }

  if (size > 0 && fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
    free(buffer);
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);

  *out_data = buffer;
  *out_len = (size_t)size;
  return ECHO_OK;
}

static echo_error_t localfs_exists(void *ctx_void, const char *object_name,
                                   int *out_exists) {
  struct stat st;
  char path[1200];
  echo_localfs_ctx_t *ctx = (echo_localfs_ctx_t *)ctx_void;

  if (!ctx || !object_name || !out_exists) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (localfs_build_path(ctx, object_name, path, sizeof(path)) != ECHO_OK) {
    return ECHO_ERR_IO;
  }

  *out_exists = (stat(path, &st) == 0);
  return ECHO_OK;
}

static void localfs_destroy(void *ctx_void) { free(ctx_void); }

static const echo_provider_vtable_t ECHO_LOCALFS_VTABLE = {
    .put = localfs_put,
    .get = localfs_get,
    .exists = localfs_exists,
    .destroy = localfs_destroy};

echo_error_t echo_provider_localfs_create(const char *base_dir,
                                          echo_provider_t *out_provider) {
  echo_localfs_ctx_t *ctx = NULL;

  if (!base_dir || !out_provider) {
    return ECHO_ERR_INVALID_ARG;
  }

  ctx = (echo_localfs_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return ECHO_ERR_NOMEM;
  }

  snprintf(ctx->base_dir, sizeof(ctx->base_dir), "%s", base_dir);

  out_provider->ctx = ctx;
  out_provider->vtable = &ECHO_LOCALFS_VTABLE;
  return ECHO_OK;
}

echo_error_t echo_provider_put(echo_provider_t *provider,
                               const char *object_name, const uint8_t *data,
                               size_t len) {
  if (!provider || !provider->vtable || !provider->vtable->put) {
    return ECHO_ERR_INVALID_ARG;
  }
  return provider->vtable->put(provider->ctx, object_name, data, len);
}

echo_error_t echo_provider_get(echo_provider_t *provider,
                               const char *object_name, uint8_t **out_data,
                               size_t *out_len) {
  if (!provider || !provider->vtable || !provider->vtable->get) {
    return ECHO_ERR_INVALID_ARG;
  }
  return provider->vtable->get(provider->ctx, object_name, out_data, out_len);
}

echo_error_t echo_provider_exists(echo_provider_t *provider,
                                  const char *object_name, int *out_exists) {
  if (!provider || !provider->vtable || !provider->vtable->exists) {
    return ECHO_ERR_INVALID_ARG;
  }
  return provider->vtable->exists(provider->ctx, object_name, out_exists);
}

void echo_provider_destroy(echo_provider_t *provider) {
  if (!provider || !provider->vtable || !provider->vtable->destroy) {
    return;
  }

  provider->vtable->destroy(provider->ctx);
  provider->ctx = NULL;
  provider->vtable = NULL;
}
