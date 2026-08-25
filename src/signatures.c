#include "signatures.h"

#include "globals.h"
#include "hook_chain.h"
#include "input.h"
#include "log.h"
#include "sigscan.h"

#include <windows.h>

#define UE_FUNC_NORMAL(NAME, RET, ...) \
  NAME##_fn_t NAME = NULL;
#define UE_FUNC_HOOKED(NAME, RET, ...) \
  NAME##_fn_t NAME        = NULL;      \
  NAME##_fn_t NAME##_real = NULL;

#include "unreal_funcs.inc"

#define SIG_ENTRY_NORMAL(ENTRY_TYPE, NAME, PATTERN, ...)            \
  {                                                                 \
    .entry    = SIG_ENTRY_##ENTRY_TYPE(NAME, PATTERN, __VA_ARGS__), \
    .detour   = NULL,                                               \
    .original = NULL,                                               \
  }

#define SIG_ENTRY_PATCHED(ENTRY_TYPE, NAME, PATCH_FN, PATTERN, ...) \
  {                                                                 \
    .entry    = SIG_ENTRY_##ENTRY_TYPE(NAME, PATTERN, __VA_ARGS__), \
    .detour   = NULL,                                               \
    .original = NULL,                                               \
    .patch    = PATCH_FN,                                           \
  }

#define SIG_ENTRY_HOOKED(ENTRY_TYPE, NAME, PATTERN, ...)            \
  {                                                                 \
    .entry    = SIG_ENTRY_##ENTRY_TYPE(NAME, PATTERN, __VA_ARGS__), \
    .detour   = (LPVOID)NAME##_hook,                                \
    .original = (LPVOID *)&NAME##_real,                             \
  }

enum {
  SIG_KIND_EARLY,
  SIG_KIND_MAIN,
  SIG_KIND_INPUT,
  SIG_KIND_MAX,
};

typedef void (*patch_fn_t)(sigscan_entry_t *entry);

typedef struct signature_s {
  sigscan_entry_t     entry;
  void              **original;
  void               *detour;
  patch_fn_t          patch;
  hook_chain_handle_t hook;
  bool                resolved;
  bool                hook_created;
  bool                hook_enabled;
} signature_t;

static void
patch_get_pak_signkey_helper(sigscan_entry_t *e)
{
  if (!e || !e->addr) {
    return;
  }

  // early return with FALSE i.e. IsSigningEnabled() ==> FALSE
  uint8_t patch[] = {
    0x31,
    0xC0, // xor eax, eax
    0xC3, // ret
  };

  DWORD old_protect = 0;
  if (VirtualProtect(e->addr, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
    mem_copy(e->addr, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), e->addr, sizeof(patch));
    VirtualProtect(e->addr, sizeof(patch), old_protect, &old_protect);
  }
}

static void
patch_setup_signed_pak_reader(sigscan_entry_t *e)
{
  if (!e || !e->addr) {
    return;
  }

  uint8_t patch[] = {
    0x48,
    0x89,
    0xD0, // mov rax, rdx
    0xC3, // ret
  };

  DWORD old_protect = 0;
  if (VirtualProtect(e->addr, sizeof(patch), PAGE_EXECUTE_READWRITE, &old_protect)) {
    mem_copy(e->addr, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), e->addr, sizeof(patch));
    VirtualProtect(e->addr, sizeof(patch), old_protect, &old_protect);
  }
}

static signature_t g_early_sigs[] = {
  SIG_ENTRY_PATCHED(DIRECT, get_pak_signkey_helper,  patch_get_pak_signkey_helper,  "48 83 EC ? E8 ? ? ? ? 83 78 ? 00",                                                                                  0x0274C4D0, 0x026D2420),
  SIG_ENTRY_PATCHED(DIRECT, setup_signed_pak_reader, patch_setup_signed_pak_reader, "48 89 5C 24 ?? 48 89 6C 24 ?? 48 89 74 24 ?? 57 41 54 41 55 41 56 41 57 48 83 EC ?? 80 B9 ?? ?? ?? ?? 00 4D 8B E0", 0x0400D950, 0x03E23700),
};

static signature_t g_main_sigs[] = {
  SIG_ENTRY_HOOKED(CALL_ARG,   pak_file_mount,                       "E8 ? ? ? ? 84 C0 74 ? 41 FF C5 FF C6",                                                                                                                                                            0x0401689D, 0x03E2C69D),
  SIG_ENTRY_HOOKED(DIRECT,     io_dispatcher_mount,                  "40 53 41 55 41 57 48 81 EC ? ? ? ? 48 8B 05",                                                                                                                                                     0x02745980, 0x026CB8D0),
  SIG_ENTRY_HOOKED(DIRECT,     pak_precacher_add_new_block,          "48 89 5C 24 ?? 48 89 74 24 ?? 48 89 7C 24 ?? 55 41 54 41 55 41 56 41 57 48 8D AC 24 ?? ?? ?? ?? 48 81 EC ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 ?? ?? ?? ?? 45 33 C9",                0x00000000, 0x03E1CF90),
  SIG_ENTRY_HOOKED(DIRECT,     game_engine_tick,                     "48 8B C4 44 88 40 ? 48 89 48 ? 55 53",                                                                                                                                                            0x0434A5A0, 0x04162180),
  SIG_ENTRY_HOOKED(DIRECT,     process_event,                        "40 55 56 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 48 8D 6C 24 ? 48 89 9D ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 85 ? ? ? ? 8B 41",                                                            0x029FDCD0, 0x02981E80),
  SIG_ENTRY_HOOKED(DIRECT,     ufunction_invoke,                     "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 48 8B 59 ? 4D 8B F1",                                                                                                    0x0289B0B0, 0x0281F2B0),
  SIG_ENTRY_HOOKED(DIRECT,     d3d12_viewport_init,                  "48 89 74 24 ? 48 89 7C 24 ? 55 41 54 41 55 41 56 41 57 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 4C 8B 69",                                                             0x02D44E20, 0x02C98770),
  SIG_ENTRY_NORMAL(LEA_ADDR,   globals.uobjects,                     "4C 8D 1D ? ? ? ? 89 41",                                                                                                                                                                          0x00FDCEF0, 0x00FA15D0),
  SIG_ENTRY_NORMAL(LEA_ADDR,   globals.name_pool,                    "48 8D 05 ? ? ? ? EB ? 48 8D 0D ? ? ? ? E8 ? ? ? ? C6 05 ? ? ? ? ? 8B 80 ? ? ? ? 83 8B",                                                                                                           0x042D26E4, 0x040E9DA4),
  SIG_ENTRY_NORMAL(MOV64_ADDR, globals.gworld_ptr,                   "48 8B 05 ? ? ? ? 4C 8D 44 24 ? 48 8D 54 24 ? 48 89 44 24 ? 48 8B CB",                                                                                                                             0x047AD35A, 0x045C489A),
  SIG_ENTRY_NORMAL(LEA_ADDR,   globals.natives,                      "48 8D 05 ? ? ? ? B9 ? ? ? ? 0F 1F 40 ? 66 0F 1F 84 00 ? ? ? ? ? ? ? 48 89 50 ? 48 89 50 ? 48 8D 40 ? 48 89 50 ? 48 89 50 ? 48 89 50 ? 48 89 50 ? 48 89 50 ? 48 83 E9 ? 75 ? 8B 05 ? ? ? ? 33 C9", 0x00A062C7, 0x009BC917),
  SIG_ENTRY_NORMAL(LEA_ADDR,   globals.uobject_vtable,               "48 8D 05 ? ? ? ? ? ? ? 48 8B FA 48 8B D9",                                                                                                                                                        0x02A1A6E1, 0x0299E821),
  SIG_ENTRY_NORMAL(MOV64_ADDR, globals.gameplay_tags_manager_ptr,    "48 8B 0D ? ? ? ? B2 ? E8 ? ? ? ? 48 8B 0D ? ? ? ? E8",                                                                                                                                            0x03FC72E3, 0x03DDD933),
  SIG_ENTRY_NORMAL(LEA_ADDR,   globals.on_gameplay_tag_tree_changed, "48 8D 0D ? ? ? ? 41 C6 84 24",                                                                                                                                                                    0x03FC467D, 0x03DDACCD),
  SIG_ENTRY_NORMAL(DIRECT,     static_load_object,                   "4C 89 4C 24 ? 48 89 54 24 ? 48 89 4C 24 ? 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC ? 33 FF 49 8B F0",                                                                                0x02A14930, 0x02998AC0),
  SIG_ENTRY_NORMAL(DIRECT,     static_construct_object,              "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 41 56 41 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? ? ? ? 48 8B D9",                                                          0x02A1B9F0, 0x0299FB10),
  SIG_ENTRY_NORMAL(DIRECT,     create_default_object,                "4C 8B DC 55 57 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 48 83 B9 ? ? ? ? 00 48 8B F9 0F 85",                                                                  0x02896DE0, 0x0281B000),
  SIG_ENTRY_NORMAL(DIRECT,     static_load_class,                    "40 55 53 57 41 56 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 8B BD",                                                                                         0x02A15D70, 0x02999EF0),
  SIG_ENTRY_NORMAL(DIRECT,     fname_construct,                      "48 89 5C 24 ? 57 48 83 EC ? 48 8B D9 48 89 54 24 ? 33 C9",                                                                                                                                        0x027EF960, 0x027758B0),
  SIG_ENTRY_NORMAL(DIRECT,     add_uobject_delete_listener,          "48 89 5C 24 ? 57 48 83 EC ? 48 8D 0D ? ? ? ? 48 8B FA FF 15",                                                                                                                                     0x02A0CFF0, 0x02991170),
  SIG_ENTRY_NORMAL(CALL_ARG,   tarray_grow,                          "E8 ? ? ? ? 48 8B 05 ? ? ? ? 48 8D 0D ? ? ? ? ? ? ? ? 48 8B 5C 24 ? 48 83 C4 ? 5F 48 FF 25 ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? ? 40 53",                                                           0x02A0D029, 0x029911A9),
  SIG_ENTRY_NORMAL(CALL_ARG,   tarray_shrink,                        "E8 ? ? ? ? 48 8D 0D ? ? ? ? 48 83 C4 ? 5B 48 FF 25 ? ? ? ? ? 8B 42",                                                                                                                              0x02A0D107, 0x02991287),
  SIG_ENTRY_NORMAL(DIRECT,     get_mcast_sparse_delegate,            "48 89 5C 24 ? 57 48 83 EC ? 48 8B F9 48 8B DA 48 8D 0D ? ? ? ? FF 15 ? ? ? ? 4C 8B C7",                                                                                                           0x02A06FF0, 0x0298B190),
  SIG_ENTRY_NORMAL(DIRECT,     fscript_map_add_pair,                 "40 55 56 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 0F B6 41",                                                                                                  0x02A26830, 0x029AA910),
  SIG_ENTRY_NORMAL(DIRECT,     fscript_map_remove_pair,              "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8B EC 48 83 EC ? 0F B6 41 ? 48 8B F2",                                                                                                                 0x029EF0A0, 0x02972FE0),
  SIG_ENTRY_NORMAL(DIRECT,     generic_map_find,                     "48 89 5C 24 ? 48 89 74 24 ? 55 57 41 56 48 8D 6C 24 ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 45 ? 4D 8B F1",                                                                            0x041C7390, 0x03FDEB30),
  SIG_ENTRY_NORMAL(DIRECT,     add_tag_table_row,                    "48 8B C4 55 57 48 8D 68 ? 48 81 EC ? ? ? ? 48 89 58 ? 48 89 70 ? 4C 89 60 ? 4C 89 68 ? 4C 89 78",                                                                                                 0x03FC7410, 0x03DDDA60),
  SIG_ENTRY_NORMAL(DIRECT,     tmulticast_delegate_broadcast,        "40 56 48 83 EC ? FF 41 ? 48 8B F1 8B 49 ? 48 89 5C 24",                                                                                                                                           0x00E6BEA0, 0x00E1FD30),
  SIG_ENTRY_NORMAL(DIRECT,     request_gameplay_tag,                 "40 53 55 56 57 48 83 EC ? 8B 81",                                                                                                                                                                 0x03FC8DC0, 0x03DDF410),
};

static signature_t g_input_sigs[] = {
  SIG_ENTRY_HOOKED(DIRECT,   process_mouse_move_event,                "40 55 53 57 41 54 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 33 FF",                 0x02B54DD0, 0x02ADF210),
  SIG_ENTRY_HOOKED(DIRECT,   process_mouse_button_down_event,         "40 55 53 57 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? FF 81",                             0x02B51260, 0x02ADB6A0),
  SIG_ENTRY_HOOKED(CALL_ARG, process_mouse_button_up_event,           "E8 ? ? ? ? 48 8B 9C 24 ? ? ? ? 84 C0 4C 8B B4 24",                                                                                          0x02B59791, 0x02AE3BD1),
  SIG_ENTRY_HOOKED(DIRECT,   process_mouse_button_double_click_event, "40 55 53 56 57 41 55 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? ? ? ? 4D 8B F0",           0x02B53870, 0x02ADDCB0),
  SIG_ENTRY_HOOKED(DIRECT,   process_mouse_wheel_or_gesture_event,    "40 55 56 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 49 8B F8",                                                       0x02B543D0, 0x02ADE810),
  SIG_ENTRY_HOOKED(DIRECT,   process_key_char_event,                  "48 89 5C 24 ? 55 56 57 41 56 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? FF 81 ? ? ? ? 45 33 FF", 0x02B4F960, 0x02AD9DA0),
  SIG_ENTRY_HOOKED(DIRECT,   process_key_down_event,                  "4C 8B DC 55 53 57 41 56 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? 49 89 73 ? 48 8B F9",                  0x02B4FE70, 0x02ADA2B0),
  SIG_ENTRY_HOOKED(DIRECT,   process_key_up_event,                    "40 55 56 41 55 41 57 48 8D AC 24 ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? FF 81",                                0x02B505E0, 0x02ADAA20),
  SIG_ENTRY_HOOKED(DIRECT,   process_analog_input_event,              "4C 8B DC 55 53 57 41 57 49 8D AB ? ? ? ? 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 85 ? ? ? ? FF 81",                                0x02B50A80, 0x02ADAEC0),
  SIG_ENTRY_HOOKED(DIRECT,   process_app_activation_event,            "48 89 6C 24 ? 56 48 83 EC ? 80 3D ? ? ? ? 00 48 8D B1",                                                                                     0x02B57D70, 0x02AE21B0),
};

static struct {
  signature_t *sigs;
  int          count;
} g_sig_table[SIG_KIND_MAX] = {
  [SIG_KIND_EARLY] = {g_early_sigs, COUNTOF(g_early_sigs)},
  [SIG_KIND_MAIN]  = {g_main_sigs,  COUNTOF(g_main_sigs) },
  [SIG_KIND_INPUT] = {g_input_sigs, COUNTOF(g_input_sigs)},
};

static void
scan_signatures(signature_t *sigs, int count)
{
  for (int i = 0; i < count; ++i) {
    signature_t  *s  = &sigs[i];
    sigscan_err_t rc = sigscan_scan_entry(globals.user_spans, globals.num_user_spans, globals.user_module_base, &s->entry);
    if (rc != SIG_ERR_OK) {
      if (rc == SIG_ERR_INVALID_PATTERN) {
        LOG_ERROR("%s: %s: '%s'", s->entry.name, sig_err_to_str(rc), s->entry.pattern);
      } else {
        LOG_ERROR("%s: %s", s->entry.name, sig_err_to_str(rc));
      }
      continue;
    }
    
    if (s->patch) {
      s->patch(&s->entry);
    }

    s->resolved = true;
  }
}

static void
create_hooks(signature_t *sigs, int count)
{
  for (int i = 0; i < count; ++i) {
    signature_t *s = &sigs[i];
    if (s->resolved && s->detour) {
      if (hook_chain_create(&s->hook, s->entry.addr, s->detour, s->original)) {
        s->hook_created = true;
      } else {
        LOG_ERROR("%s: failed to create hook", s->entry.name);
      }
    }
  }
}

static void
enable_hooks(signature_t *sigs, int count)
{
  for (int i = 0; i < count; ++i) {
    signature_t *s = &sigs[i];
    if (s->hook_created && !s->hook_enabled) {
      if (hook_chain_enable(&s->hook)) {
        s->hook_enabled = true;
      } else {
        LOG_ERROR("%s: failed to enable hook", s->entry.name);
      }
    }
  }
}

void
signatures_init(void)
{
  void *user_module = (void *)GetModuleHandleA(NULL);
  if (!user_module) {
    LOG_ERROR("signatures: failed to get the main module base address");
    return;
  }

  globals.num_user_spans   = sigscan_build_exec_spans(globals.user_spans, COUNTOF(globals.user_spans), user_module);
  globals.user_module_base = user_module;
  if (globals.num_user_spans <= 0 || !sigscan_snapshot_exec_spans(globals.user_spans, globals.num_user_spans, &globals.perm)) {
    LOG_ERROR("signatures: failed to snapshot executable sections");
    return;
  }

  for (int i = 0; i < SIG_KIND_MAX; ++i) {
    scan_signatures(g_sig_table[i].sigs, g_sig_table[i].count);
  }

  for (int i = 0; i < SIG_KIND_MAX; ++i) {
    create_hooks(g_sig_table[i].sigs, g_sig_table[i].count);
  }

  enable_hooks(g_sig_table[SIG_KIND_EARLY].sigs, g_sig_table[SIG_KIND_EARLY].count);
  enable_hooks(g_sig_table[SIG_KIND_MAIN].sigs,  g_sig_table[SIG_KIND_MAIN].count);

  int max_resolved = 0;
  int max_hooked   = 0;
  int num_resolved = 0;
  int num_hooked   = 0;

  for (int i = 0; i < SIG_KIND_MAX; ++i) {
    signature_t *sigs = g_sig_table[i].sigs;
    for (int j = 0; j < g_sig_table[i].count; ++i) {
      signature_t *s = &sigs[j];

      max_resolved += 1;
      if (s->resolved) {
        num_resolved += 1;
      }

      if (s->detour && s->original) {
        max_hooked += 1;

        if (s->hook_created) {
          num_hooked += 1;
        }
      }
    }
  }

  if (num_resolved != max_resolved) {
    LOG_ERROR("signatures: failed to resolve all entries (%d/%d)", num_resolved, max_resolved);
  } else if (num_hooked != max_hooked) {
    LOG_ERROR("signatures: failed to hook all entries (%d/%d)", num_hooked, max_hooked);
  } else {
    LOG_INFO("signatures: entries resolved (%d/%d), entries hooked (%d/%d)", num_resolved, max_resolved, num_hooked, max_hooked);
  }
}

void
enable_input_hooks(void)
{
  enable_hooks(g_sig_table[SIG_KIND_INPUT].sigs, g_sig_table[SIG_KIND_INPUT].count);
}

