#include "mod_sdk.h"

static bool
mod_init(const mod_host_api_t *host, mod_t m)
{
  mod_sdk_init(host, m);

  MOD_LOG_INFO("Hello from example mod!!!");
  return true;
}

MOD_ENTRY()
{
  static const mod_api_t api = {
    .struct_size = sizeof(mod_api_t),
    .abi_version = MOD_ABI_VERSION,
    .init        = mod_init,
  };

  return &api;
}