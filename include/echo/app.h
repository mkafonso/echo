#ifndef ECHO_APP_H
#define ECHO_APP_H

#include "errors.h"
#include "provider.h"

echo_error_t echo_upload_file(const char *input_path, const char *manifest_path,
                              const char *password, size_t chunk_size,
                              echo_provider_t *provider);

echo_error_t echo_upload_file_text(const char *input_path,
                                   const char *manifest_path,
                                   const char *password, size_t chunk_size,
                                   echo_provider_t *provider);

echo_error_t echo_upload_file_image(const char *input_path,
                                    const char *manifest_path,
                                    const char *password, size_t chunk_size,
                                    echo_provider_t *provider);

echo_error_t echo_upload_file_image_lsb(const char *input_path,
                                        const char *manifest_path,
                                        const char *password, size_t chunk_size,
                                        echo_provider_t *provider);

echo_error_t echo_upload_file_image_png(const char *input_path,
                                        const char *manifest_path,
                                        const char *password, size_t chunk_size,
                                        echo_provider_t *provider);

echo_error_t echo_download_file(const char *manifest_path,
                                const char *output_path, const char *password,
                                echo_provider_t *provider);

echo_error_t echo_verify_file(const char *manifest_path,
                              echo_provider_t *provider);

#endif
