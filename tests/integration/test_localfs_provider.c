#include "echo/provider_localfs.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void make_temp_dir(char out_dir[1024]) {
  const char *tmp = getenv("TMPDIR");
  if (!tmp || tmp[0] == '\0') {
    tmp = "/tmp";
  }

  snprintf(out_dir, 1024, "%s/echo_tests_XXXXXX", tmp);
  assert(mkdtemp(out_dir) != NULL);
}

static void test_localfs_put_get_exists_roundtrip(void) {
  char base_dir[1024];
  echo_provider_t provider = {0};

  const char *name = "obj.bin";
  const uint8_t data[] = {9, 8, 7, 6, 5};

  uint8_t *got = NULL;
  size_t got_len = 0;
  int exists = 0;

  make_temp_dir(base_dir);
  assert(echo_provider_localfs_create(base_dir, &provider) == ECHO_OK);

  assert(echo_provider_put(&provider, name, data, sizeof(data)) == ECHO_OK);

  assert(echo_provider_exists(&provider, name, &exists) == ECHO_OK);
  assert(exists);

  assert(echo_provider_get(&provider, name, &got, &got_len) == ECHO_OK);
  assert(got_len == sizeof(data));
  assert(memcmp(got, data, sizeof(data)) == 0);

  free(got);
  echo_provider_destroy(&provider);
  rmdir(base_dir);
}

static void test_localfs_get_not_found(void) {
  char base_dir[1024];
  echo_provider_t provider = {0};
  uint8_t *got = NULL;
  size_t got_len = 0;

  make_temp_dir(base_dir);
  assert(echo_provider_localfs_create(base_dir, &provider) == ECHO_OK);

  assert(echo_provider_get(&provider, "missing.bin", &got, &got_len) ==
         ECHO_ERR_NOT_FOUND);

  free(got);
  echo_provider_destroy(&provider);
  rmdir(base_dir);
}

static void test_localfs_invalid_args(void) {
  echo_provider_t provider = {0};
  assert(echo_provider_localfs_create(NULL, &provider) == ECHO_ERR_INVALID_ARG);
  assert(echo_provider_localfs_create("x", NULL) == ECHO_ERR_INVALID_ARG);
}

int main(void) {
  test_localfs_put_get_exists_roundtrip();
  test_localfs_get_not_found();
  test_localfs_invalid_args();
  return 0;
}
