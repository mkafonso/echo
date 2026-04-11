#include "echo/util.h"

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

static void test_util_write_read_roundtrip(void) {
  char tmp_dir[1024];
  char path[1200];

  const uint8_t data[] = {0, 1, 2, 3, 4, 5, 250, 251, 252, 253, 254, 255};
  uint8_t *read_back = NULL;
  size_t read_len = 0;

  make_temp_dir(tmp_dir);
  snprintf(path, sizeof(path), "%s/data.bin", tmp_dir);

  assert(echo_write_file(path, data, sizeof(data)) == ECHO_OK);
  assert(echo_read_file(path, &read_back, &read_len) == ECHO_OK);

  assert(read_len == sizeof(data));
  assert(memcmp(read_back, data, sizeof(data)) == 0);

  free(read_back);
  unlink(path);
  rmdir(tmp_dir);
}

static void test_util_invalid_args(void) {
  char tmp_dir[1024];
  char path[1200];
  uint8_t *data = NULL;
  size_t len = 0;
  const uint8_t bytes[] = {1, 2, 3};

  assert(echo_read_file(NULL, &data, &len) == ECHO_ERR_INVALID_ARG);
  assert(echo_read_file("x", NULL, &len) == ECHO_ERR_INVALID_ARG);
  assert(echo_read_file("x", &data, NULL) == ECHO_ERR_INVALID_ARG);

  assert(echo_write_file(NULL, bytes, sizeof(bytes)) == ECHO_ERR_INVALID_ARG);
  assert(echo_write_file("x", NULL, 1) == ECHO_ERR_INVALID_ARG);

  make_temp_dir(tmp_dir);
  snprintf(path, sizeof(path), "%s/empty.bin", tmp_dir);
  assert(echo_write_file(path, bytes, 0) == ECHO_OK);
  unlink(path);
  rmdir(tmp_dir);
}

static void test_util_read_not_found(void) {
  uint8_t *data = NULL;
  size_t len = 0;

  assert(echo_read_file("this_file_should_not_exist.bin", &data, &len) ==
         ECHO_ERR_NOT_FOUND);
}

static void test_util_write_not_found_missing_dir(void) {
  char tmp_dir[1024];
  char path[1200];
  const uint8_t bytes[] = {7, 8, 9};

  make_temp_dir(tmp_dir);
  snprintf(path, sizeof(path), "%s/missing_dir/file.bin", tmp_dir);

  assert(echo_write_file(path, bytes, sizeof(bytes)) == ECHO_ERR_NOT_FOUND);

  rmdir(tmp_dir);
}

int main(void) {
  test_util_write_read_roundtrip();
  test_util_invalid_args();
  test_util_read_not_found();
  test_util_write_not_found_missing_dir();
  return 0;
}
