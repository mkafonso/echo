#ifndef TEST_FAKE_PROVIDER_H
#define TEST_FAKE_PROVIDER_H

#include "echo/provider.h"

echo_error_t fake_provider_create(echo_provider_t *out_provider);

void fake_provider_set_fail_put_after(echo_provider_t *provider, int call_index);

#endif
