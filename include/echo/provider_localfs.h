#ifndef ECHO_PROVIDER_LOCALFS_H
#define ECHO_PROVIDER_LOCALFS_H

#include "provider.h"

echo_error_t echo_provider_localfs_create(const char *base_dir,
                                          echo_provider_t *out_provider);

#endif
