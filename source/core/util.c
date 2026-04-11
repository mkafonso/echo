#include "echo/util.h"
#include "echo/errors.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

echo_error_t echo_read_file(const char *path, uint8_t **out_data,
                            size_t *out_len) {
  FILE *fp = NULL;
  uint8_t *buffer = NULL;
  long size = 0;

  if (!path || !out_data || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  fp = fopen(path, "rb");
  if (!fp) {
    if (errno == ENOENT) {
      return ECHO_ERR_NOT_FOUND;
    }
    return ECHO_ERR_IO;
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

echo_error_t echo_write_file(const char *path, const uint8_t *data,
                             size_t len) {
  FILE *fp = NULL;

  if (!path || (!data && len > 0)) {
    return ECHO_ERR_INVALID_ARG;
  }

  fp = fopen(path, "wb");
  if (!fp) {
    if (errno == ENOENT) {
      return ECHO_ERR_NOT_FOUND;
    }
    return ECHO_ERR_IO;
  }

  if (len > 0 && fwrite(data, 1, len, fp) != len) {
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  return ECHO_OK;
}

void echo_secure_zero(void *ptr, size_t len) {
  volatile unsigned char *p = (volatile unsigned char *)ptr;
  while (len--) {
    *p++ = 0;
  }
}
