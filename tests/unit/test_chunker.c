#include "echo/chunker.h"

#include <assert.h>
#include <string.h>

static void test_chunk_file_single_chunk(void) {
  const uint8_t data[] = {1, 2, 3, 4, 5};
  echo_chunked_file_t chunked = {0};

  assert(echo_chunk_file(data, sizeof(data), 10, &chunked) == ECHO_OK);

  assert(chunked.chunk_count == 1);
  assert(chunked.chunks != NULL);
  assert(chunked.chunk_sizes != NULL);
  assert(chunked.chunk_sizes[0] == sizeof(data));
  assert(memcmp(chunked.chunks[0], data, sizeof(data)) == 0);

  echo_chunked_file_free(&chunked);
}

static void test_chunk_file_multiple_chunks(void) {
  const uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  echo_chunked_file_t chunked = {0};

  assert(echo_chunk_file(data, sizeof(data), 4, &chunked) == ECHO_OK);

  assert(chunked.chunk_count == 3);
  assert(chunked.chunk_sizes[0] == 4);
  assert(chunked.chunk_sizes[1] == 4);
  assert(chunked.chunk_sizes[2] == 2);

  assert(memcmp(chunked.chunks[0], data, 4) == 0);
  assert(memcmp(chunked.chunks[1], data + 4, 4) == 0);
  assert(memcmp(chunked.chunks[2], data + 8, 2) == 0);

  echo_chunked_file_free(&chunked);
}

static void test_chunk_file_invalid_args(void) {
  echo_chunked_file_t chunked = {0};
  const uint8_t data[] = {1, 2, 3};

  assert(echo_chunk_file(NULL, sizeof(data), 2, &chunked) ==
         ECHO_ERR_INVALID_ARG);
  assert(echo_chunk_file(data, 0, 2, &chunked) == ECHO_ERR_INVALID_ARG);
  assert(echo_chunk_file(data, sizeof(data), 0, &chunked) ==
         ECHO_ERR_INVALID_ARG);
  assert(echo_chunk_file(data, sizeof(data), 2, NULL) == ECHO_ERR_INVALID_ARG);
}

int main(void) {
  test_chunk_file_single_chunk();
  test_chunk_file_multiple_chunks();
  test_chunk_file_invalid_args();
  return 0;
}
