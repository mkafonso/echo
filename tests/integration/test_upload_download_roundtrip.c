#include "echo/app.h"
#include "echo/provider_localfs.h"
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

static void test_upload_download_roundtrip_binary(void) {
  char base_dir[1024];
  char manifest_path[1200];
  char output_path[1200];
  char input_path[1200];

  echo_provider_t provider = {0};

  uint8_t *in_data = NULL;
  size_t in_len = 0;
  uint8_t *out_data = NULL;
  size_t out_len = 0;

  make_temp_dir(base_dir);
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.bin", base_dir);
  snprintf(output_path, sizeof(output_path), "%s/out.bin", base_dir);
  snprintf(input_path, sizeof(input_path), "%s/sample.txt", TEST_FIXTURES_DIR);

  assert(echo_provider_localfs_create(base_dir, &provider) == ECHO_OK);

  assert(echo_upload_file(input_path, manifest_path, "pw", 8, &provider) ==
         ECHO_OK);

  assert(echo_verify_file(manifest_path, &provider) == ECHO_OK);

  assert(echo_download_file(manifest_path, output_path, "pw", &provider) ==
         ECHO_OK);

  assert(echo_read_file(input_path, &in_data, &in_len) == ECHO_OK);
  assert(echo_read_file(output_path, &out_data, &out_len) == ECHO_OK);

  assert(out_len == in_len);
  assert(memcmp(out_data, in_data, in_len) == 0);

  free(in_data);
  free(out_data);
  unlink(manifest_path);
  unlink(output_path);
  echo_provider_destroy(&provider);
  rmdir(base_dir);
}

int main(void) {
  test_upload_download_roundtrip_binary();
  return 0;
}
