#include "sigscan.h"
#include "log.h"

#include <windows.h>

int
sigscan_build_exec_spans(sigscan_exec_span_t *spans, int max_spans, void *module_base)
{
  if (!spans || max_spans == 0 || !module_base) {
    return 0;
  }

  uint8_t *base = (uint8_t *)module_base;

  IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
  if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
    return -1;
  }

  IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
  if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) {
    return -1;
  }

  IMAGE_SECTION_HEADER *sec   = IMAGE_FIRST_SECTION(nt);
  WORD                  n_sec = nt->FileHeader.NumberOfSections;

  int n_spans = 0;
  for (WORD i = 0; i < n_sec && n_spans < max_spans; ++i) {
    sigscan_exec_span_t *s = &spans[n_spans];

    DWORD ch = sec[i].Characteristics;
    if (ch & IMAGE_SCN_MEM_EXECUTE) {
      uint64_t vsz = sec[i].Misc.VirtualSize;
      uint64_t rsz = sec[i].SizeOfRawData;

      mem_copy(s->name, sec[i].Name, IMAGE_SIZEOF_SHORT_NAME);

      s->base  = base + sec[i].VirtualAddress;
      s->size  = (vsz > rsz ? vsz : rsz);
      n_spans += 1;
    }
  }

  return n_spans;
}

static inline void *
rip_rel32(uint8_t *disp_at)
{
  int32_t d;
  mem_copy(&d, disp_at, sizeof(d));
  return disp_at + 4 + d;
}

static void *
resolve_match(sigscan_entry_t *e, uint8_t *match)
{
  switch (e->kind) {
  case SIG_DIRECT:
    return match;
    // e.g. "48 8B 05 ?? ?? ?? ??" mov rax,[rip+disp32], "E8 ?? ?? ?? ??" call rel32
  case SIG_RIPREL32_AT:
    return rip_rel32(match + e->op_off);
  }
  return NULL;
}

static bool
is_readable(const void *a)
{
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(a, &mbi, sizeof(mbi))) {
    return false;
  }

  if (!(mbi.State & MEM_COMMIT)) {
    return false;
  }

  if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))) {
    return false;
  }

  return true;
}

// IDA-style pattern parser (e.g. "48 8B ?? ?? 89 ?? 90")
static uint64_t
parse_signature(const char *pattern, uint8_t *out_sig, char *out_mask, uint64_t max_len)
{
  uint64_t    n = 0;
  const char *p = pattern;

  while (*p && n < max_len) {
    // skip whitespace
    while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r' || *p == '\n') {
      p += 1;
    }

    if (*p == '\0') {
      break;
    }

    if (p[0] == '?') {
      // support "?" or "??"
      out_sig[n]   = 0;
      out_mask[n]  = '?';
      n           += 1;

      p += (p[1] == '?') ? 2 : 1;
    } else {
      // expect two hex digits
      uint32_t v = 0;

      int hi = hex_to_nibble(p[0]);
      int lo = hex_to_nibble(p[1]);
      if (hi < 0 || lo < 0) {
        return 0; // bad token
      }

      v           = (uint32_t)((hi << 4) | lo);
      out_sig[n]  = (uint8_t)v;
      out_mask[n] = 'x';

      n += 1;
      p += 2;
    }

    while (*p == ' ' || *p == '\t' || *p == ',' || *p == '\r' || *p == '\n') {
      p += 1;
    }
  }

  if (n < max_len) {
    out_mask[n] = '\0';
  }
  return n;
}

static bool
signature_match(void *a, void *b, const char *mask, uint64_t len)
{
  uint8_t *pa = (uint8_t *)a;
  uint8_t *pb = (uint8_t *)b;

  for (uint64_t i = 0; i < len; ++i) {
    if (mask[i] != '?' && pa[i] != pb[i]) {
      return false;
    }
  }

  return true;
}

static void *
find_signature(void *haystack, uint64_t haystack_len, uint8_t *sig, const char *mask, uint64_t len)
{
  const uint8_t *base = (const uint8_t *)haystack;
  if (!haystack || !sig || !mask || len == 0 || haystack_len < len) {
    return NULL;
  }

  for (size_t i = 0; i + len <= haystack_len; ++i) {
    const uint8_t *p = base + i;

    if (signature_match((void *)p, sig, mask, len)) {
      return (void *)p;
    }
  }

  return NULL;
}

static sigscan_err_t
scan_at_rva(sigscan_entry_t *e, void *module_base, uintptr_t rva, uint8_t *sig, const char *mask, uint64_t len)
{
  uint8_t *mod_base  = (uint8_t *)module_base;
  uint8_t *candidate = mod_base + rva;

  if (!signature_match(candidate, sig, mask, len)) {
    return SIG_ERR_NOT_FOUND;
  }

  uint8_t *target = (uint8_t *)resolve_match(e, candidate);
  if (!target) {
    return SIG_ERR_NOT_FOUND;
  }

  for (uint8_t i = 0; i < e->deref; ++i) {
    if (!is_readable(target)) {
      return SIG_ERR_TARGET_NOT_READABLE;
    }
    target = *(uint8_t **)target; // follow pointer(s)
  }

  *e->pp  = target;
  e->addr = target;
  return SIG_ERR_OK;
}

sigscan_err_t
sigscan_scan_entry(sigscan_exec_span_t *spans, int num_spans, void *module_base, sigscan_entry_t *entry)
{
  if (!spans || num_spans == 0 || !module_base || !entry) {
    return SIG_ERR_NOT_FOUND;
  }

  uint8_t sig[1024];
  char    mask[1024];

  uint64_t len = parse_signature(entry->pattern, sig, mask, sizeof(sig));
  if (len == 0) {
    return SIG_ERR_INVALID_PATTERN;
  }

  for (int i = 0; i < COUNTOF(entry->first_rvas); ++i) {
    uintptr_t rva = entry->first_rvas[i];

    if (rva != 0) {
      if (scan_at_rva(entry, module_base, rva, sig, mask, len) == SIG_ERR_OK) {
        // found immediately
        return SIG_ERR_OK;
      }
    }
  }

  for (int s = 0; s < num_spans; ++s) {
    sigscan_exec_span_t *span = &spans[s];

    uint8_t *match = (uint8_t *)find_signature(span->base, span->size, sig, mask, len);
    if (!match) {
      continue;
    }

    uintptr_t rva = (uintptr_t)match - (uintptr_t)module_base;
    if (entry->name && entry->name[0] != '\0') {
      LOG_DEBUG("%s: found match: 0x%08llX", entry->name, rva);
    } else {
      LOG_DEBUG("found match: 0x%08llX", rva);
    }

    uint8_t *target = (uint8_t *)resolve_match(entry, match);
    if (!target) {
      continue;
    }

    for (uint8_t i = 0; i < entry->deref; ++i) {
      if (!is_readable(target)) {
        return SIG_ERR_TARGET_NOT_READABLE;
      }

      target = *(uint8_t **)target; // follow pointer(s)
    }

    if (entry->pp) {
      *entry->pp = target;
    }
    entry->addr = target;
    return SIG_ERR_OK;
  }

  return SIG_ERR_NOT_FOUND;
}
