#include "echo/errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static echo_error_t echo_read_text_file(const char *path, char **out_text,
                                        size_t *out_len) {
  FILE *fp;
  long size;
  char *buf;

  if (!path || !out_text || !out_len) {
    return ECHO_ERR_INVALID_ARG;
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

  buf = (char *)malloc((size_t)size + 1);
  if (!buf) {
    fclose(fp);
    return ECHO_ERR_NOMEM;
  }

  if (size > 0 && fread(buf, 1, (size_t)size, fp) != (size_t)size) {
    free(buf);
    fclose(fp);
    return ECHO_ERR_IO;
  }

  fclose(fp);
  buf[(size_t)size] = '\0';
  *out_text = buf;
  *out_len = (size_t)size;
  return ECHO_OK;
}

echo_error_t echo_stego_load_default_corpus(char **out_text, size_t *out_len) {
  const char *env = getenv("ECHO_STEGO_CORPUS");
  echo_error_t err;

  if (!out_text || !out_len) {
    return ECHO_ERR_INVALID_ARG;
  }

  if (env && env[0]) {
    err = echo_read_text_file(env, out_text, out_len);
    if (err == ECHO_OK) {
      return ECHO_OK;
    }
  }

#ifdef ECHO_ASSETS_DIR
  {
    char path[2048];
    int n = snprintf(path, sizeof(path), "%s/corpus/shrek_pt.txt",
                     ECHO_ASSETS_DIR);
    if (n > 0 && (size_t)n < sizeof(path)) {
      err = echo_read_text_file(path, out_text, out_len);
      if (err == ECHO_OK) {
        return ECHO_OK;
      }
    }
  }
#endif

  err = echo_read_text_file("../assets/corpus/shrek_pt.txt", out_text, out_len);
  if (err == ECHO_OK) {
    return ECHO_OK;
  }

  err = echo_read_text_file("../../assets/corpus/shrek_pt.txt", out_text, out_len);
  if (err == ECHO_OK) {
    return ECHO_OK;
  }

  *out_text = NULL;
  *out_len = 0;
  return ECHO_ERR_NOT_FOUND;
}
