#include "fake_provider.h"

#include <stdlib.h>
#include <string.h>

typedef struct fake_provider_object {
  char *name;
  uint8_t *data;
  size_t len;
  struct fake_provider_object *next;
} fake_provider_object_t;

typedef struct fake_provider_ctx {
  int put_calls;
  int fail_put_after;
  fake_provider_object_t *objects;
} fake_provider_ctx_t;

static fake_provider_object_t *fake_provider_find(fake_provider_ctx_t *ctx,
                                                  const char *name) {
  fake_provider_object_t *obj = ctx->objects;
  while (obj) {
    if (strcmp(obj->name, name) == 0) {
      return obj;
    }
    obj = obj->next;
  }
  return NULL;
}

static echo_error_t fake_put(void *ctx_void, const char *object_name,
                             const uint8_t *data, size_t len) {
  fake_provider_ctx_t *ctx = (fake_provider_ctx_t *)ctx_void;
  fake_provider_object_t *obj;
  uint8_t *copy;

  if (!ctx || !object_name || (!data && len > 0)) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (ctx->fail_put_after >= 0 && ctx->put_calls >= ctx->fail_put_after) {
    ctx->put_calls++;
    return ECHO_ERR_PROVIDER;
  }
  ctx->put_calls++;

  copy = NULL;
  if (len > 0) {
    copy = (uint8_t *)malloc(len);
    if (!copy) {
      return ECHO_ERR_NOMEM;
    }
    memcpy(copy, data, len);
  }

  obj = fake_provider_find(ctx, object_name);
  if (obj) {
    free(obj->data);
    obj->data = copy;
    obj->len = len;
    return ECHO_OK;
  }

  obj = (fake_provider_object_t *)calloc(1, sizeof(*obj));
  if (!obj) {
    free(copy);
    return ECHO_ERR_NOMEM;
  }

  obj->name = strdup(object_name);
  if (!obj->name) {
    free(copy);
    free(obj);
    return ECHO_ERR_NOMEM;
  }

  obj->data = copy;
  obj->len = len;
  obj->next = ctx->objects;
  ctx->objects = obj;
  return ECHO_OK;
}

static echo_error_t fake_get(void *ctx_void, const char *object_name,
                             uint8_t **out_data, size_t *out_len) {
  fake_provider_ctx_t *ctx = (fake_provider_ctx_t *)ctx_void;
  fake_provider_object_t *obj;
  uint8_t *copy = NULL;

  if (!ctx || !object_name || !out_data || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  obj = fake_provider_find(ctx, object_name);
  if (!obj) {
    return ECHO_ERR_NOT_FOUND;
  }

  if (obj->len > 0) {
    copy = (uint8_t *)malloc(obj->len);
    if (!copy) {
      return ECHO_ERR_NOMEM;
    }
    memcpy(copy, obj->data, obj->len);
  }

  *out_data = copy;
  *out_len = obj->len;
  return ECHO_OK;
}

static echo_error_t fake_exists(void *ctx_void, const char *object_name,
                                int *out_exists) {
  fake_provider_ctx_t *ctx = (fake_provider_ctx_t *)ctx_void;

  if (!ctx || !object_name || !out_exists) {
    return ECHO_ERR_INVALID_ARG;
  }

  *out_exists = (fake_provider_find(ctx, object_name) != NULL);
  return ECHO_OK;
}

static void fake_destroy(void *ctx_void) {
  fake_provider_ctx_t *ctx = (fake_provider_ctx_t *)ctx_void;
  fake_provider_object_t *obj;

  if (!ctx) {
    return;
  }

  obj = ctx->objects;
  while (obj) {
    fake_provider_object_t *next = obj->next;
    free(obj->name);
    free(obj->data);
    free(obj);
    obj = next;
  }

  free(ctx);
}

static const echo_provider_vtable_t FAKE_PROVIDER_VTABLE = {
    .put = fake_put, .get = fake_get, .exists = fake_exists, .destroy = fake_destroy};

echo_error_t fake_provider_create(echo_provider_t *out_provider) {
  fake_provider_ctx_t *ctx;

  if (!out_provider) {
    return ECHO_ERR_INVALID_ARG;
  }

  ctx = (fake_provider_ctx_t *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return ECHO_ERR_NOMEM;
  }

  ctx->put_calls = 0;
  ctx->fail_put_after = -1;
  ctx->objects = NULL;

  out_provider->ctx = ctx;
  out_provider->vtable = &FAKE_PROVIDER_VTABLE;
  return ECHO_OK;
}

void fake_provider_set_fail_put_after(echo_provider_t *provider, int call_index) {
  fake_provider_ctx_t *ctx;

  if (!provider || !provider->ctx) {
    return;
  }

  ctx = (fake_provider_ctx_t *)provider->ctx;
  ctx->fail_put_after = call_index;
}
