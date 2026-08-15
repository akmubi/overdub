#include "hook_chain.h"
#include "vendor_minhook.h"

#include <windows.h>

struct hook_chain_s {
  void                *target;
  void                *trampoline;
  void                *entry_thunk;
  void *volatile       head_detour;
  int                  member_count;
  int                  enabled_count;
  hook_chain_handle_t *members;
  hook_chain_handle_t *enabled_head;
  hook_chain_t        *next;
};

static CRITICAL_SECTION g_hook_chain_lock   = {0};
static hook_chain_t    *g_hook_chains       = NULL;
static bool             g_hook_chain_inited = false;

static void
hook_chain_store(void *volatile *slot, void *value)
{
  InterlockedExchangePointer((PVOID volatile *)slot, value);
}

static void *
hook_chain_thunk_alloc(void *volatile *next_slot)
{
  uint8_t *code = (uint8_t *)VirtualAlloc(NULL, 16, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!code) {
    return NULL;
  }

  /* mov rax, next_slot; jmp qword ptr [rax] */
  code[0] = 0x48;
  code[1] = 0xB8;
  mem_copy(code + 2, &next_slot, sizeof(next_slot));
  code[10] = 0xFF;
  code[11] = 0x20;

  DWORD old_protect;
  if (!VirtualProtect(code, 16, PAGE_EXECUTE_READ, &old_protect)) {
    VirtualFree(code, 0, MEM_RELEASE);
    return NULL;
  }

  FlushInstructionCache(GetCurrentProcess(), code, 16);
  return code;
}

static void
hook_chain_thunk_free(void *thunk)
{
  if (thunk) {
    VirtualFree(thunk, 0, MEM_RELEASE);
  }
}

static hook_chain_t *
hook_chain_find(void *target)
{
  for (hook_chain_t *chain = g_hook_chains; chain; chain = chain->next) {
    if (chain->target == target) {
      return chain;
    }
  }
  return NULL;
}

static void
hook_chain_destroy(hook_chain_t *chain)
{
  hook_chain_t **link = &g_hook_chains;
  while (*link && *link != chain) {
    link = &(*link)->next;
  }

  if (*link == chain) {
    *link = chain->next;
  }

  hook_chain_thunk_free(chain->entry_thunk);
  HeapFree(GetProcessHeap(), 0, chain);
}

static bool
hook_chain_create_locked(hook_chain_handle_t *handle, void *target, void *detour, void **original)
{
  bool          new_chain = false;
  hook_chain_t *chain     = hook_chain_find(target);
  if (!chain) {
    chain = (hook_chain_t *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*chain));
    if (!chain) {
      return false;
    }

    chain->target      = target;
    chain->entry_thunk = hook_chain_thunk_alloc(&chain->head_detour);
    if (!chain->entry_thunk) {
      HeapFree(GetProcessHeap(), 0, chain);
      return false;
    }

    MH_STATUS status = MH_CreateHook(target, chain->entry_thunk, &chain->trampoline);
    if (status != MH_OK) {
      hook_chain_thunk_free(chain->entry_thunk);
      HeapFree(GetProcessHeap(), 0, chain);
      return false;
    }

    hook_chain_store(&chain->head_detour, chain->trampoline);
    chain->next   = g_hook_chains;
    g_hook_chains = chain;
    new_chain     = true;
  }

  handle->next_detour = chain->trampoline;
  handle->thunk       = hook_chain_thunk_alloc(&handle->next_detour);
  if (!handle->thunk) {
    if (new_chain) {
      MH_RemoveHook(target);
      hook_chain_destroy(chain);
    }
    return false;
  }

  handle->created      = true;
  handle->target       = target;
  handle->detour       = detour;
  handle->original     = original;
  handle->chain        = chain;
  handle->chain_next   = chain->members;
  chain->members       = handle;
  chain->member_count += 1;
  *original            = handle->thunk;
  return true;
}

static bool
hook_chain_enable_locked(hook_chain_handle_t *handle)
{
  if (handle->enabled) {
    return true;
  }

  hook_chain_t        *chain    = handle->chain;
  hook_chain_handle_t *old_head = chain->enabled_head;

  hook_chain_store(&handle->next_detour, old_head ? old_head->detour : chain->trampoline);
  handle->enabled_prev = NULL;
  handle->enabled_next = old_head;
  if (old_head) {
    old_head->enabled_prev = handle;
  }

  chain->enabled_head = handle;
  hook_chain_store(&chain->head_detour, handle->detour);

  if (chain->enabled_count == 0) {
    MH_STATUS status = MH_EnableHook(chain->target);
    if (status != MH_OK) {
      chain->enabled_head = old_head;
      if (old_head) {
        old_head->enabled_prev = NULL;
      }

      handle->enabled_next = NULL;
      hook_chain_store(&handle->next_detour, chain->trampoline);
      hook_chain_store(&chain->head_detour, chain->trampoline);
      return false;
    }
  }

  handle->enabled       = true;
  chain->enabled_count += 1;
  return true;
}

static bool
hook_chain_disable_locked(hook_chain_handle_t *handle)
{
  if (!handle->enabled) {
    return true;
  }

  hook_chain_t *chain = handle->chain;
  if (chain->enabled_count == 1 && MH_DisableHook(chain->target) != MH_OK) {
    return false;
  }

  hook_chain_handle_t *prev = handle->enabled_prev;
  hook_chain_handle_t *next = handle->enabled_next;

  if (prev) {
    hook_chain_store(&prev->next_detour, next ? next->detour : chain->trampoline);
    prev->enabled_next = next;
  } else {
    chain->enabled_head = next;
    hook_chain_store(&chain->head_detour, next ? next->detour : chain->trampoline);
  }

  if (next) {
    next->enabled_prev = prev;
  }

  handle->enabled      = false;
  handle->enabled_prev = NULL;
  handle->enabled_next = NULL;
  hook_chain_store(&handle->next_detour, next ? next->detour : chain->trampoline);
  chain->enabled_count -= 1;
  return true;
}

static bool
hook_chain_remove_locked(hook_chain_handle_t *handle)
{
  if (!hook_chain_disable_locked(handle)) {
    return false;
  }

  hook_chain_t *chain = handle->chain;
  if (chain->member_count == 1 && MH_RemoveHook(chain->target) != MH_OK) {
    return false;
  }

  hook_chain_handle_t **link = &chain->members;
  while (*link && *link != handle) {
    link = &(*link)->chain_next;
  }

  if (*link != handle) {
    return false;
  }

  *link                = handle->chain_next;
  chain->member_count -= 1;
  hook_chain_thunk_free(handle->thunk);
  mem_zero(handle, sizeof(*handle));

  if (chain->member_count == 0) {
    hook_chain_destroy(chain);
  }

  return true;
}

void
hook_chain_init(void)
{
  if (!g_hook_chain_inited) {
    InitializeCriticalSection(&g_hook_chain_lock);
    g_hook_chain_inited = true;
  }
}

bool
hook_chain_create(hook_chain_handle_t *handle, void *target, void *detour, void **original)
{
  if (!g_hook_chain_inited || !handle || handle->created || !target || !detour || !original) {
    return false;
  }

  bool result = false;
  EnterCriticalSection(&g_hook_chain_lock);
  {
    result = hook_chain_create_locked(handle, target, detour, original);
  }
  LeaveCriticalSection(&g_hook_chain_lock);
  return result;
}

bool
hook_chain_enable(hook_chain_handle_t *handle)
{
  if (!g_hook_chain_inited || !handle || !handle->created) {
    return false;
  }

  bool result = false;
  EnterCriticalSection(&g_hook_chain_lock);
  {
    result = hook_chain_enable_locked(handle);
  }
  LeaveCriticalSection(&g_hook_chain_lock);
  return result;
}

bool
hook_chain_disable(hook_chain_handle_t *handle)
{
  if (!g_hook_chain_inited || !handle || !handle->created) {
    return false;
  }

  bool result = false;
  EnterCriticalSection(&g_hook_chain_lock);
  {
    result = hook_chain_disable_locked(handle);
  }
  LeaveCriticalSection(&g_hook_chain_lock);
  return result;
}

bool
hook_chain_remove(hook_chain_handle_t *handle)
{
  if (!g_hook_chain_inited || !handle || !handle->created) {
    return false;
  }

  bool result = false;
  EnterCriticalSection(&g_hook_chain_lock);
  {
    result = hook_chain_remove_locked(handle);
  }
  LeaveCriticalSection(&g_hook_chain_lock);
  return result;
}
