#include "str.h"

#define BITMASK1  0x01
#define BITMASK2  0x03
#define BITMASK3  0x07
#define BITMASK4  0x0F
#define BITMASK5  0x1F
#define BITMASK6  0x3F
#define BITMASK7  0x7F
#define BITMASK8  0xFF
#define BITMASK9  0x01FF
#define BITMASK10 0x03FF

typedef struct decoded_codepoint_s decoded_codepoint_t;
struct decoded_codepoint_s {
  uint32_t codepoint;
  uint32_t advance;
};

static uint8_t utf8_class[32] = {
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 5,
};

static decoded_codepoint_t
codepoint_from_utf8(uint8_t *str, uint64_t max)
{
  decoded_codepoint_t result = {
    .codepoint = 0xFFFFFFFF,
    .advance   = 1,
  };

  uint8_t byte       = str[0];
  uint8_t byte_class = utf8_class[byte >> 3];

  switch (byte_class) {
    case 1:
      result.codepoint = byte;
      break;

    case 2:
      if (2 <= max) {
        uint8_t cont_byte = str[1];
        if (utf8_class[cont_byte >> 3] == 0) {
          result.codepoint  = (byte & BITMASK5) << 6;
          result.codepoint |= (cont_byte & BITMASK6);
          result.advance    = 2;
        }
      }
      break;

    case 3:
      if (3 <= max) {
        uint8_t cont_byte[2] = {str[1], str[2]};
        if (utf8_class[cont_byte[0] >> 3] == 0 && utf8_class[cont_byte[1] >> 3] == 0) {
          result.codepoint  = (byte & BITMASK4) << 12;
          result.codepoint |= ((cont_byte[0] & BITMASK6) << 6);
          result.codepoint |= (cont_byte[1] & BITMASK6);
          result.advance    = 3;
        }
      }
      break;

    case 4:
      if (4 <= max) {
        uint8_t cont_byte[3] = {str[1], str[2], str[3]};
        if (utf8_class[cont_byte[0] >> 3] == 0 && utf8_class[cont_byte[1] >> 3] == 0 &&
            utf8_class[cont_byte[2] >> 3] == 0) {
          result.codepoint  = (byte & BITMASK3) << 18;
          result.codepoint |= ((cont_byte[0] & BITMASK6) << 12);
          result.codepoint |= ((cont_byte[1] & BITMASK6) << 6);
          result.codepoint |= (cont_byte[2] & BITMASK6);
          result.advance    = 4;
        }
      }
      break;
  }

  if (result.advance == 2 && result.codepoint < 0x80) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.advance == 3 && result.codepoint < 0x800) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.advance == 4 && (result.codepoint < 0x10000 || result.codepoint > 0x10FFFF)) {
    result.codepoint = 0xFFFFFFFF;
  }

  if (result.codepoint >= 0xD800 && result.codepoint <= 0xDFFF) {
    result.codepoint = 0xFFFFFFFF;
  }

  return result;
}

static decoded_codepoint_t
codepoint_from_utf16(uint16_t *str, uint64_t max)
{
  decoded_codepoint_t result = {
    .codepoint = 0xFFFFFFFF,
    .advance   = 1,
  };

  if (!max) {
    return result;
  }

  uint16_t w0 = str[0];
  if (w0 >= 0xD800 && w0 <= 0xDBFF) {
    if (max >= 2) {
      uint16_t w1 = str[1];
      if (w1 >= 0xDC00 && w1 <= 0xDFFF) {
        result.codepoint = 0x10000 + (((uint32_t)(w0 - 0xD800) << 10) | ((uint32_t)(w1 - 0xDC00) & 0x3FF));
        result.advance   = 2;
        return result;
      }
    }
    return result;
  }

  if (w0 >= 0xDC00 && w0 <= 0xDFFF) {
    return result;
  }

  result.codepoint = w0;
  return result;
}

static uint64_t
utf8_len_from_codepoint(uint32_t codepoint)
{
#define BIT8 0x80
  uint32_t advance = 0;
  if (codepoint <= 0x7F) {
    advance = 1;
  } else if (codepoint <= 0x7FF) {
    advance = 2;
  } else if (codepoint <= 0xFFFF) {
    advance = 3;
  } else if (codepoint <= 0x10FFFF) {
    advance = 4;
  } else {
    advance = 1;
  }
#undef BIT8
  return advance;
}

static uint32_t
utf8_from_codepoint(uint8_t *out, uint32_t codepoint)
{
#define BIT8 0x80
  uint32_t advance = 0;
  if (codepoint <= 0x7F) {
    out[0]  = (uint8_t)codepoint;
    advance = 1;
  } else if (codepoint <= 0x7FF) {
    out[0]  = (BITMASK2 << 6) | ((codepoint >> 6) & BITMASK5);
    out[1]  = BIT8 | (codepoint & BITMASK6);
    advance = 2;
  } else if (codepoint <= 0xFFFF) {
    out[0]  = (BITMASK3 << 5) | ((codepoint >> 12) & BITMASK4);
    out[1]  = BIT8 | ((codepoint >> 6) & BITMASK6);
    out[2]  = BIT8 | (codepoint & BITMASK6);
    advance = 3;
  } else if (codepoint <= 0x10FFFF) {
    out[0]  = (BITMASK4 << 4) | ((codepoint >> 18) & BITMASK3);
    out[1]  = BIT8 | ((codepoint >> 12) & BITMASK6);
    out[2]  = BIT8 | ((codepoint >> 6) & BITMASK6);
    out[3]  = BIT8 | (codepoint & BITMASK6);
    advance = 4;
  } else {
    out[0]  = '?';
    advance = 1;
  }
#undef BIT8
  return advance;
}

static uint32_t
utf16_from_codepoint(uint16_t *out, uint32_t codepoint)
{
  uint32_t advance = 1;
  if (codepoint == 0xFFFFFFFF) {
    out[0] = (uint16_t)'?';
  } else if (codepoint < 0x10000) {
    out[0] = (uint16_t)codepoint;
  } else {
    uint64_t v = codepoint - 0x10000;
    out[0]  = 0xD800 + (uint16_t)(v >> 10);
    out[1]  = 0xDC00 + (uint16_t)(v & BITMASK10);
    advance = 2;
  }
  return advance;
}

str_t
str_from_str16(arena_t *arena, str16_t in)
{
  uint64_t  cap  = in.len * 3; // worst case UTF-8
  uint8_t  *buf  = ARENA_PUSH_ARRAY(arena, uint8_t, cap+1);
  uint16_t *ptr  = in.data;
  uint16_t *opl  = ptr + in.len;
  uint64_t  size = 0;

  if (buf) {
    while (ptr < opl) {
      decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));

      ptr  += c.advance;
      size += utf8_from_codepoint(buf + size, c.codepoint);
    }
    buf[size] = 0;
    arena_pop(arena, cap - size);
  }

  return str_make(buf, size);
}

str16_t
str16_from_str(arena_t *arena, str_t in)
{
  uint64_t  cap  = in.len * 2; // worst case UTF-16
  uint16_t *buf  = ARENA_PUSH_ARRAY(arena, uint16_t, cap + 1);
  uint8_t  *ptr  = in.data;
  uint8_t  *opl  = ptr + in.len;
  uint64_t  size = 0;

  if (buf) {
    while (ptr < opl) {
      decoded_codepoint_t c = codepoint_from_utf8(ptr, (uint64_t)(opl - ptr));

      ptr  += c.advance;
      size += utf16_from_codepoint(buf + size, c.codepoint);
    }
    buf[size] = 0;
    arena_pop(arena, (cap - size) * sizeof(uint16_t));
  }

  return str16_make(buf, size);
}

uint64_t
str16_utf8_len(str16_t in)
{
  uint16_t *ptr = in.data;
  uint16_t *opl = ptr + in.len;
  uint64_t  len = 0;

  while (ptr < opl) {
    decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));

    ptr += c.advance;
    len += utf8_len_from_codepoint(c.codepoint);
  }

  return len;
}

uint64_t
str16_write_utf8(uint8_t *buf, uint64_t max_len, str16_t in)
{
  uint16_t *ptr = in.data;
  uint16_t *opl = ptr + in.len;
  uint64_t  len = 0;

  while (ptr < opl) {
    decoded_codepoint_t c = codepoint_from_utf16(ptr, (uint64_t)(opl - ptr));
    uint64_t bytes = utf8_len_from_codepoint(c.codepoint);

    if (len + bytes > max_len) {
      break;
    }

    ptr += c.advance;
    len += utf8_from_codepoint(buf + len, c.codepoint);
  }

  return len;
}
