#ifndef HOOK_CHAIN_H
#define HOOK_CHAIN_H

#include "types.h"

typedef struct hook_chain_s        hook_chain_t;
typedef struct hook_chain_handle_s hook_chain_handle_t;
struct hook_chain_handle_s {
  bool                 created;
  bool                 enabled;
  void                *target;
  void                *detour;
  void               **original;
  void                *thunk;
  void *volatile       next_detour;
  hook_chain_t        *chain;
  hook_chain_handle_t *chain_next;
  hook_chain_handle_t *enabled_prev;
  hook_chain_handle_t *enabled_next;
};

void
hook_chain_init(void);

bool
hook_chain_create(hook_chain_handle_t *handle, void *target, void *detour, void **original);
bool
hook_chain_enable(hook_chain_handle_t *handle);
bool
hook_chain_disable(hook_chain_handle_t *handle);
bool
hook_chain_remove(hook_chain_handle_t *handle);

#endif /* HOOK_CHAIN_H */
