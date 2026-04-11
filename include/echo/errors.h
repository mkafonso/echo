#ifndef ECHO_ERRORS_H
#define ECHO_ERRORS_H

typedef enum echo_error {
  ECHO_OK = 0,
  ECHO_ERR_INVALID_ARG,
  ECHO_ERR_IO,
  ECHO_ERR_NOMEM,
  ECHO_ERR_CRYPTO,
  ECHO_ERR_NOT_FOUND,
  ECHO_ERR_CORRUPTED,
  ECHO_ERR_PROVIDER,
  ECHO_ERR_INTERNAL
} echo_error_t;

const char *echo_error_str(echo_error_t err);

#endif
