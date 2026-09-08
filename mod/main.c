#include "mod.h"

static bool
mod_init(mod_handle_t mod)
{
  MOD_LOG_INFO(mod, "Hello from example mod!!!");
  return true;
}

MOD_ABI_VERSION_ENTRY()
{
  return MOD_ABI_VERSION;
}

MOD_ENTRY()
{
  static const mod_api_t api = {
    .struct_size = sizeof(mod_api_t),
    .init        = mod_init,
  };

  return &api;
}
