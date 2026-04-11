#include "echo/app.h"
#include "echo/errors.h"
#include "echo/provider_localfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *echo_error_str(echo_error_t err) {
  switch (err) {
  case ECHO_OK:
    return "ok";
  case ECHO_ERR_INVALID_ARG:
    return "invalid argument";
  case ECHO_ERR_IO:
    return "io error";
  case ECHO_ERR_NOMEM:
    return "out of memory";
  case ECHO_ERR_CRYPTO:
    return "crypto error";
  case ECHO_ERR_NOT_FOUND:
    return "not found";
  case ECHO_ERR_CORRUPTED:
    return "corrupted data";
  case ECHO_ERR_PROVIDER:
    return "provider error";
  case ECHO_ERR_INTERNAL:
    return "internal error";
  default:
    return "unknown error";
  }
}

static void print_usage(const char *prog) {
  const char *name = (prog && prog[0]) ? prog : "echo";

  fprintf(stderr,
          "%s\n"
          "\n"
          "Usage:\n"
          "  %s <command> [args]\n"
          "\n"
          "Commands:\n"
          "  upload    <input_file> <manifest_file> <password> <storage_dir> "
          "<chunk_size>\n"
          "  upload-text  <input_file> <manifest_file> <password> <storage_dir> "
          "<chunk_size>\n"
          "  download  <manifest_file> <output_file> <password> <storage_dir>\n"
          "  verify    <manifest_file> <storage_dir>\n"
          "\n"
          "Examples:\n"
          "  %s upload ./arquivo.zip ./manifest.bin minhaSenha ./storage 65536\n"
          "  %s upload-text ./arquivo.zip ./manifest.bin minhaSenha ./storage 65536\n"
          "  %s download ./manifest.bin ./arquivo.out minhaSenha ./storage\n"
          "  %s verify ./manifest.bin ./storage\n"
          "\n"
          "Notes:\n"
          "  Paths are resolved relative to the current working directory.\n",
          name, name, name, name, name, name);
}

int main(int argc, char **argv) {
  echo_error_t err;
  echo_provider_t provider = {0};

  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "upload") == 0) {
    size_t chunk_size;

    if (argc != 7) {
      print_usage(argv[0]);
      return 1;
    }

    chunk_size = (size_t)strtoull(argv[6], NULL, 10);

    err = echo_provider_localfs_create(argv[5], &provider);
    if (err != ECHO_OK) {
      fprintf(stderr, "provider error: %s\n", echo_error_str(err));
      return 1;
    }

    err = echo_upload_file(argv[2], argv[3], argv[4], chunk_size, &provider);
    echo_provider_destroy(&provider);

    if (err != ECHO_OK) {
      fprintf(stderr, "upload error: %s\n", echo_error_str(err));
      return 1;
    }

    printf("upload completed successfully\n");
    return 0;
  }

  if (strcmp(argv[1], "upload-text") == 0) {
    size_t chunk_size;

    if (argc != 7) {
      print_usage(argv[0]);
      return 1;
    }

    chunk_size = (size_t)strtoull(argv[6], NULL, 10);

    err = echo_provider_localfs_create(argv[5], &provider);
    if (err != ECHO_OK) {
      fprintf(stderr, "provider error: %s\n", echo_error_str(err));
      return 1;
    }

    err =
        echo_upload_file_text(argv[2], argv[3], argv[4], chunk_size, &provider);
    echo_provider_destroy(&provider);

    if (err != ECHO_OK) {
      fprintf(stderr, "upload error: %s\n", echo_error_str(err));
      return 1;
    }

    printf("upload completed successfully\n");
    return 0;
  }

  if (strcmp(argv[1], "download") == 0) {
    if (argc != 6) {
      print_usage(argv[0]);
      return 1;
    }

    err = echo_provider_localfs_create(argv[5], &provider);
    if (err != ECHO_OK) {
      fprintf(stderr, "provider error: %s\n", echo_error_str(err));
      return 1;
    }

    err = echo_download_file(argv[2], argv[3], argv[4], &provider);
    echo_provider_destroy(&provider);

    if (err != ECHO_OK) {
      fprintf(stderr, "download error: %s\n", echo_error_str(err));
      return 1;
    }

    printf("download completed successfully\n");
    return 0;
  }

  if (strcmp(argv[1], "verify") == 0) {
    if (argc != 4) {
      print_usage(argv[0]);
      return 1;
    }

    err = echo_provider_localfs_create(argv[3], &provider);
    if (err != ECHO_OK) {
      fprintf(stderr, "provider error: %s\n", echo_error_str(err));
      return 1;
    }

    err = echo_verify_file(argv[2], &provider);
    echo_provider_destroy(&provider);

    if (err != ECHO_OK) {
      fprintf(stderr, "verify error: %s\n", echo_error_str(err));
      return 1;
    }

    printf("all chunks are present\n");
    return 0;
  }

  print_usage(argv[0]);
  return 1;
}
