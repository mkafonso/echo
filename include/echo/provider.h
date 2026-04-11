#ifndef ECHO_PROVIDER_H
#define ECHO_PROVIDER_H

#include "errors.h"
#include <stddef.h>
#include <stdint.h>

typedef struct echo_provider echo_provider_t;

typedef struct echo_provider_vtable {
  echo_error_t (*put)(void *ctx, const char *object_name, const uint8_t *data,
                      size_t len);

  echo_error_t (*get)(void *ctx, const char *object_name, uint8_t **out_data,
                      size_t *out_len);

  echo_error_t (*exists)(void *ctx, const char *object_name, int *out_exists);

  void (*destroy)(void *ctx);
} echo_provider_vtable_t;

struct echo_provider {
  void *ctx;
  const echo_provider_vtable_t *vtable;
};

echo_error_t echo_provider_put(echo_provider_t *provider,
                               const char *object_name, const uint8_t *data,
                               size_t len);

echo_error_t echo_provider_get(echo_provider_t *provider,
                               const char *object_name, uint8_t **out_data,
                               size_t *out_len);

echo_error_t echo_provider_exists(echo_provider_t *provider,
                                  const char *object_name, int *out_exists);

void echo_provider_destroy(echo_provider_t *provider);

#endif
