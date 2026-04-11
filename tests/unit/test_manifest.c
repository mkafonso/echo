#include "echo/manifest.h"

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

static void test_manifest_roundtrip_save_load(void) {
  char tmp_dir[1024];
  char path[1200];

  echo_manifest_t manifest = {0};
  echo_manifest_t loaded = {0};

  make_temp_dir(tmp_dir);
  snprintf(path, sizeof(path), "%s/manifest.bin", tmp_dir);

  assert(echo_manifest_init(&manifest, 10, 4, 3) == ECHO_OK);

  for (size_t i = 0; i < ECHO_HASH_SIZE; i++) {
    manifest.file_hash[i] = (uint8_t)i;
  }

  for (uint32_t i = 0; i < manifest.total_chunks; i++) {
    manifest.chunks[i].index = i;
    manifest.chunks[i].plain_size = 4;
    manifest.chunks[i].cipher_size = 5;
    for (size_t j = 0; j < ECHO_HASH_SIZE; j++) {
      manifest.chunks[i].hash[j] = (uint8_t)(i + j);
    }
    for (size_t j = 0; j < ECHO_NONCE_SIZE; j++) {
      manifest.chunks[i].nonce[j] = (uint8_t)(i ^ j);
    }
    snprintf(manifest.chunks[i].object_name, ECHO_OBJECT_NAME_SIZE,
             "chunk_%06u.bin", i);
  }

  assert(echo_manifest_save(path, &manifest) == ECHO_OK);
  assert(echo_manifest_load(path, &loaded) == ECHO_OK);

  assert(loaded.version == manifest.version);
  assert(loaded.original_size == manifest.original_size);
  assert(loaded.chunk_size == manifest.chunk_size);
  assert(loaded.total_chunks == manifest.total_chunks);
  assert(memcmp(loaded.file_hash, manifest.file_hash, ECHO_HASH_SIZE) == 0);

  assert(loaded.chunks != NULL);
  for (uint32_t i = 0; i < manifest.total_chunks; i++) {
    assert(loaded.chunks[i].index == manifest.chunks[i].index);
    assert(loaded.chunks[i].plain_size == manifest.chunks[i].plain_size);
    assert(loaded.chunks[i].cipher_size == manifest.chunks[i].cipher_size);
    assert(memcmp(loaded.chunks[i].hash, manifest.chunks[i].hash,
                  ECHO_HASH_SIZE) == 0);
    assert(memcmp(loaded.chunks[i].nonce, manifest.chunks[i].nonce,
                  ECHO_NONCE_SIZE) == 0);
    assert(strcmp(loaded.chunks[i].object_name,
                  manifest.chunks[i].object_name) == 0);
  }

  unlink(path);
  rmdir(tmp_dir);
  echo_manifest_free(&manifest);
  echo_manifest_free(&loaded);
}

static void test_manifest_invalid_args(void) {
  echo_manifest_t manifest = {0};
  echo_manifest_t loaded = {0};

  assert(echo_manifest_init(NULL, 0, 1, 1) == ECHO_ERR_INVALID_ARG);
  assert(echo_manifest_init(&manifest, 0, 0, 1) == ECHO_ERR_INVALID_ARG);
  assert(echo_manifest_init(&manifest, 0, 1, 0) == ECHO_ERR_INVALID_ARG);

  assert(echo_manifest_save(NULL, &manifest) == ECHO_ERR_INVALID_ARG);
  assert(echo_manifest_save("x", NULL) == ECHO_ERR_INVALID_ARG);

  assert(echo_manifest_load(NULL, &loaded) == ECHO_ERR_INVALID_ARG);
  assert(echo_manifest_load("x", NULL) == ECHO_ERR_INVALID_ARG);
}

static void test_manifest_load_not_found(void) {
  echo_manifest_t loaded = {0};
  assert(echo_manifest_load("this_file_should_not_exist.bin", &loaded) ==
         ECHO_ERR_NOT_FOUND);
}

static void test_manifest_load_corrupted_fixture(void) {
  char path[1400];
  echo_manifest_t loaded = {0};

  snprintf(path, sizeof(path), "%s/corrupted_manifest.bin", TEST_FIXTURES_DIR);
  assert(echo_manifest_load(path, &loaded) == ECHO_ERR_IO);
}

int main(void) {
  test_manifest_roundtrip_save_load();
  test_manifest_invalid_args();
  test_manifest_load_not_found();
  test_manifest_load_corrupted_fixture();
  return 0;
}
