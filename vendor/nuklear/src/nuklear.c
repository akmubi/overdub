#include "nuklear/include/nuklear.h"

#ifndef NK_PROFILE_SCOPE_BEGIN
#  define NK_PROFILE_SCOPE_BEGIN(NAME, SCOPE)
#endif

#ifndef NK_PROFILE_SCOPE_END
#  define NK_PROFILE_SCOPE_END(SCOPE)
#endif

/* ===============================================================
 *
 *                              MATH
 *
 * ===============================================================*/
/*/// ### Math
///  Since nuklear is supposed to work on all systems providing floating point
///  math without any dependencies I also had to implement my own math functions
///  for sqrt, sin and cos. Since the actual highly accurate implementations for
///  the standard library functions are quite complex and I do not need high
///  precision for my use cases I use approximations.
///
///  Sqrt
///  ----
///  For square root nuklear uses the famous fast inverse square root:
///  https://en.wikipedia.org/wiki/Fast_inverse_square_root with
///  slightly tweaked magic constant. While on today's hardware it is
///  probably not faster it is still fast and accurate enough for
///  nuklear's use cases. IMPORTANT: this requires float format IEEE 754
///
///  Sine/Cosine
///  -----------
///  All constants inside both function are generated Remez's minimax
///  approximations for value range 0...2*PI. The reason why I decided to
///  approximate exactly that range is that nuklear only needs sine and
///  cosine to generate circles which only requires that exact range.
///  In addition I used Remez instead of Taylor for additional precision:
///  www.lolengine.net/blog/2011/12/21/better-function-approximations.
///
///  The tool I used to generate constants for both sine and cosine
///  (it can actually approximate a lot more functions) can be
///  found here: www.lolengine.net/wiki/oss/lolremez
*/
#ifdef NK_INV_SQRT_NEEDED
NK_LIB float
nk_inv_sqrt(float n)
{
  float       x2;
  const float threehalfs = 1.5f;

  union {
    nk_uint i;
    float   f;
  } conv = {0};
  conv.f = n;
  x2     = n * 0.5f;
  conv.i = 0x5f375A84 - (conv.i >> 1);
  conv.f = conv.f * (threehalfs - (x2 * conv.f * conv.f));
  return conv.f;
}
#endif

#ifdef NK_SIN_NEEDED
NK_LIB float
nk_sin(float x)
{
  NK_STORAGE const float a0 = +1.91059300966915117e-31f;
  NK_STORAGE const float a1 = +1.00086760103908896f;
  NK_STORAGE const float a2 = -1.21276126894734565e-2f;
  NK_STORAGE const float a3 = -1.38078780785773762e-1f;
  NK_STORAGE const float a4 = -2.67353392911981221e-2f;
  NK_STORAGE const float a5 = +2.08026600266304389e-2f;
  NK_STORAGE const float a6 = -3.03996055049204407e-3f;
  NK_STORAGE const float a7 = +1.38235642404333740e-4f;
  return a0 + x * (a1 + x * (a2 + x * (a3 + x * (a4 + x * (a5 + x * (a6 + x * a7))))));
}
#endif

#ifdef NK_COS_NEEDED
NK_LIB float
nk_cos(float x)
{
  /* New implementation. Also generated using lolremez. */
  /* Old version significantly deviated from expected results. */
  NK_STORAGE const float a0 = 9.9995999154986614e-1f;
  NK_STORAGE const float a1 = 1.2548995793001028e-3f;
  NK_STORAGE const float a2 = -5.0648546280678015e-1f;
  NK_STORAGE const float a3 = 1.2942246466519995e-2f;
  NK_STORAGE const float a4 = 2.8668384702547972e-2f;
  NK_STORAGE const float a5 = 7.3726485210586547e-3f;
  NK_STORAGE const float a6 = -3.8510875386947414e-3f;
  NK_STORAGE const float a7 = 4.7196604604366623e-4f;
  NK_STORAGE const float a8 = -1.8776444013090451e-5f;
  return a0 + x * (a1 + x * (a2 + x * (a3 + x * (a4 + x * (a5 + x * (a6 + x * (a7 + x * a8)))))));
}
#endif

#ifdef NK_ATAN_NEEDED
NK_LIB float
nk_atan(float x)
{
  /* ./lolremez --progress --float -d 9 -r "0:pi*2" "atan(x)" */
  float u = -1.0989005e-05f;

  NK_ASSERT(x >= 0.0f && "TODO support negative floats");
  u = u * x + 0.00034117949f;
  u = u * x + -0.0044932296f;
  u = u * x + 0.032596264f;
  u = u * x + -0.14088021f;
  u = u * x + 0.36040401f;
  u = u * x + -0.47017866f;
  u = u * x + 0.00050198776f;
  u = u * x + 1.0077682f;
  u = u * x + -0.0004765437f;
  return u;
}
#endif

#ifdef NK_ATAN2_NEEDED
NK_LIB float
nk_atan2(float y, float x)
{
  float   ax = NK_ABS(x), ay = NK_ABS(y);
  /* 0 = +y +x    1 = -y +x
     2 = +y -x    3 = -y -x */
  nk_uint signs = (y < 0) | ((x < 0) << 1);

  float a;

  if (y == 0.0 && x == 0.0) {
    return 0.0f;
  }

  a = (ay > ax) ? NK_PI_HALF - NK_ATAN(ax / ay) : NK_ATAN(ay / ax);

  switch (signs) {
    case 0:
      return a;

    case 1:
      return -a;

    case 2:
      return -a + NK_PI;

    case 3:
      return a - NK_PI;
  }
  return 0.0f; /* prevents warning */
}
#endif

NK_LIB nk_uint
nk_round_up_pow2(nk_uint v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}

#ifdef NK_DTOA_NEEDED
NK_LIB double
nk_pow(double x, int n)
{
  /*  check the sign of n */
  double r    = 1;
  int    plus = n >= 0;

  n = (plus) ? n : -n;

  while (n > 0) {
    if ((n & 1) == 1) {
      r *= x;
    }

    n /= 2;
    x *= x;
  }
  return plus ? r : 1.0 / r;
}
#endif

#ifdef NK_DTOA_NEEDED
NK_LIB int
nk_ifloord(double x)
{
  x = (double)((int)x - ((x < 0.0) ? 1 : 0));
  return (int)x;
}
#endif

NK_LIB int
nk_ifloorf(float x)
{
  x = (float)((int)x - ((x < 0.0f) ? 1 : 0));
  return (int)x;
}

NK_LIB int
nk_iceilf(float x)
{
  if (x >= 0) {
    int i = (int)x;
    return (x > i) ? i + 1 : i;
  } else {
    int   t = (int)x;
    float r = x - (float)t;
    return (r > 0.0f) ? t + 1 : t;
  }
}

#ifdef NK_DTOA_NEEDED
NK_LIB int
nk_log10(double n)
{
  int neg;
  int ret;
  int exp = 0;

  neg = (n < 0) ? 1 : 0;
  ret = (neg) ? (int)-n : (int)n;

  while ((ret / 10) > 0) {
    ret /= 10;
    exp++;
  }

  if (neg) {
    exp = -exp;
  }
  return exp;
}
#endif

NK_LIB float
nk_roundf(float x)
{
  return (x >= 0.0f) ? (float)nk_ifloorf(x + 0.5f) : (float)nk_iceilf(x - 0.5f);
}

NK_API struct nk_rect
nk_get_null_rect(void)
{
  return nk_null_rect;
}

NK_API struct nk_rect
nk_rect(float x, float y, float w, float h)
{
  struct nk_rect r;

  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  return r;
}

NK_API struct nk_rect
nk_recti(int x, int y, int w, int h)
{
  struct nk_rect r;

  r.x = (float)x;
  r.y = (float)y;
  r.w = (float)w;
  r.h = (float)h;
  return r;
}

NK_API struct nk_rect
nk_recta(struct nk_vec2 pos, struct nk_vec2 size)
{
  return nk_rect(pos.x, pos.y, size.x, size.y);
}

NK_API struct nk_rect
nk_rectv(const float *r)
{
  return nk_rect(r[0], r[1], r[2], r[3]);
}

NK_API struct nk_rect
nk_rectiv(const int *r)
{
  return nk_recti(r[0], r[1], r[2], r[3]);
}

NK_API struct nk_vec2
nk_rect_pos(struct nk_rect r)
{
  struct nk_vec2 ret;

  ret.x = r.x;
  ret.y = r.y;
  return ret;
}

NK_API struct nk_vec2
nk_rect_size(struct nk_rect r)
{
  struct nk_vec2 ret;

  ret.x = r.w;
  ret.y = r.h;
  return ret;
}

NK_LIB struct nk_rect
nk_shrink_rect(struct nk_rect r, float amount)
{
  struct nk_rect res;

  r.w   = NK_MAX(r.w, 2 * amount);
  r.h   = NK_MAX(r.h, 2 * amount);
  res.x = r.x + amount;
  res.y = r.y + amount;
  res.w = r.w - 2 * amount;
  res.h = r.h - 2 * amount;
  return res;
}

NK_LIB struct nk_rect
nk_pad_rect(struct nk_rect r, struct nk_vec2 pad)
{
  r.w  = NK_MAX(r.w, 2 * pad.x);
  r.h  = NK_MAX(r.h, 2 * pad.y);
  r.x += pad.x;
  r.y += pad.y;
  r.w -= 2 * pad.x;
  r.h -= 2 * pad.y;
  return r;
}

NK_API struct nk_vec2
nk_vec2(float x, float y)
{
  struct nk_vec2 ret;

  ret.x = x;
  ret.y = y;
  return ret;
}

NK_API struct nk_vec2
nk_vec2i(int x, int y)
{
  struct nk_vec2 ret;

  ret.x = (float)x;
  ret.y = (float)y;
  return ret;
}

NK_API struct nk_vec2
nk_vec2v(const float *v)
{
  return nk_vec2(v[0], v[1]);
}

NK_API struct nk_vec2
nk_vec2iv(const int *v)
{
  return nk_vec2i(v[0], v[1]);
}

NK_LIB void
nk_unify(struct nk_rect *clip, const struct nk_rect *a, float x0, float y0, float x1, float y1)
{
  NK_ASSERT(a != NULL);
  NK_ASSERT(clip != NULL);
  clip->x = NK_MAX(a->x, x0);
  clip->y = NK_MAX(a->y, y0);
  clip->w = NK_MIN(a->x + a->w, x1) - clip->x;
  clip->h = NK_MIN(a->y + a->h, y1) - clip->y;
  clip->w = NK_MAX(0, clip->w);
  clip->h = NK_MAX(0, clip->h);
}

NK_API void
nk_triangle_from_direction(struct nk_vec2 *result, struct nk_rect r, float pad_x, float pad_y, enum nk_heading direction)
{
  float w_half, h_half;

  NK_ASSERT(result != NULL);

  r.w = NK_MAX(2 * pad_x, r.w);
  r.h = NK_MAX(2 * pad_y, r.h);
  r.w = r.w - 2 * pad_x;
  r.h = r.h - 2 * pad_y;

  r.x = r.x + pad_x;
  r.y = r.y + pad_y;

  w_half = r.w / 2.0f;
  h_half = r.h / 2.0f;

  if (direction == NK_UP) {
    result[0] = nk_vec2(r.x + w_half, r.y);
    result[1] = nk_vec2(r.x + r.w, r.y + r.h);
    result[2] = nk_vec2(r.x, r.y + r.h);
  } else if (direction == NK_RIGHT) {
    result[0] = nk_vec2(r.x, r.y);
    result[1] = nk_vec2(r.x + r.w, r.y + h_half);
    result[2] = nk_vec2(r.x, r.y + r.h);
  } else if (direction == NK_DOWN) {
    result[0] = nk_vec2(r.x, r.y);
    result[1] = nk_vec2(r.x + r.w, r.y);
    result[2] = nk_vec2(r.x + w_half, r.y + r.h);
  } else {
    result[0] = nk_vec2(r.x, r.y + h_half);
    result[1] = nk_vec2(r.x + r.w, r.y);
    result[2] = nk_vec2(r.x + r.w, r.y + r.h);
  }
}

/* ===============================================================
 *
 *                              UTIL
 *
 * ===============================================================*/

#ifdef NK_MEMMOVE_NEEDED
NK_LIB void *
nk_memmove(void *dst0, const void *src0, nk_size length)
{
  nk_byte       *dst = (nk_byte *)dst0;
  const nk_byte *src = (const nk_byte *)src0;

  if (dst < src) {
    while (length--) {
      *dst++ = *src++;
    }
  } else if (dst > src) {
    while (length--) {
      dst[length] = src[length];
    }
  }
  return dst0;
}
#endif

#ifdef NK_MEMCPY_NEEDED
NK_LIB void *
nk_memcopy(void *dst0, const void *src0, nk_size length)
{
  nk_ptr      t;
  char       *dst = (char *)dst0;
  const char *src = (const char *)src0;

  if (length == 0 || dst == src) {
    goto done;
  }

#  define nk_word  int
#  define nk_wsize sizeof(nk_word)
#  define nk_wmask (nk_wsize - 1)
#  define NK_TLOOP(s) \
    if (t)            \
    NK_TLOOP1(s)
#  define NK_TLOOP1(s) \
    do {               \
      s;               \
    } while (--t)

  if (dst < src) {
    t = (nk_ptr)src; /* only need low bits */

    if ((t | (nk_ptr)dst) & nk_wmask) {
      if ((t ^ (nk_ptr)dst) & nk_wmask || length < nk_wsize) {
        t = length;
      } else {
        t = nk_wsize - (t & nk_wmask);
      }

      length -= t;
      NK_TLOOP1(*dst++ = *src++);
    }

    t = length / nk_wsize;
    NK_TLOOP(*(nk_word *)(void *)dst = *(const nk_word *)(const void *)src; src += nk_wsize; dst += nk_wsize);
    t = length & nk_wmask;
    NK_TLOOP(*dst++ = *src++);
  } else {
    src += length;
    dst += length;
    t    = (nk_ptr)src;

    if ((t | (nk_ptr)dst) & nk_wmask) {
      if ((t ^ (nk_ptr)dst) & nk_wmask || length <= nk_wsize) {
        t = length;
      } else {
        t &= nk_wmask;
      }

      length -= t;
      NK_TLOOP1(*--dst = *--src);
    }

    t = length / nk_wsize;
    NK_TLOOP(src -= nk_wsize; dst -= nk_wsize; *(nk_word *)(void *)dst = *(const nk_word *)(const void *)src);
    t = length & nk_wmask;
    NK_TLOOP(*--dst = *--src);
  }
#  undef nk_word
#  undef nk_wsize
#  undef nk_wmask
#  undef NK_TLOOP
#  undef NK_TLOOP1
done:
  return (dst0);
}
#endif

#ifdef NK_MEMSET_NEEDED
NK_LIB void
nk_memset(void *ptr, int c0, nk_size size)
{
#  define nk_word  unsigned
#  define nk_wsize sizeof(nk_word)
#  define nk_wmask (nk_wsize - 1)
  nk_byte *dst = (nk_byte *)ptr;
  unsigned c   = 0;
  nk_size  t   = 0;

  if ((c = (nk_byte)c0) != 0) {
    c = (c << 8) | c; /* at least 16-bits  */

    if (sizeof(unsigned int) > 2) {
      c = (c << 16) | c; /* at least 32-bits*/
    }
  }

  /* too small of a word count */
  dst = (nk_byte *)ptr;

  if (size < 3 * nk_wsize) {
    while (size--) {
      *dst++ = (nk_byte)c0;
    }
    return;
  }

  /* align destination */
  if ((t = NK_PTR_TO_UINT(dst) & nk_wmask) != 0) {
    t     = nk_wsize - t;
    size -= t;

    do {
      *dst++ = (nk_byte)c0;
    } while (--t != 0);
  }

  /* fill word */
  t = size / nk_wsize;

  do {
    *(nk_word *)((void *)dst)  = c;
    dst += nk_wsize;
  } while (--t != 0);

  /* fill trailing bytes */
  t = (size & nk_wmask);

  if (t != 0) {
    do {
      *dst++ = (nk_byte)c0;
    } while (--t != 0);
  }

#  undef nk_word
#  undef nk_wsize
#  undef nk_wmask
}
#endif

NK_LIB void
nk_zero(void *ptr, nk_size size)
{
  NK_ASSERT(ptr);
  NK_MEMSET(ptr, 0, size);
}

NK_API int
nk_strlen(const char *str)
{
  int siz = 0;

  NK_ASSERT(str);

  while (str && *str++ != '\0') {
    siz++;
  }
  return siz;
}

NK_API int
nk_strtoi(const char *str, char **endptr)
{
  int         neg   = 1;
  const char *p     = str;
  int         value = 0;

  NK_ASSERT(str);

  if (!str) {
    return 0;
  }

  /* skip whitespace */
  while (*p == ' ') {
    p++;
  }

  if (*p == '-') {
    neg = -1;
    p++;
  }

  while (*p && *p >= '0' && *p <= '9') {
    value = value * 10 + (int)(*p - '0');
    p++;
  }

  if (endptr) {
    *endptr = (char *)p;
  }
  return neg * value;
}

#ifdef NK_STRTOD_NEEDED
NK_API double
nk_strtod(const char *str, char **endptr)
{
  double m;
  double neg    = 1.0;
  char  *p      = (char *)str;
  double value  = 0;
  double number = 0;

  NK_ASSERT(str);

  if (!str) {
    return 0;
  }

  /* skip whitespace */
  while (*p == ' ') {
    p++;
  }

  if (*p == '-') {
    neg = -1.0;
    p++;
  }

  while (*p && *p != '.' && *p != 'e') {
    value = value * 10.0 + (double)(*p - '0');
    p++;
  }

  if (*p == '.') {
    p++;

    for (m = 0.1; *p && *p != 'e'; p++) {
      value = value + (double)(*p - '0') * m;
      m    *= 0.1;
    }
  }

  if (*p == 'e') {
    int i, pow, div;

    p++;

    if (*p == '-') {
      div = nk_true;
      p++;
    } else if (*p == '+') {
      div = nk_false;
      p++;
    } else {
      div = nk_false;
    }

    for (pow = 0; *p; p++) {
      pow = pow * 10 + (int)(*p - '0');
    }

    for (m = 1.0, i = 0; i < pow; i++) {
      m *= 10.0;
    }

    if (div) {
      value /= m;
    } else {
      value *= m;
    }
  }

  number = value * neg;

  if (endptr) {
    *endptr = (char *)p;
  }
  return number;
}
#endif

NK_API float
nk_strtof(const char *str, char **endptr)
{
  float  float_value;
  double double_value;

  double_value = NK_STRTOD(str, endptr);
  float_value  = (float)double_value;
  return float_value;
}

NK_API int
nk_stricmp(const char *s1, const char *s2)
{
  nk_int c1, c2, d;

  do {
    c1 = *s1++;
    c2 = *s2++;
    d  = c1 - c2;

    while (d) {
      if (c1 <= 'Z' && c1 >= 'A') {
        d += ('a' - 'A');

        if (!d) {
          break;
        }
      }

      if (c2 <= 'Z' && c2 >= 'A') {
        d -= ('a' - 'A');

        if (!d) {
          break;
        }
      }
      return ((d >= 0) << 1) - 1;
    }
  } while (c1);
  return 0;
}

NK_API int
nk_stricmpn(const char *s1, const char *s2, int n)
{
  int c1, c2, d;

  NK_ASSERT(n >= 0);

  do {
    c1 = *s1++;
    c2 = *s2++;

    if (!n--) {
      return 0;
    }

    d = c1 - c2;

    while (d) {
      if (c1 <= 'Z' && c1 >= 'A') {
        d += ('a' - 'A');

        if (!d) {
          break;
        }
      }

      if (c2 <= 'Z' && c2 >= 'A') {
        d -= ('a' - 'A');

        if (!d) {
          break;
        }
      }
      return ((d >= 0) << 1) - 1;
    }
  } while (c1);
  return 0;
}

NK_LIB int
nk_string_float_limit(char *string, int prec)
{
  int   dot = 0;
  char *c   = string;

  while (*c) {
    if (*c == '.') {
      dot = 1;
      c++;
      continue;
    }

    if (dot == (prec + 1)) {
      *c = 0;
      break;
    }

    if (dot > 0) {
      dot++;
    }

    c++;
  }
  return (int)(c - string);
}

NK_INTERN void
nk_strrev_ascii(char *s)
{
  int  len = nk_strlen(s);
  int  end = len / 2;
  int  i   = 0;
  char t;

  for (; i < end; ++i) {
    t              = s[i];
    s[i]           = s[len - 1 - i];
    s[len - 1 - i] = t;
  }
}

NK_LIB char *
nk_itoa(char *s, long n)
{
  long i = 0;

  if (n == 0) {
    s[i++] = '0';
    s[i] = 0;
    return s;
  }

  if (n < 0) {
    s[i++] = '-';
    n = -n;
  }

  while (n > 0) {
    s[i++]  = (char)('0' + (n % 10));
    n /= 10;
  }

  s[i] = 0;

  if (s[0] == '-') {
    ++s;
  }

  nk_strrev_ascii(s);
  return s;
}

#ifdef NK_DTOA_NEEDED
NK_LIB char *
nk_dtoa(char *s, double n)
{
  int   useExp = 0;
  int   digit  = 0, m = 0, m1 = 0;
  char *c      = s;
  int   neg    = 0;

  NK_ASSERT(s);

  if (!s) {
    return 0;
  }

  if (n == 0.0) {
    s[0] = '0';
    s[1] = '\0';
    return s;
  }

  neg = (n < 0);

  if (neg) {
    n = -n;
  }

  /* calculate magnitude */
  m      = nk_log10(n);
  useExp = (m >= 14 || (neg && m >= 9) || m <= -9);

  if (neg) {
    *(c++) = '-';
  }

  /* set up for scientific notation */
  if (useExp) {
    if (m < 0) {
      m -= 1;
    }

    n  = n / (double)nk_pow(10.0, m);
    m1 = m;
    m  = 0;
  }

  if (m < 1.0) {
    m = 0;
  }

  /* convert the number */
  while (n > NK_FLOAT_PRECISION || m >= 0) {
    double weight = nk_pow(10.0, m);

    if (weight > 0) {
      double t = (double)n / weight;

      digit = nk_ifloord(t);
      n    -= ((double)digit * weight);
      *(c++)    = (char)('0' + (char)digit);
    }

    if (m == 0 && n > 0) {
      *(c++) = '.';
    }

    m--;
  }

  if (useExp) {
    /* convert the exponent */
    int i, j;

    *(c++) = 'e';

    if (m1 > 0) {
      *(c++) = '+';
    } else {
      *(c++) = '-';
      m1 = -m1;
    }

    m = 0;

    while (m1 > 0) {
      *(c++)  = (char)('0' + (char)(m1 % 10));
      m1 /= 10;
      m++;
    }

    c -= m;

    for (i = 0, j = m - 1; i < j; i++, j--) {
      /* swap without temporary */
      c[i] ^= c[j];
      c[j] ^= c[i];
      c[i] ^= c[j];
    }

    c += m;
  }

  *(c) = '\0';
  return s;
}
#endif

#ifdef NK_INCLUDE_STANDARD_VARARGS
#  ifndef NK_INCLUDE_STANDARD_IO
NK_INTERN int
nk_vsnprintf(char *buf, int buf_size, const char *fmt, va_list args)
{
  enum nk_arg_type { NK_ARG_TYPE_CHAR, NK_ARG_TYPE_SHORT, NK_ARG_TYPE_DEFAULT, NK_ARG_TYPE_LONG };
  enum nk_arg_flags { NK_ARG_FLAG_LEFT = 0x01, NK_ARG_FLAG_PLUS = 0x02, NK_ARG_FLAG_SPACE = 0x04, NK_ARG_FLAG_NUM = 0x10, NK_ARG_FLAG_ZERO = 0x20 };

  char             number_buffer[NK_MAX_NUMBER_BUFFER];
  enum nk_arg_type arg_type  = NK_ARG_TYPE_DEFAULT;
  int              precision = NK_DEFAULT;
  int              width     = NK_DEFAULT;
  nk_flags         flag      = 0;

  int         len    = 0;
  int         result = -1;
  const char *iter   = fmt;

  NK_ASSERT(buf);
  NK_ASSERT(buf_size);

  if (!buf || !buf_size || !fmt) {
    return 0;
  }

  for (iter = fmt; *iter && len < buf_size; iter++) {
    /* copy all non-format characters */
    while (*iter && (*iter != '%') && (len < buf_size)) {
      buf[len++] = *iter++;
    }

    if (!(*iter) || len >= buf_size) {
      break;
    }

    iter++;

    /* flag arguments */
    while (*iter) {
      if (*iter == '-') {
        flag |= NK_ARG_FLAG_LEFT;
      } else if (*iter == '+') {
        flag |= NK_ARG_FLAG_PLUS;
      } else if (*iter == ' ') {
        flag |= NK_ARG_FLAG_SPACE;
      } else if (*iter == '#') {
        flag |= NK_ARG_FLAG_NUM;
      } else if (*iter == '0') {
        flag |= NK_ARG_FLAG_ZERO;
      } else {
        break;
      }

      iter++;
    }

    /* width argument */
    width = NK_DEFAULT;

    if (*iter >= '1' && *iter <= '9') {
      char *end;

      width = nk_strtoi(iter, &end);

      if (end == iter) {
        width = -1;
      } else {
        iter = end;
      }
    } else if (*iter == '*') {
      width = va_arg(args, int);
      iter++;
    }

    /* precision argument */
    precision = NK_DEFAULT;

    if (*iter == '.') {
      iter++;

      if (*iter == '*') {
        precision = va_arg(args, int);
        iter++;
      } else {
        char *end;

        precision = nk_strtoi(iter, &end);

        if (end == iter) {
          precision = -1;
        } else {
          iter = end;
        }
      }
    }

    /* length modifier */
    if (*iter == 'h') {
      if (*(iter + 1) == 'h') {
        arg_type = NK_ARG_TYPE_CHAR;
        iter++;
      } else {
        arg_type = NK_ARG_TYPE_SHORT;
      }

      iter++;
    } else if (*iter == 'l') {
      arg_type = NK_ARG_TYPE_LONG;
      iter++;
    } else {
      arg_type = NK_ARG_TYPE_DEFAULT;
    }

    /* specifier */
    if (*iter == '%') {
      NK_ASSERT(arg_type == NK_ARG_TYPE_DEFAULT);
      NK_ASSERT(precision == NK_DEFAULT);
      NK_ASSERT(width == NK_DEFAULT);

      if (len < buf_size) {
        buf[len++] = '%';
      }
    } else if (*iter == 's') {
      /* string  */
      const char *str = va_arg(args, const char *);

      NK_ASSERT(str != buf && "buffer and argument are not allowed to overlap!");
      NK_ASSERT(arg_type == NK_ARG_TYPE_DEFAULT);
      NK_ASSERT(precision == NK_DEFAULT);
      NK_ASSERT(width == NK_DEFAULT);

      if (str == buf) {
        return -1;
      }

      while (str && *str && len < buf_size) {
        buf[len++] = *str++;
      }
    } else if (*iter == 'n') {
      /* current length callback */
      signed int *n = va_arg(args, int *);

      NK_ASSERT(arg_type == NK_ARG_TYPE_DEFAULT);
      NK_ASSERT(precision == NK_DEFAULT);
      NK_ASSERT(width == NK_DEFAULT);

      if (n) {
        *n = len;
      }
    } else if (*iter == 'c' || *iter == 'i' || *iter == 'd') {
      /* signed integer */
      long        value = 0;
      const char *num_iter;
      int         num_len, num_print, padding;
      int         cur_precision = NK_MAX(precision, 1);
      int         cur_width     = NK_MAX(width, 0);

      /* retrieve correct value type */
      if (arg_type == NK_ARG_TYPE_CHAR) {
        value = (signed char)va_arg(args, int);
      } else if (arg_type == NK_ARG_TYPE_SHORT) {
        value = (signed short)va_arg(args, int);
      } else if (arg_type == NK_ARG_TYPE_LONG) {
        value = va_arg(args, signed long);
      } else if (*iter == 'c') {
        value = (unsigned char)va_arg(args, int);
      } else {
        value = va_arg(args, signed int);
      }

      /* convert number to string */
      nk_itoa(number_buffer, value);
      num_len = nk_strlen(number_buffer);
      padding = NK_MAX(cur_width - NK_MAX(cur_precision, num_len), 0);

      if ((flag & NK_ARG_FLAG_PLUS) || (flag & NK_ARG_FLAG_SPACE)) {
        padding = NK_MAX(padding - 1, 0);
      }

      /* fill left padding up to a total of `width` characters */
      if (!(flag & NK_ARG_FLAG_LEFT)) {
        while (padding-- > 0 && (len < buf_size)) {
          if ((flag & NK_ARG_FLAG_ZERO) && (precision == NK_DEFAULT)) {
            buf[len++] = '0';
          } else {
            buf[len++] = ' ';
          }
        }
      }

      /* copy string value representation into buffer */
      if ((flag & NK_ARG_FLAG_PLUS) && value >= 0 && len < buf_size) {
        buf[len++] = '+';
      } else if ((flag & NK_ARG_FLAG_SPACE) && value >= 0 && len < buf_size) {
        buf[len++] = ' ';
      }

      /* fill up to precision number of digits with '0' */
      num_print = NK_MAX(cur_precision, num_len);

      while (precision && (num_print > num_len) && (len < buf_size)) {
        buf[len++] = '0';
        num_print--;
      }

      /* copy string value representation into buffer */
      num_iter = number_buffer;

      while (precision && *num_iter && len < buf_size) {
        buf[len++] = *num_iter++;
      }

      /* fill right padding up to width characters */
      if (flag & NK_ARG_FLAG_LEFT) {
        while ((padding-- > 0) && (len < buf_size)) {
          buf[len++] = ' ';
        }
      }
    } else if (*iter == 'o' || *iter == 'x' || *iter == 'X' || *iter == 'u') {
      /* unsigned integer */
      unsigned long value         = 0;
      int           num_len       = 0, num_print, padding = 0;
      int           cur_precision = NK_MAX(precision, 1);
      int           cur_width     = NK_MAX(width, 0);
      unsigned int  base          = (*iter == 'o') ? 8 : (*iter == 'u') ? 10 : 16;

      /* print oct/hex/dec value */
      const char *upper_output_format = "0123456789ABCDEF";
      const char *lower_output_format = "0123456789abcdef";
      const char *output_format       = (*iter == 'x') ? lower_output_format : upper_output_format;

      /* retrieve correct value type */
      if (arg_type == NK_ARG_TYPE_CHAR) {
        value = (unsigned char)va_arg(args, int);
      } else if (arg_type == NK_ARG_TYPE_SHORT) {
        value = (unsigned short)va_arg(args, int);
      } else if (arg_type == NK_ARG_TYPE_LONG) {
        value = va_arg(args, unsigned long);
      } else {
        value = va_arg(args, unsigned int);
      }

      do {
        /* convert decimal number into hex/oct number */
        int digit = output_format[value % base];

        if (num_len < NK_MAX_NUMBER_BUFFER) {
          number_buffer[num_len++] = (char)digit;
        }

        value /= base;
      } while (value > 0);

      num_print = NK_MAX(cur_precision, num_len);
      padding   = NK_MAX(cur_width - NK_MAX(cur_precision, num_len), 0);

      if (flag & NK_ARG_FLAG_NUM) {
        padding = NK_MAX(padding - 1, 0);
      }

      /* fill left padding up to a total of `width` characters */
      if (!(flag & NK_ARG_FLAG_LEFT)) {
        while ((padding-- > 0) && (len < buf_size)) {
          if ((flag & NK_ARG_FLAG_ZERO) && (precision == NK_DEFAULT)) {
            buf[len++] = '0';
          } else {
            buf[len++] = ' ';
          }
        }
      }

      /* fill up to precision number of digits */
      if (num_print && (flag & NK_ARG_FLAG_NUM)) {
        if ((*iter == 'o') && (len < buf_size)) {
          buf[len++] = '0';
        } else if ((*iter == 'x') && ((len + 1) < buf_size)) {
          buf[len++] = '0';
          buf[len++] = 'x';
        } else if ((*iter == 'X') && ((len + 1) < buf_size)) {
          buf[len++] = '0';
          buf[len++] = 'X';
        }
      }

      while (precision && (num_print > num_len) && (len < buf_size)) {
        buf[len++] = '0';
        num_print--;
      }

      /* reverse number direction */
      while (num_len > 0) {
        if (precision && (len < buf_size)) {
          buf[len++] = number_buffer[num_len - 1];
        }

        num_len--;
      }

      /* fill right padding up to width characters */
      if (flag & NK_ARG_FLAG_LEFT) {
        while ((padding-- > 0) && (len < buf_size)) {
          buf[len++] = ' ';
        }
      }
    } else if (*iter == 'f') {
      /* floating point */
      const char *num_iter;
      int         cur_precision = (precision < 0) ? 6 : precision;
      int         prefix, cur_width = NK_MAX(width, 0);
      double      value   = va_arg(args, double);
      int         num_len = 0, frac_len = 0, dot = 0;
      int         padding = 0;

      NK_ASSERT(arg_type == NK_ARG_TYPE_DEFAULT);
      NK_DTOA(number_buffer, value);
      num_len = nk_strlen(number_buffer);

      /* calculate padding */
      num_iter = number_buffer;

      while (*num_iter && *num_iter != '.') {
        num_iter++;
      }

      prefix  = (*num_iter == '.') ? (int)(num_iter - number_buffer) + 1 : 0;
      padding = NK_MAX(cur_width - (prefix + NK_MIN(cur_precision, num_len - prefix)), 0);

      if ((flag & NK_ARG_FLAG_PLUS) || (flag & NK_ARG_FLAG_SPACE)) {
        padding = NK_MAX(padding - 1, 0);
      }

      /* fill left padding up to a total of `width` characters */
      if (!(flag & NK_ARG_FLAG_LEFT)) {
        while (padding-- > 0 && (len < buf_size)) {
          if (flag & NK_ARG_FLAG_ZERO) {
            buf[len++] = '0';
          } else {
            buf[len++] = ' ';
          }
        }
      }

      /* copy string value representation into buffer */
      num_iter = number_buffer;

      if ((flag & NK_ARG_FLAG_PLUS) && (value >= 0) && (len < buf_size)) {
        buf[len++] = '+';
      } else if ((flag & NK_ARG_FLAG_SPACE) && (value >= 0) && (len < buf_size)) {
        buf[len++] = ' ';
      }

      while (*num_iter) {
        if (dot) {
          frac_len++;
        }

        if (len < buf_size) {
          buf[len++] = *num_iter;
        }

        if (*num_iter == '.') {
          dot = 1;
        }

        if (frac_len >= cur_precision) {
          break;
        }

        num_iter++;
      }

      /* fill number up to precision */
      while (frac_len < cur_precision) {
        if (!dot && len < buf_size) {
          buf[len++] = '.';
          dot = 1;
        }

        if (len < buf_size) {
          buf[len++] = '0';
        }

        frac_len++;
      }

      /* fill right padding up to width characters */
      if (flag & NK_ARG_FLAG_LEFT) {
        while ((padding-- > 0) && (len < buf_size)) {
          buf[len++] = ' ';
        }
      }
    } else {
      /* Specifier not supported: g,G,e,E,p,z */
      NK_ASSERT(0 && "specifier is not supported!");
      return result;
    }
  }

  buf[(len >= buf_size) ? (buf_size - 1) : len] = 0;
  result = (len >= buf_size) ? -1 : len;
  return result;
}
#  endif

NK_LIB int
nk_strfmt(char *buf, int buf_size, const char *fmt, va_list args)
{
  int result = -1;

  NK_ASSERT(buf);
  NK_ASSERT(buf_size);

  if (!buf || !buf_size || !fmt) {
    return 0;
  }
#  ifdef NK_INCLUDE_STANDARD_IO
  result            = NK_VSNPRINTF(buf, (nk_size)buf_size, fmt, args);
  result            = (result >= buf_size) ? -1 : result;
  buf[buf_size - 1] = 0;
#  else
  result = nk_vsnprintf(buf, buf_size, fmt, args);
#  endif
  return result;
}
#endif

NK_API nk_hash
nk_murmur_hash(const void *key, int len, nk_hash seed)
{
/* 32-Bit MurmurHash3: https://code.google.com/p/smhasher/wiki/MurmurHash3*/
#define NK_ROTL(x, r) ((x) << (r) | ((x) >> (32 - r)))

  nk_uint        h1 = seed;
  nk_uint        k1;
  const nk_byte *data   = (const nk_byte *)key;
  const nk_byte *keyptr = data;
  nk_byte       *k1ptr;
  const int      bsize   = sizeof(k1);
  const int      nblocks = len / 4;

  const nk_uint  c1 = 0xcc9e2d51;
  const nk_uint  c2 = 0x1b873593;
  const nk_byte *tail;
  int            i;

  /* body */
  if (!key) {
    return 0;
  }

  for (i = 0; i < nblocks; ++i, keyptr += bsize) {
    k1ptr    = (nk_byte *)&k1;
    k1ptr[0] = keyptr[0];
    k1ptr[1] = keyptr[1];
    k1ptr[2] = keyptr[2];
    k1ptr[3] = keyptr[3];

    k1 *= c1;
    k1  = NK_ROTL(k1, 15);
    k1 *= c2;

    h1 ^= k1;
    h1  = NK_ROTL(h1, 13);
    h1  = h1 * 5 + 0xe6546b64;
  }

  /* tail */
  tail = (const nk_byte *)(data + nblocks * 4);
  k1   = 0;

  switch (len & 3) {
    case 3:
      k1 ^= (nk_uint)(tail[2] << 16); /* fallthrough */

    case 2:
      k1 ^= (nk_uint)(tail[1] << 8u); /* fallthrough */

    case 1:
      k1 ^= tail[0];
      k1 *= c1;
      k1  = NK_ROTL(k1, 15);
      k1 *= c2;
      h1 ^= k1;
      break;

    default:
      break;
  }

  /* finalization */
  h1 ^= (nk_uint)len;
  /* fmix32 */
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

#undef NK_ROTL
  return h1;
}

NK_INTERN struct nk_text_shape *
nk_text_shape_build_default(struct nk_context *ctx, const struct nk_user_font *font, float height, const char *text, int len);

/* bytes of the whole clusters that fit in space */
NK_INTERN int
nk_text_clamp_shape(const struct nk_text_shape *shape, float space, int *glyphs, float *text_width)
{
  float width = 0.0f;
  int   len   = 0;
  int   i;

  *glyphs = 0;

  for (i = 0; shape && i < shape->num_clusters; ++i) {
    const struct nk_text_cluster *cluster = &shape->clusters[i];
    float                         next    = width;
    int                           glyph_idx;

    for (glyph_idx = cluster->glyph_begin; glyph_idx < cluster->glyph_begin + cluster->glyph_count; ++glyph_idx) {
      next += shape->glyphs[glyph_idx].advance_x;
    }

    if (next > space) {
      break;
    }

    width    = next;
    len      = cluster->source_end;
    *glyphs += nk_utf_len(shape->source + cluster->source_begin, cluster->source_end - cluster->source_begin);
  }

  *text_width = width;
  return len;
}

NK_LIB int
nk_text_clamp(struct nk_context *ctx, const struct nk_user_font *font, const char *text, int text_len, float space, int *glyphs, float *text_width)
{
  struct nk_text_shape *shape = nk_text_shape_build_default(ctx, font, ctx->style.font_size, text, text_len);

  if (!shape) {
    *glyphs     = 0;
    *text_width = 0.0f;
    return 0;
  }
  return nk_text_clamp_shape(shape, space, glyphs, text_width);
}

NK_LIB struct nk_vec2
nk_text_calculate_text_bounds(struct nk_context *ctx, const struct nk_user_font *font, const char *begin, int byte_len,
                              float row_height, const char **remaining, struct nk_vec2 *out_offset, int *glyphs, int op)
{
  float          line_height = row_height;
  struct nk_vec2 text_size   = nk_vec2(0, 0);
  float          line_width  = 0.0f;

  int     glyph_len          = 0;
  nk_rune unicode            = 0;
  int     text_len           = 0;
  int     line_begin         = 0;
  nk_bool stopped_on_newline = nk_false;

  *glyphs = 0;

  if (!begin || byte_len <= 0 || !font) {
    return nk_vec2(0, row_height);
  }

  glyph_len = nk_utf_decode(begin, &unicode, byte_len);

  if (!glyph_len) {
    return text_size;
  }

  *glyphs = 0;

  while ((text_len < byte_len) && glyph_len) {
    if (unicode == '\n') {
      line_width   = nk_text_width(ctx, font, ctx->style.font_size, begin + line_begin, text_len - line_begin);
      text_size.x  = NK_MAX(text_size.x, line_width);
      text_size.y += line_height;
      *glyphs     += 1;

      if (op == NK_STOP_ON_NEW_LINE) {
        stopped_on_newline = nk_true;
        break;
      }

      text_len  += glyph_len;
      line_begin = text_len;
      glyph_len  = nk_utf_decode(begin + text_len, &unicode, byte_len - text_len);
      continue;
    }

    if (unicode == '\r') {
      text_len += glyph_len;
      *glyphs  += 1;
      glyph_len = nk_utf_decode(begin + text_len, &unicode, byte_len - text_len);
      continue;
    }

    *glyphs   = *glyphs + 1;
    text_len += glyph_len;
    glyph_len = nk_utf_decode(begin + text_len, &unicode, byte_len - text_len);
  }

  line_width = nk_text_width(ctx, font, ctx->style.font_size, begin + line_begin, text_len - line_begin);

  if (text_size.x < line_width) {
    text_size.x = line_width;
  }

  if (!stopped_on_newline && (line_width > 0 || text_size.y == 0.0f)) {
    text_size.y += line_height;
  }

  if (out_offset) {
    *out_offset = nk_vec2(line_width, text_size.y);
  }

  if (remaining) {
    *remaining = begin + text_len;
  }
  return text_size;
}

struct nk_text_geometry_run {
  nk_handle texture;
  int       quad_begin;
  int       quad_count;
};

struct nk_text_geometry_quad {
  struct nk_vec2 min;
  struct nk_vec2 max;
  struct nk_vec2 uv_min;
  struct nk_vec2 uv_max;
};

struct nk_text_geometry {
  struct nk_text_geometry_run  *runs;
  int                           num_runs;
  struct nk_text_geometry_quad *quads;
  int                           num_quads;
};

/*
 * Text cache: a ring of records in caller memory; a record holds the key, the shape and the
 * geometry. When the ring is full the tail makes room: records used in this frame stay (the
 * allocation fails), records used in the previous frame move to the head, others are dropped.
 */
#define NK_TEXT_CACHE_ALIGN 16

struct nk_text_cache_entry {
  nk_size                     size;
  unsigned int                last_used; /* ctx->seq of the last use */
  nk_bool                     alive;     /* false: replaced, only waits to be reclaimed */
  struct nk_text_cache_entry *next;
  nk_hash                     hash;
  const struct nk_user_font  *font;
  float                       font_height;
  enum nk_text_direction      direction;
  nk_uint                     script_tag;
  char                       *source;
  int                         source_len;
  char                       *lang;
  int                         lang_len;
  struct nk_text_feature     *features;
  int                         num_features;
  float                       width;
  nk_bool                     measured;
  struct nk_text_shape       *shape;    /* NULL until a shape is needed */
  struct nk_text_geometry    *geometry; /* NULL until drawn */
};

typedef unsigned long long nk_text_hash_word;

NK_INTERN nk_text_hash_word
nk_text_hash_mix(nk_text_hash_word h, nk_text_hash_word v)
{
  h ^= v * 0x9E3779B97F4A7C15ull;
  h  = (h << 31 | h >> 33) * 0xBF58476D1CE4E5B9ull;
  return h;
}

NK_INTERN nk_text_hash_word
nk_text_hash_bytes(nk_text_hash_word h, const void *data, int len)
{
  const nk_byte    *p = (const nk_byte *)data;
  nk_text_hash_word v;

  for (; len >= 8; len -= 8, p += 8) {
    NK_MEMCPY(&v, p, 8);
    h = nk_text_hash_mix(h, v);
  }

  v = (nk_text_hash_word)len << 56;

  if (len > 0) {
    nk_text_hash_word tail = 0;

    NK_MEMCPY(&tail, p, (nk_size)len);
    v ^= tail;
  }
  return nk_text_hash_mix(h, v);
}

NK_INTERN nk_hash
nk_text_request_hash(const struct nk_text_request *request)
{
  nk_text_hash_word h = 0x61E5F08D;
  nk_uint           height;

  NK_MEMCPY(&height, &request->font_height, sizeof(height));
  h = nk_text_hash_mix(h, (nk_text_hash_word)NK_PTR_TO_UINT(request->font));
  h = nk_text_hash_mix(h, (nk_text_hash_word)height | (nk_text_hash_word)(nk_uint)request->direction << 32);
  h = nk_text_hash_mix(h, (nk_text_hash_word)request->script_tag | (nk_text_hash_word)(nk_uint)request->num_features << 32);
  h = nk_text_hash_bytes(h, request->source, request->source_len);

  if (request->lang_len) {
    h = nk_text_hash_bytes(h, request->lang, request->lang_len);
  }

  if (request->num_features) {
    h = nk_text_hash_bytes(h, request->features, (int)sizeof(*request->features) * request->num_features);
  }

  h ^= h >> 32;
  h *= 0xD6E8FEB86659FD93ull;
  h ^= h >> 32;
  return (nk_hash)h;
}

NK_INTERN nk_bool
nk_text_memory_equal(const void *a, const void *b, int size)
{
#ifdef NK_MEMCMP
  return size <= 0 || NK_MEMCMP(a, b, (nk_size)size) == 0;
#else
  const nk_byte *left  = (const nk_byte *)a;
  const nk_byte *right = (const nk_byte *)b;
  int            i;

  for (i = 0; i < size; ++i) {
    if (left[i] != right[i]) {
      return nk_false;
    }
  }
  return nk_true;
#endif
}

NK_INTERN nk_bool
nk_text_cache_entry_matches(const struct nk_text_cache_entry *entry, nk_hash hash, const struct nk_text_request *request)
{
  int i;

  if (entry->hash != hash || entry->font != request->font || entry->font_height != request->font_height ||
      entry->direction != request->direction || entry->script_tag != request->script_tag ||
      entry->source_len != request->source_len || entry->lang_len != request->lang_len ||
      entry->num_features != request->num_features ||
      !nk_text_memory_equal(entry->source, request->source, request->source_len) ||
      !nk_text_memory_equal(entry->lang, request->lang, request->lang_len)) {
    return nk_false;
  }

  for (i = 0; i < request->num_features; ++i) {
    if (entry->features[i].tag != request->features[i].tag || entry->features[i].value != request->features[i].value) {
      return nk_false;
    }
  }
  return nk_true;
}

NK_INTERN nk_bool
nk_text_cache_enabled(const struct nk_context *ctx)
{
  return ctx->text_cache.buckets != NULL;
}

NK_INTERN nk_size
nk_text_cache_align(nk_size size)
{
  return (size + (NK_TEXT_CACHE_ALIGN - 1)) & ~(nk_size)(NK_TEXT_CACHE_ALIGN - 1);
}

NK_INTERN struct nk_text_cache_entry *
nk_text_cache_at(const struct nk_text_cache *cache, nk_size offset)
{
  return (struct nk_text_cache_entry *)(void *)(cache->ring + offset);
}

/* ---- ring allocation ---- */

/* false when size bytes do not fit without reclaiming */
NK_INTERN nk_bool
nk_text_cache_room(struct nk_text_cache *cache, nk_size size, nk_size *offset)
{
  if (!cache->used) {
    cache->head = cache->tail = 0;
    cache->wrap = cache->capacity;
  }

  if (cache->head >= cache->tail && (cache->used == 0 || cache->head != cache->tail)) {
    if (size <= cache->capacity - cache->head) {
      *offset = cache->head;
      return nk_true;
    }

    if (size <= cache->tail) {
      cache->wrap = cache->head;
      *offset     = 0;
      return nk_true;
    }
    return nk_false;
  }

  if (size <= cache->tail - cache->head) {
    *offset = cache->head;
    return nk_true;
  }
  return nk_false;
}

NK_INTERN void *
nk_text_cache_take(struct nk_text_cache *cache, nk_size offset, nk_size size)
{
  cache->head  = offset + size;
  cache->used += size;
  return cache->ring + offset;
}

NK_INTERN void
nk_text_cache_unlink(struct nk_context *ctx, struct nk_text_cache_entry *entry)
{
  struct nk_text_cache_entry **link = &ctx->text_cache.buckets[entry->hash % NK_TEXT_CACHE_BUCKET_COUNT];

  while (*link && *link != entry) {
    link = &(*link)->next;
  }

  if (*link) {
    *link = entry->next;
  }
}

NK_INTERN void
nk_text_cache_link(struct nk_context *ctx, struct nk_text_cache_entry *entry)
{
  struct nk_text_cache_entry **bucket = &ctx->text_cache.buckets[entry->hash % NK_TEXT_CACHE_BUCKET_COUNT];

  entry->next = *bucket;
  *bucket     = entry;
}

NK_INTERN void
nk_text_cache_relocate(struct nk_text_cache_entry *entry, nk_byte *from, nk_byte *to)
{
#define NK_TEXT_MOVED(T, p) ((p) ? (T *)(void *)(to + ((nk_byte *)(void *)(p) - from)) : NULL)
  entry->source   = NK_TEXT_MOVED(char, entry->source);
  entry->lang     = NK_TEXT_MOVED(char, entry->lang);
  entry->features = NK_TEXT_MOVED(struct nk_text_feature, entry->features);
  entry->shape    = NK_TEXT_MOVED(struct nk_text_shape, entry->shape);
  entry->geometry = NK_TEXT_MOVED(struct nk_text_geometry, entry->geometry);

  if (entry->shape) {
    entry->shape->source     = NK_TEXT_MOVED(char, entry->shape->source);
    entry->shape->boundaries = NK_TEXT_MOVED(struct nk_text_boundary, entry->shape->boundaries);
    entry->shape->clusters   = NK_TEXT_MOVED(struct nk_text_cluster, entry->shape->clusters);
    entry->shape->runs       = NK_TEXT_MOVED(struct nk_text_run, entry->shape->runs);
    entry->shape->glyphs     = NK_TEXT_MOVED(struct nk_text_glyph, entry->shape->glyphs);
  }

  if (entry->geometry) {
    entry->geometry->runs  = NK_TEXT_MOVED(struct nk_text_geometry_run, entry->geometry->runs);
    entry->geometry->quads = NK_TEXT_MOVED(struct nk_text_geometry_quad, entry->geometry->quads);
  }
#undef NK_TEXT_MOVED
}

/* false when the tail record is used in this frame */
NK_INTERN nk_bool
nk_text_cache_reclaim(struct nk_context *ctx)
{
  struct nk_text_cache       *cache = &ctx->text_cache;
  struct nk_text_cache_entry *entry;
  nk_size                     offset;

  if (!cache->used) {
    return nk_false;
  }

  if (cache->tail == cache->wrap) {
    cache->tail = 0;
    cache->wrap = cache->capacity;
  }

  entry = nk_text_cache_at(cache, cache->tail);

  if (entry->last_used == ctx->seq) {
    return nk_false;
  }

  if (entry->alive) {
    nk_text_cache_unlink(ctx, entry);

    if (entry->last_used + 1 == ctx->seq && nk_text_cache_room(cache, entry->size, &offset) && offset != cache->tail) {
      struct nk_text_cache_entry *moved = (struct nk_text_cache_entry *)nk_text_cache_take(cache, offset, entry->size);

      NK_MEMCPY(moved, entry, entry->size);
      nk_text_cache_relocate(moved, (nk_byte *)entry, (nk_byte *)moved);
      nk_text_cache_link(ctx, moved);
    }
  }

  cache->tail += entry->size;
  cache->used -= entry->size;

  if (cache->tail == cache->wrap) {
    cache->tail = 0;
    cache->wrap = cache->capacity;
  }
  return nk_true;
}

/* NULL when this frame's text fills the ring */
NK_INTERN void *
nk_text_cache_alloc(struct nk_context *ctx, nk_size size)
{
  struct nk_text_cache *cache = &ctx->text_cache;
  nk_size               offset;

  size = nk_text_cache_align(size);

  if (size > cache->capacity / 2) {
    return NULL;
  }

  while (!nk_text_cache_room(cache, size, &offset)) {
    if (!nk_text_cache_reclaim(ctx)) {
      return NULL;
    }
  }
  return nk_text_cache_take(cache, offset, size);
}

/* ---- records ---- */

NK_INTERN nk_size
nk_text_shape_size(const struct nk_text_shape *shape)
{
  if (!shape) {
    return 0;
  }
  return nk_text_cache_align(sizeof(*shape)) + nk_text_cache_align(sizeof(*shape->boundaries) * (nk_size)shape->num_boundaries) +
         nk_text_cache_align(sizeof(*shape->clusters) * (nk_size)shape->num_clusters) +
         nk_text_cache_align(sizeof(*shape->runs) * (nk_size)shape->num_runs) +
         nk_text_cache_align(sizeof(*shape->glyphs) * (nk_size)shape->num_glyphs);
}

NK_INTERN nk_size
nk_text_geometry_size(int num_runs, int num_quads)
{
  return nk_text_cache_align(sizeof(struct nk_text_geometry)) + nk_text_cache_align(sizeof(struct nk_text_geometry_run) * (nk_size)num_runs) +
         nk_text_cache_align(sizeof(struct nk_text_geometry_quad) * (nk_size)num_quads);
}

NK_INTERN void *
nk_text_record_piece(nk_byte **cursor, nk_size size)
{
  void *piece = *cursor;

  *cursor += nk_text_cache_align(size);
  return size ? piece : NULL;
}

NK_INTERN void *
nk_text_record_copy(nk_byte **cursor, const void *source, nk_size size)
{
  void *piece = nk_text_record_piece(cursor, size);

  if (piece) {
    NK_MEMCPY(piece, source, size);
  }
  return piece;
}

NK_INTERN void
nk_text_geometry_fill(struct nk_text_geometry *geometry, const struct nk_text_shape *shape, float font_height)
{
  struct nk_vec2 pen = nk_vec2(0.0f, 0.0f);
  int            run_idx;

  for (run_idx = 0; run_idx < shape->num_runs; ++run_idx) {
    const struct nk_text_run *run = &shape->runs[run_idx];
    int                       glyph_idx;

    geometry->runs[run_idx].texture    = run->font->texture;
    geometry->runs[run_idx].quad_begin = run->glyph_begin;
    geometry->runs[run_idx].quad_count = run->glyph_count;

    for (glyph_idx = run->glyph_begin; glyph_idx < run->glyph_begin + run->glyph_count; ++glyph_idx) {
      const struct nk_text_glyph   *glyph = &shape->glyphs[glyph_idx];
      struct nk_text_geometry_quad *quad  = &geometry->quads[glyph_idx];
      struct nk_user_font_glyph     font_glyph;
      float                         x;
      float                         y;

      run->font->query_glyph(run->font->userdata, font_height, &font_glyph, glyph->id);
      x = pen.x + glyph->offset_x + font_glyph.offset.x;
      y = pen.y + shape->baseline + glyph->offset_y + font_glyph.offset.y;

      quad->min    = nk_vec2(x, y);
      quad->max    = nk_vec2(x + font_glyph.width, y + font_glyph.height);
      quad->uv_min = font_glyph.uv[0];
      quad->uv_max = font_glyph.uv[1];

      pen.x += glyph->advance_x;
      pen.y += glyph->advance_y;
    }
  }
}

/* NULL when it does not fit; replaces (or NULL) is the record it supersedes */
NK_INTERN struct nk_text_cache_entry *
nk_text_cache_write(struct nk_context *ctx, const struct nk_text_request *request, nk_hash hash,
                    const struct nk_text_shape *shape, nk_bool with_geometry, struct nk_text_cache_entry *replaces)
{
  struct nk_text_cache_entry *entry;
  nk_byte                    *cursor;
  nk_size                     size;
  float                       width    = replaces ? replaces->width : 0.0f;
  nk_bool                     measured = replaces ? replaces->measured : nk_false;

  size = nk_text_cache_align(sizeof(*entry)) + nk_text_cache_align((nk_size)request->source_len) +
         nk_text_cache_align((nk_size)request->lang_len) + nk_text_cache_align(sizeof(*request->features) * (nk_size)request->num_features) +
         nk_text_shape_size(shape) + (with_geometry && shape ? nk_text_geometry_size(shape->num_runs, shape->num_glyphs) : 0);

  if (replaces) {
    /* not reclaimed while copied from */
    replaces->last_used = ctx->seq;
  }

  entry = (struct nk_text_cache_entry *)nk_text_cache_alloc(ctx, size);

  if (!entry) {
    return NULL;
  }

  cursor = (nk_byte *)entry;
  nk_text_record_piece(&cursor, sizeof(*entry));
  NK_MEMSET(entry, 0, sizeof(*entry));
  entry->size         = nk_text_cache_align(size);
  entry->last_used    = ctx->seq;
  entry->alive        = nk_true;
  entry->hash         = hash;
  entry->font         = request->font;
  entry->font_height  = request->font_height;
  entry->direction    = request->direction;
  entry->script_tag   = request->script_tag;
  entry->source_len   = request->source_len;
  entry->lang_len     = request->lang_len;
  entry->num_features = request->num_features;
  entry->width        = width;
  entry->measured     = measured;
  entry->source       = (char *)nk_text_record_copy(&cursor, request->source, (nk_size)request->source_len);
  entry->lang         = (char *)nk_text_record_copy(&cursor, request->lang, (nk_size)request->lang_len);
  entry->features     = (struct nk_text_feature *)nk_text_record_copy(&cursor, request->features, sizeof(*request->features) * (nk_size)request->num_features);

  if (shape) {
    struct nk_text_shape *copy = (struct nk_text_shape *)nk_text_record_copy(&cursor, shape, sizeof(*shape));

    copy->source     = entry->source;
    copy->boundaries = (struct nk_text_boundary *)nk_text_record_copy(&cursor, shape->boundaries, sizeof(*shape->boundaries) * (nk_size)shape->num_boundaries);
    copy->clusters   = (struct nk_text_cluster *)nk_text_record_copy(&cursor, shape->clusters, sizeof(*shape->clusters) * (nk_size)shape->num_clusters);
    copy->runs       = (struct nk_text_run *)nk_text_record_copy(&cursor, shape->runs, sizeof(*shape->runs) * (nk_size)shape->num_runs);
    copy->glyphs     = (struct nk_text_glyph *)nk_text_record_copy(&cursor, shape->glyphs, sizeof(*shape->glyphs) * (nk_size)shape->num_glyphs);
    entry->shape     = copy;
    entry->width     = shape->advance.x;
    entry->measured  = nk_true;

    if (with_geometry) {
      struct nk_text_geometry *geometry = (struct nk_text_geometry *)nk_text_record_piece(&cursor, sizeof(*geometry));

      geometry->num_runs  = shape->num_runs;
      geometry->num_quads = shape->num_glyphs;
      geometry->runs      = (struct nk_text_geometry_run *)nk_text_record_piece(&cursor, sizeof(*geometry->runs) * (nk_size)shape->num_runs);
      geometry->quads     = (struct nk_text_geometry_quad *)nk_text_record_piece(&cursor, sizeof(*geometry->quads) * (nk_size)shape->num_glyphs);
      nk_text_geometry_fill(geometry, copy, request->font_height);
      entry->geometry = geometry;
    }
  }

  if (replaces) {
    nk_text_cache_unlink(ctx, replaces);
    replaces->alive = nk_false;
  }

  nk_text_cache_link(ctx, entry);
  return entry;
}

NK_INTERN struct nk_text_cache_entry *
nk_text_cache_find(struct nk_context *ctx, const struct nk_text_request *request, nk_hash hash)
{
  struct nk_text_cache_entry *entry;

  if (!nk_text_cache_enabled(ctx)) {
    return NULL;
  }

  for (entry = ctx->text_cache.buckets[hash % NK_TEXT_CACHE_BUCKET_COUNT]; entry; entry = entry->next) {
    if (nk_text_cache_entry_matches(entry, hash, request)) {
      entry->last_used      = ctx->seq;
      ctx->text_cache.hits += 1;
      return entry;
    }
  }

  ctx->text_cache.misses += 1;
  return NULL;
}

NK_API void
nk_text_chunk_list_init(struct nk_text_chunk_list *list, nk_size item_size)
{
  NK_ASSERT(list);
  NK_MEMSET(list, 0, sizeof(*list));
  list->item_size = item_size;
}

NK_API nk_bool
nk_text_chunk_list_push(struct nk_text_chunk_list *list, struct nk_buffer *buffer, const void *item)
{
  struct nk_text_chunk *chunk;

  NK_ASSERT(list);
  NK_ASSERT(buffer);
  NK_ASSERT(item);
  NK_ASSERT(list->item_size);
  NK_ASSERT(list->item_size <= NK_TEXT_CHUNK_DATA_SIZE);

  chunk = list->last;

  if (!chunk || chunk->used + list->item_size > NK_TEXT_CHUNK_DATA_SIZE) {
    chunk = nk_buffer_alloc(buffer, NK_BUFFER_FRONT, sizeof(*chunk), NK_ALIGNOF(struct nk_text_chunk));

    if (!chunk) {
      return nk_false;
    }

    chunk->next = NULL;
    chunk->used = 0;

    if (list->last) {
      list->last->next = chunk;
    } else {
      list->first = chunk;
    }

    list->last = chunk;
  }

  NK_MEMCPY(chunk->data + chunk->used, item, list->item_size);
  chunk->used += list->item_size;
  list->count++;
  return nk_true;
}

NK_API void *
nk_text_chunk_list_flatten(const struct nk_text_chunk_list *list, struct nk_buffer *buffer, nk_size alignment)
{
  const struct nk_text_chunk *chunk;
  nk_byte                    *result;
  nk_byte                    *destination;

  NK_ASSERT(list);
  NK_ASSERT(buffer);

  if (!list->count) {
    return NULL;
  }

  result = nk_buffer_alloc(buffer, NK_BUFFER_FRONT, list->count * list->item_size, alignment);

  if (!result) {
    return NULL;
  }

  destination = result;

  for (chunk = list->first; chunk; chunk = chunk->next) {
    NK_MEMCPY(destination, chunk->data, chunk->used);
    destination += chunk->used;
  }
  return result;
}

NK_API nk_bool
nk_text_memory_init(struct nk_context *ctx, void *memory, nk_size size)
{
  struct nk_text_cache *cache;
  nk_size               table = sizeof(struct nk_text_cache_entry *) * NK_TEXT_CACHE_BUCKET_COUNT;
  nk_size               skip;

  NK_ASSERT(ctx);
  NK_ASSERT(memory);

  if (!ctx || !memory) {
    return nk_false;
  }

  skip = (NK_TEXT_CACHE_ALIGN - ((nk_size)memory & (NK_TEXT_CACHE_ALIGN - 1))) & (NK_TEXT_CACHE_ALIGN - 1);

  if (size < skip + table + 4096) {
    return nk_false;
  }

  cache           = &ctx->text_cache;
  cache->buckets  = (struct nk_text_cache_entry **)nk_ptr_add(void, memory, skip);
  cache->ring     = (nk_byte *)nk_ptr_add(void, cache->buckets, table);
  cache->capacity = (size - skip - table) & ~(nk_size)(NK_TEXT_CACHE_ALIGN - 1);
  nk_text_memory_clear(ctx);
  return nk_true;
}

NK_API nk_bool
nk_text_memory_init_fixed(struct nk_context *ctx, nk_size cache_size)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->memory.type == NK_BUFFER_FIXED);
  NK_ASSERT(!ctx->memory.allocated);

  if (ctx->memory.type != NK_BUFFER_FIXED || cache_size >= ctx->memory.size) {
    return nk_false;
  }

  ctx->memory.memory.size -= cache_size;
  ctx->memory.size        -= cache_size;
  return nk_text_memory_init(ctx, nk_ptr_add(void, ctx->memory.memory.ptr, ctx->memory.size), cache_size);
}

NK_API void
nk_text_memory_clear(struct nk_context *ctx)
{
  struct nk_text_cache *cache = &ctx->text_cache;

  NK_ASSERT(ctx);

  if (nk_text_cache_enabled(ctx)) {
    NK_MEMSET(cache->buckets, 0, sizeof(*cache->buckets) * NK_TEXT_CACHE_BUCKET_COUNT);
    cache->head = cache->tail = cache->used = 0;
    cache->wrap = cache->capacity;
  }
}

NK_API void
nk_text_backend_set(struct nk_context *ctx, const struct nk_text_backend *backend)
{
  NK_ASSERT(ctx);
  NK_ASSERT(backend);
  nk_text_memory_clear(ctx);
  ctx->text_backend = *backend;
}

/* valid until the next shaping call */
NK_INTERN struct nk_text_shape *
nk_text_shape_build_temporary(struct nk_context *ctx, const struct nk_text_request *request)
{
  struct nk_text_shape *shape;

  NK_ASSERT(ctx->text_backend.build);

  if (!ctx->text_backend.build || !ctx->text_backend.temporary) {
    return NULL;
  }

  nk_buffer_clear(ctx->text_backend.temporary);
  NK_PROFILE_SCOPE_BEGIN("text.shape_backend", shape_backend);

  {
    shape = ctx->text_backend.build(ctx->text_backend.userdata, ctx->text_backend.temporary, request);
  }

  NK_PROFILE_SCOPE_END(shape_backend);
  return shape;
}

NK_INTERN struct nk_text_shape *
nk_text_shape_build_hashed(struct nk_context *ctx, const struct nk_text_request *request, nk_hash hash, struct nk_text_cache_entry **out_entry)
{
  struct nk_text_cache_entry *entry = nk_text_cache_find(ctx, request, hash);
  struct nk_text_shape       *shape;

  *out_entry = entry;

  if (entry && entry->shape) {
    return entry->shape;
  }

  shape = nk_text_shape_build_temporary(ctx, request);

  if (shape && nk_text_cache_enabled(ctx)) {
    struct nk_text_cache_entry *cached = nk_text_cache_write(ctx, request, hash, shape, nk_false, entry);

    if (cached) {
      *out_entry = cached;
      return cached->shape;
    }
  }
  return shape;
}

NK_API struct nk_text_shape *
nk_text_shape_build(struct nk_context *ctx, const struct nk_text_request *request)
{
  struct nk_text_cache_entry *entry;

  NK_ASSERT(ctx);
  NK_ASSERT(request);
  return nk_text_shape_build_hashed(ctx, request, nk_text_request_hash(request), &entry);
}

/* NULL without room: then draw from *shape */
NK_INTERN const struct nk_text_geometry *
nk_text_geometry_get(struct nk_context *ctx, const struct nk_text_request *request, const struct nk_text_shape **shape)
{
  nk_hash                     hash = nk_text_request_hash(request);
  struct nk_text_cache_entry *entry;

  *shape = nk_text_shape_build_hashed(ctx, request, hash, &entry);

  if (entry && entry->geometry) {
    return entry->geometry;
  }

  if (*shape && entry && entry->shape) {
    entry = nk_text_cache_write(ctx, request, hash, entry->shape, nk_true, entry);

    if (entry) {
      *shape = entry->shape;
      return entry->geometry;
    }
  }
  return NULL;
}

NK_INTERN struct nk_text_shape *
nk_text_shape_build_default(struct nk_context *ctx, const struct nk_user_font *font, float height, const char *text, int len)
{
  struct nk_text_request request = {0};

  if (!len) {
    return NULL;
  }

  request.source      = text;
  request.source_len  = len;
  request.font        = font;
  request.font_height = height;
  return nk_text_shape_build(ctx, &request);
}

NK_API float
nk_text_shape_width(const struct nk_text_shape *shape, int source_begin, int source_end)
{
  float width = 0.0f;
  int   i;

  if (source_begin == 0 && source_end == shape->source_len) {
    return shape->advance.x;
  }

  for (i = 0; i < shape->num_glyphs; ++i) {
    const struct nk_text_glyph   *glyph   = &shape->glyphs[i];
    const struct nk_text_cluster *cluster = &shape->clusters[glyph->cluster_idx];

    if (cluster->source_begin >= source_begin && cluster->source_end <= source_end) {
      width += glyph->advance_x;
    }
  }
  return width;
}

NK_API int
nk_text_shape_boundary_previous(const struct nk_text_shape *shape, int source_offset, nk_flags flags)
{
  int i;

  for (i = shape->num_boundaries - 1; i >= 0; --i) {
    if (shape->boundaries[i].source_offset < source_offset && (shape->boundaries[i].flags & flags)) {
      return shape->boundaries[i].source_offset;
    }
  }
  return 0;
}

NK_API int
nk_text_shape_boundary_next(const struct nk_text_shape *shape, int source_offset, nk_flags flags)
{
  int i;

  for (i = 0; i < shape->num_boundaries; ++i) {
    if (shape->boundaries[i].source_offset > source_offset && (shape->boundaries[i].flags & flags)) {
      return shape->boundaries[i].source_offset;
    }
  }
  return shape->source_len;
}

NK_INTERN nk_bool
nk_text_shape_cluster_glyph_bounds(const struct nk_text_shape *shape, int cluster_idx,
                                   float *left, float *right, enum nk_text_direction *direction)
{
  float   pen = 0.0f;
  int     run_idx;
  nk_bool found = nk_false;

  for (run_idx = 0; run_idx < shape->num_runs; ++run_idx) {
    struct nk_text_run *run = &shape->runs[run_idx];
    int                 glyph_idx;

    for (glyph_idx = run->glyph_begin; glyph_idx < run->glyph_begin + run->glyph_count; ++glyph_idx) {
      struct nk_text_glyph *glyph = &shape->glyphs[glyph_idx];
      float                 next  = pen + glyph->advance_x;

      if (glyph->cluster_idx == cluster_idx) {
        if (!found) {
          *left  = NK_MIN(pen, next);
          *right = NK_MAX(pen, next);
          found  = nk_true;
        } else {
          *left  = NK_MIN(*left,  NK_MIN(pen, next));
          *right = NK_MAX(*right, NK_MAX(pen, next));
        }

        *direction = run->direction;
      }

      pen = next;
    }
  }
  return found;
}

NK_INTERN nk_bool
nk_text_shape_cluster_bounds(const struct nk_text_shape *shape, int cluster_idx, float *left, float *right, enum nk_text_direction *direction)
{
  int   owner = cluster_idx;
  int   group_end;
  int   group_count;
  int   group_offset;
  float group_left;
  float group_right;
  float width;

  while (owner > 0 && !shape->clusters[owner].glyph_count) {
    owner -= 1;
  }

  if (!shape->clusters[owner].glyph_count ||
      !nk_text_shape_cluster_glyph_bounds(shape, owner, &group_left, &group_right, direction)) {
    return nk_false;
  }

  group_end = owner + 1;

  while (group_end < shape->num_clusters && !shape->clusters[group_end].glyph_count) {
    group_end += 1;
  }

  if (cluster_idx >= group_end) {
    return nk_false;
  }

  group_count  = group_end - owner;
  group_offset = cluster_idx - owner;
  width        = (group_right - group_left) / group_count;

  if (*direction == NK_TEXT_DIRECTION_RTL) {
    *left  = group_right - width * (group_offset + 1);
    *right = group_right - width * group_offset;
  } else {
    *left  = group_left + width * group_offset;
    *right = *left + width;
  }
  return nk_true;
}

NK_INTERN struct nk_text_cursor
nk_text_shape_cluster_edge(const struct nk_text_shape *shape, int cluster_idx, enum nk_text_affinity affinity)
{
  struct nk_text_cluster *cluster = &shape->clusters[cluster_idx];
  struct nk_text_cursor   cursor;
  enum nk_text_direction  direction;
  float                   left;
  float                   right;

  cursor.source_offset = affinity == NK_TEXT_AFFINITY_LEADING ? cluster->source_begin : cluster->source_end;
  cursor.affinity      = affinity;

  if (nk_text_shape_cluster_bounds(shape, cluster_idx, &left, &right, &direction)) {
    if (direction == NK_TEXT_DIRECTION_RTL) {
      cursor.x = affinity == NK_TEXT_AFFINITY_LEADING ? right : left;
    } else {
      cursor.x = affinity == NK_TEXT_AFFINITY_LEADING ? left : right;
    }
    return cursor;
  }

  if (affinity == NK_TEXT_AFFINITY_LEADING) {
    int previous;

    for (previous = cluster_idx - 1; previous >= 0; --previous) {
      if (shape->clusters[previous].glyph_count) {
        cursor.x = nk_text_shape_cluster_edge(shape, previous, NK_TEXT_AFFINITY_TRAILING).x;
        return cursor;
      }
    }
  } else {
    int next;

    for (next = cluster_idx + 1; next < shape->num_clusters; ++next) {
      if (shape->clusters[next].glyph_count) {
        cursor.x = nk_text_shape_cluster_edge(shape, next, NK_TEXT_AFFINITY_LEADING).x;
        return cursor;
      }
    }
  }

  cursor.x = affinity == NK_TEXT_AFFINITY_LEADING ? 0.0f : shape->advance.x;
  return cursor;
}

NK_API struct nk_text_cursor
nk_text_shape_cursor_at(const struct nk_text_shape *shape, int source_offset, enum nk_text_affinity affinity)
{
  struct nk_text_cursor cursor;
  int                   cluster_idx;

  cursor.source_offset = NK_CLAMP(0, source_offset, shape->source_len);
  cursor.affinity      = affinity;
  cursor.x             = source_offset <= 0 ? 0.0f : shape->advance.x;

  if (affinity == NK_TEXT_AFFINITY_LEADING) {
    for (cluster_idx = 0; cluster_idx < shape->num_clusters; ++cluster_idx) {
      if (shape->clusters[cluster_idx].source_begin == cursor.source_offset) {
        return nk_text_shape_cluster_edge(shape, cluster_idx, affinity);
      }
    }
  } else {
    for (cluster_idx = shape->num_clusters - 1; cluster_idx >= 0; --cluster_idx) {
      if (shape->clusters[cluster_idx].source_end == cursor.source_offset) {
        return nk_text_shape_cluster_edge(shape, cluster_idx, affinity);
      }
    }
  }

  for (cluster_idx = 0; cluster_idx < shape->num_clusters; ++cluster_idx) {
    const struct nk_text_cluster *cluster = &shape->clusters[cluster_idx];

    if (cluster->source_begin < cursor.source_offset && cursor.source_offset < cluster->source_end) {
      nk_bool leading = cursor.source_offset - cluster->source_begin <= cluster->source_end - cursor.source_offset;
      return nk_text_shape_cluster_edge(shape, cluster_idx, leading ? NK_TEXT_AFFINITY_LEADING : NK_TEXT_AFFINITY_TRAILING);
    }
  }
  return cursor;
}

NK_API struct nk_text_cursor
nk_text_shape_hit_test(const struct nk_text_shape *shape, float x)
{
  struct nk_text_cursor first = nk_text_shape_cursor_at(shape, 0, NK_TEXT_AFFINITY_LEADING);
  struct nk_text_cursor last  = nk_text_shape_cursor_at(shape, shape->source_len, NK_TEXT_AFFINITY_TRAILING);
  float                 pen   = 0.0f;
  int                   run_idx;

  if (x <= 0.0f) {
    return first.x <= last.x ? first : last;
  }

  if (x >= shape->advance.x) {
    return first.x >= last.x ? first : last;
  }

  for (run_idx = 0; run_idx < shape->num_runs; ++run_idx) {
    struct nk_text_run *run       = &shape->runs[run_idx];
    int                 glyph_idx = run->glyph_begin;

    while (glyph_idx < run->glyph_begin + run->glyph_count) {
      struct nk_text_glyph *glyph       = &shape->glyphs[glyph_idx];
      int                   cluster_idx = glyph->cluster_idx;
      float                 begin       = pen;

      do {
        pen       += shape->glyphs[glyph_idx].advance_x;
        glyph_idx += 1;
      } while (glyph_idx < run->glyph_begin + run->glyph_count && shape->glyphs[glyph_idx].cluster_idx == cluster_idx);

      if (x < (begin + pen) * 0.5f) {
        return nk_text_shape_cluster_edge(shape, cluster_idx, run->direction == NK_TEXT_DIRECTION_RTL ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING);
      }

      if (x <= pen) {
        return nk_text_shape_cluster_edge(shape, cluster_idx, run->direction == NK_TEXT_DIRECTION_RTL ? NK_TEXT_AFFINITY_LEADING : NK_TEXT_AFFINITY_TRAILING);
      }
    }
  }
  return last;
}

NK_API struct nk_text_cursor
nk_text_shape_cursor_move(const struct nk_text_shape *shape, struct nk_text_cursor cursor, int direction)
{
  struct nk_text_cursor current = nk_text_shape_cursor_at(shape, cursor.source_offset, cursor.affinity);
  struct nk_text_cursor best    = current;
  int                   cluster_idx;

  for (cluster_idx = 0; cluster_idx < shape->num_clusters; ++cluster_idx) {
    struct nk_text_cursor leading  = nk_text_shape_cluster_edge(shape, cluster_idx, NK_TEXT_AFFINITY_LEADING);
    struct nk_text_cursor trailing = nk_text_shape_cluster_edge(shape, cluster_idx, NK_TEXT_AFFINITY_TRAILING);
    struct nk_text_cursor edges[]  = {leading, trailing};
    int                   edge_idx;

    for (edge_idx = 0; edge_idx < 2; ++edge_idx) {
      struct nk_text_cursor edge = edges[edge_idx];

      if (direction < 0 && edge.x < current.x && (best.x == current.x || edge.x > best.x)) {
        best = edge;
      }

      if (direction > 0 && edge.x > current.x && (best.x == current.x || edge.x < best.x)) {
        best = edge;
      }
    }
  }
  return best;
}

NK_API void
nk_text_selection_iterator_begin(struct nk_text_selection_iterator *iterator, const struct nk_text_shape *shape, int source_begin, int source_end)
{
  NK_MEMSET(iterator, 0, sizeof(*iterator));
  iterator->shape        = shape;
  iterator->source_begin = NK_CLAMP(0, source_begin, shape->source_len);
  iterator->source_end   = NK_CLAMP(iterator->source_begin, source_end, shape->source_len);
  iterator->glyph_idx    = shape->num_runs ? shape->runs[0].glyph_begin : 0;
}

NK_API nk_bool
nk_text_selection_iterator_next(struct nk_text_selection_iterator *iterator, struct nk_text_selection_span *span)
{
  const struct nk_text_shape *shape  = iterator->shape;
  nk_bool                     inside = nk_false;

  if (iterator->has_pending) {
    span->x               = iterator->pending_x;
    span->width           = iterator->pending_width;
    iterator->has_pending = nk_false;
    inside                = nk_true;
  }

  while (iterator->run_idx < shape->num_runs) {
    struct nk_text_run *run = &shape->runs[iterator->run_idx];

    while (iterator->glyph_idx < run->glyph_begin + run->glyph_count) {
      int   cluster_idx    = shape->glyphs[iterator->glyph_idx].cluster_idx;
      int   group_end      = cluster_idx + 1;
      int   selected_begin = 0;
      int   selected_end   = 0;
      int   group_count    = 0;
      float group_left     = iterator->pen_x;
      float group_right    = 0;
      float selected_left  = 0;
      float selected_right = 0;

      do {
        iterator->pen_x     += shape->glyphs[iterator->glyph_idx].advance_x;
        iterator->glyph_idx += 1;
      } while (iterator->glyph_idx < run->glyph_begin + run->glyph_count && shape->glyphs[iterator->glyph_idx].cluster_idx == cluster_idx);

      group_right = iterator->pen_x;

      while (group_end < shape->num_clusters && !shape->clusters[group_end].glyph_count) {
        group_end += 1;
      }

      selected_begin = cluster_idx;

      while (selected_begin < group_end && shape->clusters[selected_begin].source_end <= iterator->source_begin) {
        selected_begin += 1;
      }

      selected_end = selected_begin;

      while (selected_end < group_end && shape->clusters[selected_end].source_begin < iterator->source_end) {
        selected_end += 1;
      }

      if (selected_begin == selected_end) {
        if (inside) {
          return nk_true;
        }
        continue;
      }

      group_count = group_end - cluster_idx;

      if (run->direction == NK_TEXT_DIRECTION_RTL) {
        selected_left  = group_right - (group_right - group_left) * (selected_end - cluster_idx) / group_count;
        selected_right = group_right - (group_right - group_left) * (selected_begin - cluster_idx) / group_count;
      } else {
        selected_left  = group_left + (group_right - group_left) * (selected_begin - cluster_idx) / group_count;
        selected_right = group_left + (group_right - group_left) * (selected_end - cluster_idx) / group_count;
      }

      if (!inside) {
        span->x     = selected_left;
        span->width = selected_right - selected_left;
        inside      = nk_true;
      } else if (selected_left == span->x + span->width) {
        span->width = selected_right - span->x;
      } else {
        iterator->pending_x     = selected_left;
        iterator->pending_width = selected_right - selected_left;
        iterator->has_pending   = nk_true;
        return nk_true;
      }
    }

    iterator->run_idx += 1;

    if (iterator->run_idx < shape->num_runs) {
      iterator->glyph_idx = shape->runs[iterator->run_idx].glyph_begin;
    }
  }

  if (inside) {
    return nk_true;
  }
  return nk_false;
}

NK_API void
nk_text_wrap_iterator_begin(struct nk_text_wrap_iterator *iterator, const struct nk_text_shape *shape, float max_width)
{
  NK_ASSERT(iterator);
  NK_MEMSET(iterator, 0, sizeof(*iterator));
  iterator->shape     = shape;
  iterator->max_width = NK_MAX(0.0f, max_width);
}

NK_INTERN float
nk_text_cluster_width(const struct nk_text_shape *shape, int cluster_idx)
{
  struct nk_text_cluster *cluster = &shape->clusters[cluster_idx];
  float                   width   = 0.0f;
  int                     glyph_idx;

  for (glyph_idx = cluster->glyph_begin; glyph_idx < cluster->glyph_begin + cluster->glyph_count; ++glyph_idx) {
    width += shape->glyphs[glyph_idx].advance_x;
  }
  return width;
}

NK_INTERN nk_flags
nk_text_boundary_flags_at(const struct nk_text_shape *shape, int *boundary_idx, int source_offset)
{
  nk_flags flags = 0;

  while (*boundary_idx < shape->num_boundaries && shape->boundaries[*boundary_idx].source_offset <= source_offset) {
    struct nk_text_boundary *boundary = &shape->boundaries[*boundary_idx];

    if (boundary->source_offset == source_offset) {
      flags |= boundary->flags;
    }

    *boundary_idx += 1;
  }
  return flags;
}

NK_INTERN nk_bool
nk_text_wrap_line_set(struct nk_text_wrap_iterator *iterator, struct nk_text_wrap_line *line,
                      int source_end, float width, nk_flags flags, int cluster_idx, int boundary_idx)
{
  line->source_begin      = iterator->source_offset;
  line->source_end        = source_end;
  line->width             = width;
  line->break_flags       = flags;
  iterator->source_offset = source_end;
  iterator->cluster_idx   = cluster_idx;
  iterator->boundary_idx  = boundary_idx;
  return nk_true;
}

NK_API nk_bool
nk_text_wrap_iterator_next(struct nk_text_wrap_iterator *iterator, struct nk_text_wrap_line *line)
{
  const struct nk_text_shape *shape;
  int                         line_cluster_begin;
  int                         cluster_idx;
  int                         boundary_idx;
  int                         soft_source_end   = 0;
  int                         soft_cluster_idx  = 0;
  int                         soft_boundary_idx = 0;
  float                       soft_width        = 0.0f;
  float                       width             = 0.0f;

  NK_ASSERT(iterator);
  NK_ASSERT(line);
  shape = iterator->shape;

  if (!shape || iterator->source_offset >= shape->source_len) {
    return nk_false;
  }

  line_cluster_begin = iterator->cluster_idx;
  cluster_idx        = iterator->cluster_idx;
  boundary_idx       = iterator->boundary_idx;

  while (boundary_idx < shape->num_boundaries && shape->boundaries[boundary_idx].source_offset <= iterator->source_offset) {
    boundary_idx += 1;
  }

  while (cluster_idx < shape->num_clusters) {
    const struct nk_text_cluster *cluster    = &shape->clusters[cluster_idx];
    float                         next_width = width + nk_text_cluster_width(shape, cluster_idx);
    nk_flags                      flags;

    if (next_width > iterator->max_width && cluster_idx > line_cluster_begin) {
      if (soft_source_end > iterator->source_offset) {
        return nk_text_wrap_line_set(iterator, line, soft_source_end, soft_width, NK_TEXT_BOUNDARY_LINE_SOFT, soft_cluster_idx, soft_boundary_idx);
      }
      return nk_text_wrap_line_set(iterator, line, cluster->source_begin, width, NK_TEXT_BOUNDARY_GRAPHEME, cluster_idx, boundary_idx);
    }

    width        = next_width;
    flags        = nk_text_boundary_flags_at(shape, &boundary_idx, cluster->source_end);
    cluster_idx += 1;

    if (flags & NK_TEXT_BOUNDARY_LINE_HARD) {
      return nk_text_wrap_line_set(iterator, line, cluster->source_end, width, flags, cluster_idx, boundary_idx);
    }

    if (flags & NK_TEXT_BOUNDARY_LINE_SOFT) {
      soft_source_end   = cluster->source_end;
      soft_cluster_idx  = cluster_idx;
      soft_boundary_idx = boundary_idx;
      soft_width        = width;
    }

    if (width > iterator->max_width && cluster_idx == line_cluster_begin + 1) {
      return nk_text_wrap_line_set(iterator, line, cluster->source_end, width, NK_TEXT_BOUNDARY_GRAPHEME, cluster_idx, boundary_idx);
    }
  }

  return nk_text_wrap_line_set(iterator, line, shape->source_len, width, 0, shape->num_clusters, shape->num_boundaries);
}

NK_API float
nk_text_width(struct nk_context *ctx, const struct nk_user_font *font, float height, const char *text, int len)
{
  struct nk_text_request      request;
  struct nk_text_cache_entry *entry;
  struct nk_text_shape       *shape;
  nk_hash                     hash;

  NK_ASSERT(ctx);

  if (!len) {
    return 0.0f;
  }

  NK_MEMSET(&request, 0, sizeof(request));
  request.source      = text;
  request.source_len  = len;
  request.font        = font;
  request.font_height = height;

  hash  = nk_text_request_hash(&request);
  entry = nk_text_cache_find(ctx, &request, hash);

  if (entry && entry->measured) {
    return entry->width;
  }

  shape = nk_text_shape_build_temporary(ctx, &request);

  if (!shape) {
    return 0.0f;
  }

  if (nk_text_cache_enabled(ctx) && !entry) {
    entry = nk_text_cache_write(ctx, &request, hash, NULL, nk_false, NULL);
  }

  if (entry) {
    entry->width    = shape->advance.x;
    entry->measured = nk_true;
  }
  return shape->advance.x;
}

/* ==============================================================
 *
 *                          COLOR
 *
 * ===============================================================*/
NK_INTERN int
nk_parse_hex(const char *p, int length)
{
  int i   = 0;
  int len = 0;

  while (len < length) {
    i <<= 4;

    if (p[len] >= 'a' && p[len] <= 'f') {
      i += ((p[len] - 'a') + 10);
    } else if (p[len] >= 'A' && p[len] <= 'F') {
      i += ((p[len] - 'A') + 10);
    } else {
      i += (p[len] - '0');
    }

    len++;
  }
  return i;
}

NK_API struct nk_color
nk_rgb_factor(struct nk_color col, float factor)
{
  if (factor == 1.0f) {
    return col;
  }

  col.r = (nk_byte)(col.r * factor);
  col.g = (nk_byte)(col.g * factor);
  col.b = (nk_byte)(col.b * factor);
  return col;
}

NK_API struct nk_color
nk_rgba(int r, int g, int b, int a)
{
  struct nk_color ret;

  ret.r = (nk_byte)NK_CLAMP(0, r, 255);
  ret.g = (nk_byte)NK_CLAMP(0, g, 255);
  ret.b = (nk_byte)NK_CLAMP(0, b, 255);
  ret.a = (nk_byte)NK_CLAMP(0, a, 255);
  return ret;
}

NK_API struct nk_color
nk_rgb_hex(const char *rgb)
{
  struct nk_color col;
  const char     *c = rgb;

  if (*c == '#') {
    c++;
  }

  col.r = (nk_byte)nk_parse_hex(c, 2);
  col.g = (nk_byte)nk_parse_hex(c + 2, 2);
  col.b = (nk_byte)nk_parse_hex(c + 4, 2);
  col.a = 255;
  return col;
}

NK_API struct nk_color
nk_rgba_hex(const char *rgb)
{
  struct nk_color col;
  const char     *c = rgb;

  if (*c == '#') {
    c++;
  }

  col.r = (nk_byte)nk_parse_hex(c, 2);
  col.g = (nk_byte)nk_parse_hex(c + 2, 2);
  col.b = (nk_byte)nk_parse_hex(c + 4, 2);
  col.a = (nk_byte)nk_parse_hex(c + 6, 2);
  return col;
}

NK_API void
nk_color_hex_rgba(char *output, struct nk_color col)
{
#define NK_TO_HEX(i) ((i) <= 9 ? '0' + (i) : 'A' - 10 + (i))
  output[0] = (char)NK_TO_HEX((col.r & 0xF0) >> 4);
  output[1] = (char)NK_TO_HEX((col.r & 0x0F));
  output[2] = (char)NK_TO_HEX((col.g & 0xF0) >> 4);
  output[3] = (char)NK_TO_HEX((col.g & 0x0F));
  output[4] = (char)NK_TO_HEX((col.b & 0xF0) >> 4);
  output[5] = (char)NK_TO_HEX((col.b & 0x0F));
  output[6] = (char)NK_TO_HEX((col.a & 0xF0) >> 4);
  output[7] = (char)NK_TO_HEX((col.a & 0x0F));
  output[8] = '\0';
#undef NK_TO_HEX
}

NK_API void
nk_color_hex_rgb(char *output, struct nk_color col)
{
#define NK_TO_HEX(i) ((i) <= 9 ? '0' + (i) : 'A' - 10 + (i))
  output[0] = (char)NK_TO_HEX((col.r & 0xF0) >> 4);
  output[1] = (char)NK_TO_HEX((col.r & 0x0F));
  output[2] = (char)NK_TO_HEX((col.g & 0xF0) >> 4);
  output[3] = (char)NK_TO_HEX((col.g & 0x0F));
  output[4] = (char)NK_TO_HEX((col.b & 0xF0) >> 4);
  output[5] = (char)NK_TO_HEX((col.b & 0x0F));
  output[6] = '\0';
#undef NK_TO_HEX
}

NK_API struct nk_color
nk_rgba_iv(const int *c)
{
  return nk_rgba(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_color
nk_rgba_bv(const nk_byte *c)
{
  return nk_rgba(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_color
nk_rgb(int r, int g, int b)
{
  struct nk_color ret;

  ret.r = (nk_byte)NK_CLAMP(0, r, 255);
  ret.g = (nk_byte)NK_CLAMP(0, g, 255);
  ret.b = (nk_byte)NK_CLAMP(0, b, 255);
  ret.a = (nk_byte)255;
  return ret;
}

NK_API struct nk_color
nk_rgb_iv(const int *c)
{
  return nk_rgb(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_rgb_bv(const nk_byte *c)
{
  return nk_rgb(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_rgba_u32(nk_uint in)
{
  struct nk_color ret;

  ret.r = (in & 0xFF);
  ret.g = ((in >> 8) & 0xFF);
  ret.b = ((in >> 16) & 0xFF);
  ret.a = (nk_byte)((in >> 24) & 0xFF);
  return ret;
}

NK_API struct nk_color
nk_rgba_f(float r, float g, float b, float a)
{
  struct nk_color ret;

  ret.r = (nk_byte)(NK_SATURATE(r) * 255.0f);
  ret.g = (nk_byte)(NK_SATURATE(g) * 255.0f);
  ret.b = (nk_byte)(NK_SATURATE(b) * 255.0f);
  ret.a = (nk_byte)(NK_SATURATE(a) * 255.0f);
  return ret;
}

NK_API struct nk_color
nk_rgba_fv(const float *c)
{
  return nk_rgba_f(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_color
nk_rgba_cf(struct nk_colorf c)
{
  return nk_rgba_f(c.r, c.g, c.b, c.a);
}

NK_API struct nk_color
nk_rgb_f(float r, float g, float b)
{
  struct nk_color ret;

  ret.r = (nk_byte)(NK_SATURATE(r) * 255.0f);
  ret.g = (nk_byte)(NK_SATURATE(g) * 255.0f);
  ret.b = (nk_byte)(NK_SATURATE(b) * 255.0f);
  ret.a = 255;
  return ret;
}

NK_API struct nk_color
nk_rgb_fv(const float *c)
{
  return nk_rgb_f(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_rgb_cf(struct nk_colorf c)
{
  return nk_rgb_f(c.r, c.g, c.b);
}

NK_API struct nk_color
nk_hsv(int h, int s, int v)
{
  return nk_hsva(h, s, v, 255);
}

NK_API struct nk_color
nk_hsv_iv(const int *c)
{
  return nk_hsv(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_hsv_bv(const nk_byte *c)
{
  return nk_hsv(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_hsv_f(float h, float s, float v)
{
  return nk_hsva_f(h, s, v, 1.0f);
}

NK_API struct nk_color
nk_hsv_fv(const float *c)
{
  return nk_hsv_f(c[0], c[1], c[2]);
}

NK_API struct nk_color
nk_hsva(int h, int s, int v, int a)
{
  float hf = ((float)NK_CLAMP(0, h, 255)) / 255.0f;
  float sf = ((float)NK_CLAMP(0, s, 255)) / 255.0f;
  float vf = ((float)NK_CLAMP(0, v, 255)) / 255.0f;
  float af = ((float)NK_CLAMP(0, a, 255)) / 255.0f;
  return nk_hsva_f(hf, sf, vf, af);
}

NK_API struct nk_color
nk_hsva_iv(const int *c)
{
  return nk_hsva(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_color
nk_hsva_bv(const nk_byte *c)
{
  return nk_hsva(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_colorf
nk_hsva_colorf(float h, float s, float v, float a)
{
  int              i;
  float            p, q, t, f;
  struct nk_colorf out = {0, 0, 0, 0};

  if (s <= 0.0f) {
    out.r = v;
    out.g = v;
    out.b = v;
    out.a = a;
    return out;
  }

  h = h / (60.0f / 360.0f);
  i = (int)h;
  f = h - (float)i;
  p = v * (1.0f - s);
  q = v * (1.0f - (s * f));
  t = v * (1.0f - s * (1.0f - f));

  switch (i) {
    case 0:
    default:
      out.r = v;
      out.g = t;
      out.b = p;
      break;

    case 1:
      out.r = q;
      out.g = v;
      out.b = p;
      break;

    case 2:
      out.r = p;
      out.g = v;
      out.b = t;
      break;

    case 3:
      out.r = p;
      out.g = q;
      out.b = v;
      break;

    case 4:
      out.r = t;
      out.g = p;
      out.b = v;
      break;

    case 5:
      out.r = v;
      out.g = p;
      out.b = q;
      break;
  }

  out.a = a;
  return out;
}

NK_API struct nk_colorf
nk_hsva_colorfv(const float *c)
{
  return nk_hsva_colorf(c[0], c[1], c[2], c[3]);
}

NK_API struct nk_color
nk_hsva_f(float h, float s, float v, float a)
{
  struct nk_colorf c = nk_hsva_colorf(h, s, v, a);
  return nk_rgba_f(c.r, c.g, c.b, c.a);
}

NK_API struct nk_color
nk_hsva_fv(const float *c)
{
  return nk_hsva_f(c[0], c[1], c[2], c[3]);
}

NK_API nk_uint
nk_color_u32(struct nk_color in)
{
  nk_uint out = (nk_uint)in.r;

  out |= ((nk_uint)in.g << 8);
  out |= ((nk_uint)in.b << 16);
  out |= ((nk_uint)in.a << 24);
  return out;
}

NK_API void
nk_color_f(float *r, float *g, float *b, float *a, struct nk_color in)
{
  NK_STORAGE const float s = 1.0f / 255.0f;

  *r = (float)in.r * s;
  *g = (float)in.g * s;
  *b = (float)in.b * s;
  *a = (float)in.a * s;
}

NK_API void
nk_color_fv(float *c, struct nk_color in)
{
  nk_color_f(&c[0], &c[1], &c[2], &c[3], in);
}

NK_API struct nk_colorf
nk_color_cf(struct nk_color in)
{
  struct nk_colorf o;

  nk_color_f(&o.r, &o.g, &o.b, &o.a, in);
  return o;
}

NK_API void
nk_color_d(double *r, double *g, double *b, double *a, struct nk_color in)
{
  NK_STORAGE const double s = 1.0 / 255.0;

  *r = (double)in.r * s;
  *g = (double)in.g * s;
  *b = (double)in.b * s;
  *a = (double)in.a * s;
}

NK_API void
nk_color_dv(double *c, struct nk_color in)
{
  nk_color_d(&c[0], &c[1], &c[2], &c[3], in);
}

NK_API void
nk_color_hsv_f(float *out_h, float *out_s, float *out_v, struct nk_color in)
{
  float a;

  nk_color_hsva_f(out_h, out_s, out_v, &a, in);
}

NK_API void
nk_color_hsv_fv(float *out, struct nk_color in)
{
  float a;

  nk_color_hsva_f(&out[0], &out[1], &out[2], &a, in);
}

NK_API void
nk_colorf_hsva_f(float *out_h, float *out_s, float *out_v, float *out_a, struct nk_colorf in)
{
  float chroma;
  float K = 0.0f;

  if (in.g < in.b) {
    const float t = in.g;

    in.g = in.b;
    in.b = t;
    K    = -1.f;
  }

  if (in.r < in.g) {
    const float t = in.r;

    in.r = in.g;
    in.g = t;
    K    = -2.f / 6.0f - K;
  }

  chroma = in.r - ((in.g < in.b) ? in.g : in.b);
  *out_h = NK_ABS(K + (in.g - in.b) / (6.0f * chroma + 1e-20f));
  *out_s = chroma / (in.r + 1e-20f);
  *out_v = in.r;
  *out_a = in.a;
}

NK_API void
nk_colorf_hsva_fv(float *hsva, struct nk_colorf in)
{
  nk_colorf_hsva_f(&hsva[0], &hsva[1], &hsva[2], &hsva[3], in);
}

NK_API void
nk_color_hsva_f(float *out_h, float *out_s, float *out_v, float *out_a, struct nk_color in)
{
  struct nk_colorf col;

  nk_color_f(&col.r, &col.g, &col.b, &col.a, in);
  nk_colorf_hsva_f(out_h, out_s, out_v, out_a, col);
}

NK_API void
nk_color_hsva_fv(float *out, struct nk_color in)
{
  nk_color_hsva_f(&out[0], &out[1], &out[2], &out[3], in);
}

NK_API void
nk_color_hsva_i(int *out_h, int *out_s, int *out_v, int *out_a, struct nk_color in)
{
  float h, s, v, a;

  nk_color_hsva_f(&h, &s, &v, &a, in);
  *out_h = (nk_byte)(h * 255.0f);
  *out_s = (nk_byte)(s * 255.0f);
  *out_v = (nk_byte)(v * 255.0f);
  *out_a = (nk_byte)(a * 255.0f);
}

NK_API void
nk_color_hsva_iv(int *out, struct nk_color in)
{
  nk_color_hsva_i(&out[0], &out[1], &out[2], &out[3], in);
}

NK_API void
nk_color_hsva_bv(nk_byte *out, struct nk_color in)
{
  int tmp[4];

  nk_color_hsva_i(&tmp[0], &tmp[1], &tmp[2], &tmp[3], in);
  out[0] = (nk_byte)tmp[0];
  out[1] = (nk_byte)tmp[1];
  out[2] = (nk_byte)tmp[2];
  out[3] = (nk_byte)tmp[3];
}

NK_API void
nk_color_hsva_b(nk_byte *h, nk_byte *s, nk_byte *v, nk_byte *a, struct nk_color in)
{
  int tmp[4];

  nk_color_hsva_i(&tmp[0], &tmp[1], &tmp[2], &tmp[3], in);
  *h = (nk_byte)tmp[0];
  *s = (nk_byte)tmp[1];
  *v = (nk_byte)tmp[2];
  *a = (nk_byte)tmp[3];
}

NK_API void
nk_color_hsv_i(int *out_h, int *out_s, int *out_v, struct nk_color in)
{
  int a;

  nk_color_hsva_i(out_h, out_s, out_v, &a, in);
}

NK_API void
nk_color_hsv_b(nk_byte *out_h, nk_byte *out_s, nk_byte *out_v, struct nk_color in)
{
  int tmp[4];

  nk_color_hsva_i(&tmp[0], &tmp[1], &tmp[2], &tmp[3], in);
  *out_h = (nk_byte)tmp[0];
  *out_s = (nk_byte)tmp[1];
  *out_v = (nk_byte)tmp[2];
}

NK_API void
nk_color_hsv_iv(int *out, struct nk_color in)
{
  nk_color_hsv_i(&out[0], &out[1], &out[2], in);
}

NK_API void
nk_color_hsv_bv(nk_byte *out, struct nk_color in)
{
  int tmp[4];

  nk_color_hsv_i(&tmp[0], &tmp[1], &tmp[2], in);
  out[0] = (nk_byte)tmp[0];
  out[1] = (nk_byte)tmp[1];
  out[2] = (nk_byte)tmp[2];
}

/* ===============================================================
 *
 *                              UTF-8
 *
 * ===============================================================*/
NK_GLOBAL const nk_byte nk_utfbyte[NK_UTF_SIZE + 1] = {0x80, 0, 0xC0, 0xE0, 0xF0};

NK_GLOBAL const nk_byte nk_utfmask[NK_UTF_SIZE + 1] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};

NK_GLOBAL const nk_uint nk_utfmin[NK_UTF_SIZE + 1]  = {0, 0, 0x80, 0x800, 0x10000};

NK_GLOBAL const nk_uint nk_utfmax[NK_UTF_SIZE + 1]  = {0x10FFFF, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};

NK_INTERN int
nk_utf_validate(nk_rune *u, int i)
{
  NK_ASSERT(u != NULL);

  if (!u) {
    return 0;
  }

  if (!NK_BETWEEN(*u, nk_utfmin[i], nk_utfmax[i]) || NK_BETWEEN(*u, 0xD800, 0xDFFF)) {
    *u = NK_UTF_INVALID;
  }

  for (i = 1; *u > nk_utfmax[i]; ++i)
    ;
  return i;
}

NK_INTERN nk_rune
nk_utf_decode_byte(char c, int *i)
{
  NK_ASSERT(i != NULL);

  if (!i) {
    return 0;
  }

  for (*i = 0; *i < (int)NK_LEN(nk_utfmask); ++(*i)) {
    if (((nk_byte)c & nk_utfmask[*i]) == nk_utfbyte[*i]) {
      return (nk_byte)(c & ~nk_utfmask[*i]);
    }
  }
  return 0;
}

NK_API int
nk_utf_decode(const char *c, nk_rune *u, int clen)
{
  int     i, j, len, type = 0;
  nk_rune udecoded;

  NK_ASSERT(c != NULL);
  NK_ASSERT(u != NULL);

  if (!c || !u) {
    return 0;
  }

  if (!clen) {
    return 0;
  }

  *u = NK_UTF_INVALID;

  udecoded = nk_utf_decode_byte(c[0], &len);

  if (!NK_BETWEEN(len, 1, NK_UTF_SIZE + 1)) { /* +1 because NK_BETWEEN uses strict upper bound ((a) <= (x) && (x) < (b)) */
    return 1;
  }

  for (i = 1, j = 1; i < clen && j < len; ++i, ++j) {
    udecoded = (udecoded << 6) | nk_utf_decode_byte(c[i], &type);

    if (type != 0) {
      return j;
    }
  }

  if (j < len) {
    return 0;
  }

  *u = udecoded;
  nk_utf_validate(u, len);
  return len;
}

NK_INTERN char
nk_utf_encode_byte(nk_rune u, int i)
{
  return (char)((nk_utfbyte[i]) | ((nk_byte)u & ~nk_utfmask[i]));
}

NK_API int
nk_utf_encode(nk_rune u, char *c, int clen)
{
  int len, i;

  len = nk_utf_validate(&u, 0);

  if (clen < len || !len || len > NK_UTF_SIZE) {
    return 0;
  }

  for (i = len - 1; i != 0; --i) {
    c[i] = nk_utf_encode_byte(u, 0);
    u  >>= 6;
  }

  c[0] = nk_utf_encode_byte(u, len);
  return len;
}

NK_API int
nk_utf_len(const char *str, int len)
{
  const char *text;
  int         glyphs = 0;
  int         text_len;
  int         glyph_len;
  int         src_len = 0;
  nk_rune     unicode;

  NK_ASSERT(str);

  if (!str || !len) {
    return 0;
  }

  text      = str;
  text_len  = len;
  glyph_len = nk_utf_decode(text, &unicode, text_len);

  while (glyph_len && src_len < len) {
    glyphs++;
    src_len   = src_len + glyph_len;
    glyph_len = nk_utf_decode(text + src_len, &unicode, text_len - src_len);
  }
  return glyphs;
}

NK_API const char *
nk_utf_at(const char *buffer, int length, int index, nk_rune *unicode, int *len)
{
  int         i         = 0;
  int         src_len   = 0;
  int         glyph_len = 0;
  const char *text;
  int         text_len;

  NK_ASSERT(buffer);
  NK_ASSERT(unicode);
  NK_ASSERT(len);

  if (!buffer || !unicode || !len) {
    return 0;
  }

  if (index < 0) {
    *unicode = NK_UTF_INVALID;
    *len     = 0;
    return 0;
  }

  text      = buffer;
  text_len  = length;
  glyph_len = nk_utf_decode(text, unicode, text_len);

  while (glyph_len) {
    if (i == index) {
      *len = glyph_len;
      break;
    }

    i++;
    src_len   = src_len + glyph_len;
    glyph_len = nk_utf_decode(text + src_len, unicode, text_len - src_len);
  }

  if (i != index) {
    return 0;
  }
  return buffer + src_len;
}

/* ==============================================================
 *
 *                          BUFFER
 *
 * ===============================================================*/
#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
NK_LIB void *
nk_malloc(nk_handle unused, void *old, nk_size size)
{
  NK_UNUSED(unused);
  NK_UNUSED(old);
  return malloc(size);
}

NK_LIB void
nk_mfree(nk_handle unused, void *ptr)
{
  NK_UNUSED(unused);
  free(ptr);
}

NK_API void
nk_buffer_init_default(struct nk_buffer *buffer)
{
  struct nk_allocator alloc;

  alloc.userdata.ptr = 0;
  alloc.alloc        = nk_malloc;
  alloc.free         = nk_mfree;
  nk_buffer_init(buffer, &alloc, NK_BUFFER_DEFAULT_INITIAL_SIZE);
}
#endif

NK_API void
nk_buffer_init(struct nk_buffer *b, const struct nk_allocator *a, nk_size initial_size)
{
  NK_ASSERT(b);
  NK_ASSERT(a);
  NK_ASSERT(initial_size);

  if (!b || !a || !initial_size) {
    return;
  }

  nk_zero(b, sizeof(*b));
  b->type        = NK_BUFFER_DYNAMIC;
  b->memory.ptr  = a->alloc(a->userdata, 0, initial_size);
  b->memory.size = initial_size;
  b->size        = initial_size;
  b->grow_factor = 2.0f;
  b->pool        = *a;
}

NK_API void
nk_buffer_init_fixed(struct nk_buffer *b, void *m, nk_size size)
{
  NK_ASSERT(b);
  NK_ASSERT(m);
  NK_ASSERT(size);

  if (!b || !m || !size) {
    return;
  }

  nk_zero(b, sizeof(*b));
  b->type        = NK_BUFFER_FIXED;
  b->memory.ptr  = m;
  b->memory.size = size;
  b->size        = size;
}

NK_LIB void *
nk_buffer_align(void *unaligned, nk_size align, nk_size *alignment, enum nk_buffer_allocation_type type)
{
  void *memory = 0;

  switch (type) {
    default:
    case NK_BUFFER_MAX:
    case NK_BUFFER_FRONT:
      if (align) {
        memory     = NK_ALIGN_PTR(unaligned, align);
        *alignment = (nk_size)((nk_byte *)memory - (nk_byte *)unaligned);
      } else {
        memory     = unaligned;
        *alignment = 0;
      }
      break;

    case NK_BUFFER_BACK:
      if (align) {
        memory     = NK_ALIGN_PTR_BACK(unaligned, align);
        *alignment = (nk_size)((nk_byte *)unaligned - (nk_byte *)memory);
      } else {
        memory     = unaligned;
        *alignment = 0;
      }
      break;
  }
  return memory;
}

NK_LIB void *
nk_buffer_realloc(struct nk_buffer *b, nk_size capacity, nk_size *size)
{
  void   *temp;
  nk_size buffer_size;

  NK_ASSERT(b);
  NK_ASSERT(size);

  if (!b || !size || !b->pool.alloc || !b->pool.free) {
    return 0;
  }

  buffer_size = b->memory.size;
  temp        = b->pool.alloc(b->pool.userdata, b->memory.ptr, capacity);
  NK_ASSERT(temp);

  if (!temp) {
    return 0;
  }

  *size = capacity;

  if (temp != b->memory.ptr) {
    NK_MEMCPY(temp, b->memory.ptr, buffer_size);
    b->pool.free(b->pool.userdata, b->memory.ptr);
  }

  if (b->size == buffer_size) {
    /* no back buffer so just set correct size */
    b->size = capacity;
    return temp;
  } else {
    /* copy back buffer to the end of the new buffer */
    void   *dst, *src;
    nk_size back_size;

    back_size = buffer_size - b->size;
    dst       = nk_ptr_add(void, temp, capacity - back_size);
    src       = nk_ptr_add(void, temp, b->size);
    NK_MEMMOVE(dst, src, back_size);
    b->size = capacity - back_size;
  }
  return temp;
}

NK_LIB void *
nk_buffer_alloc(struct nk_buffer *b, enum nk_buffer_allocation_type type, nk_size size, nk_size align)
{
  int     full;
  nk_size alignment;
  void   *unaligned;
  void   *memory;

  NK_ASSERT(b);
  NK_ASSERT(size);

  if (!b || !size) {
    return 0;
  }

  b->needed += size;

  /* calculate total size with needed alignment + size */
  if (type == NK_BUFFER_FRONT) {
    unaligned = nk_ptr_add(void, b->memory.ptr, b->allocated);
  } else {
    unaligned = nk_ptr_add(void, b->memory.ptr, b->size - size);
  }

  memory = nk_buffer_align(unaligned, align, &alignment, type);

  /* check if buffer has enough memory*/
  if (type == NK_BUFFER_FRONT) {
    full = ((b->allocated + size + alignment) > b->size);
  } else {
    full = ((b->size - NK_MIN(b->size, (size + alignment))) <= b->allocated);
  }

  if (full) {
    nk_size capacity;

    if (b->type != NK_BUFFER_DYNAMIC) {
      return 0;
    }

    NK_ASSERT(b->pool.alloc && b->pool.free);

    if (b->type != NK_BUFFER_DYNAMIC || !b->pool.alloc || !b->pool.free) {
      return 0;
    }

    /* buffer is full so allocate bigger buffer if dynamic */
    capacity      = (nk_size)((float)b->memory.size * b->grow_factor);
    capacity      = NK_MAX(capacity, nk_round_up_pow2((nk_uint)(b->allocated + size)));
    b->memory.ptr = nk_buffer_realloc(b, capacity, &b->memory.size);

    if (!b->memory.ptr) {
      return 0;
    }

    /* align newly allocated pointer */
    if (type == NK_BUFFER_FRONT) {
      unaligned = nk_ptr_add(void, b->memory.ptr, b->allocated);
    } else {
      unaligned = nk_ptr_add(void, b->memory.ptr, b->size - size);
    }

    memory = nk_buffer_align(unaligned, align, &alignment, type);
  }

  if (type == NK_BUFFER_FRONT) {
    b->allocated += size + alignment;
  } else {
    b->size -= (size + alignment);
  }

  b->needed += alignment;
  b->calls++;
  return memory;
}

NK_API void
nk_buffer_push(struct nk_buffer *b, enum nk_buffer_allocation_type type, const void *memory, nk_size size, nk_size align)
{
  void *mem = nk_buffer_alloc(b, type, size, align);

  if (!mem) {
    return;
  }

  NK_MEMCPY(mem, memory, size);
}

NK_API void
nk_buffer_mark(struct nk_buffer *buffer, enum nk_buffer_allocation_type type)
{
  NK_ASSERT(buffer);

  if (!buffer) {
    return;
  }

  buffer->marker[type].active = nk_true;

  if (type == NK_BUFFER_BACK) {
    buffer->marker[type].offset = buffer->size;
  } else {
    buffer->marker[type].offset = buffer->allocated;
  }
}

NK_API void
nk_buffer_reset(struct nk_buffer *buffer, enum nk_buffer_allocation_type type)
{
  NK_ASSERT(buffer);

  if (!buffer) {
    return;
  }

  if (type == NK_BUFFER_BACK) {
    /* reset back buffer either back to marker or empty */
    buffer->needed -= (buffer->memory.size - buffer->marker[type].offset);

    if (buffer->marker[type].active) {
      buffer->size = buffer->marker[type].offset;
    } else {
      buffer->size = buffer->memory.size;
    }

    buffer->marker[type].active = nk_false;
  } else {
    /* reset front buffer either back to back marker or empty */
    buffer->needed -= (buffer->allocated - buffer->marker[type].offset);

    if (buffer->marker[type].active) {
      buffer->allocated = buffer->marker[type].offset;
    } else {
      buffer->allocated = 0;
    }

    buffer->marker[type].active = nk_false;
  }
}

NK_API void
nk_buffer_clear(struct nk_buffer *b)
{
  NK_ASSERT(b);

  if (!b) {
    return;
  }

  b->allocated = 0;
  b->size      = b->memory.size;
  b->calls     = 0;
  b->needed    = 0;
}

NK_API void
nk_buffer_free(struct nk_buffer *b)
{
  NK_ASSERT(b);

  if (!b || !b->memory.ptr) {
    return;
  }

  if (b->type == NK_BUFFER_FIXED) {
    return;
  }

  if (!b->pool.free) {
    return;
  }

  NK_ASSERT(b->pool.free);
  b->pool.free(b->pool.userdata, b->memory.ptr);
}

NK_API void
nk_buffer_info(struct nk_memory_status *s, const struct nk_buffer *b)
{
  NK_ASSERT(b);
  NK_ASSERT(s);

  if (!s || !b) {
    return;
  }

  s->allocated = b->allocated;
  s->size      = b->memory.size;
  s->needed    = b->needed;
  s->memory    = b->memory.ptr;
  s->calls     = b->calls;
}

NK_API void *
nk_buffer_memory(struct nk_buffer *buffer)
{
  NK_ASSERT(buffer);

  if (!buffer) {
    return 0;
  }
  return buffer->memory.ptr;
}

NK_API const void *
nk_buffer_memory_const(const struct nk_buffer *buffer)
{
  NK_ASSERT(buffer);

  if (!buffer) {
    return 0;
  }
  return buffer->memory.ptr;
}

NK_API nk_size
nk_buffer_total(const struct nk_buffer *buffer)
{
  NK_ASSERT(buffer);

  if (!buffer) {
    return 0;
  }
  return buffer->memory.size;
}

/* ===============================================================
 *
 *                              STRING
 *
 * ===============================================================*/
#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
NK_API void
nk_str_init_default(struct nk_str *str)
{
  struct nk_allocator alloc;

  alloc.userdata.ptr = 0;
  alloc.alloc        = nk_malloc;
  alloc.free         = nk_mfree;
  nk_buffer_init(&str->buffer, &alloc, 32);
  str->len = 0;
}
#endif

NK_API void
nk_str_init(struct nk_str *str, const struct nk_allocator *alloc, nk_size size)
{
  nk_buffer_init(&str->buffer, alloc, size);
  str->len = 0;
}

NK_API void
nk_str_init_fixed(struct nk_str *str, void *memory, nk_size size)
{
  nk_buffer_init_fixed(&str->buffer, memory, size);
  str->len = 0;
}

NK_API int
nk_str_append_text_char(struct nk_str *s, const char *str, int len)
{
  char *mem;

  NK_ASSERT(s);
  NK_ASSERT(str);

  if (!s || !str || !len) {
    return 0;
  }

  mem = (char *)nk_buffer_alloc(&s->buffer, NK_BUFFER_FRONT, (nk_size)len * sizeof(char), 0);

  if (!mem) {
    return 0;
  }

  NK_MEMCPY(mem, str, (nk_size)len * sizeof(char));
  s->len += nk_utf_len(str, len);
  return len;
}

NK_API int
nk_str_append_str_char(struct nk_str *s, const char *str)
{
  return nk_str_append_text_char(s, str, nk_strlen(str));
}

NK_API int
nk_str_append_text_utf8(struct nk_str *str, const char *text, int len)
{
  int     i        = 0;
  int     byte_len = 0;
  nk_rune unicode;

  if (!str || !text || !len) {
    return 0;
  }

  for (i = 0; i < len; ++i) {
    byte_len += nk_utf_decode(text + byte_len, &unicode, 4);
  }

  nk_str_append_text_char(str, text, byte_len);
  return len;
}

NK_API int
nk_str_append_str_utf8(struct nk_str *str, const char *text)
{
  int     byte_len  = 0;
  int     num_runes = 0;
  int     glyph_len = 0;
  nk_rune unicode;

  if (!str || !text) {
    return 0;
  }

  glyph_len = byte_len = nk_utf_decode(text + byte_len, &unicode, 4);

  while (unicode != '\0' && glyph_len) {
    glyph_len = nk_utf_decode(text + byte_len, &unicode, 4);
    byte_len += glyph_len;
    num_runes++;
  }

  nk_str_append_text_char(str, text, byte_len);
  return num_runes;
}

NK_API int
nk_str_append_text_runes(struct nk_str *str, const nk_rune *text, int len)
{
  int      i        = 0;
  int      byte_len = 0;
  nk_glyph glyph;

  NK_ASSERT(str);

  if (!str || !text || !len) {
    return 0;
  }

  for (i = 0; i < len; ++i) {
    byte_len = nk_utf_encode(text[i], glyph, NK_UTF_SIZE);

    if (!byte_len) {
      break;
    }

    nk_str_append_text_char(str, glyph, byte_len);
  }
  return len;
}

NK_API int
nk_str_append_str_runes(struct nk_str *str, const nk_rune *runes)
{
  int      i = 0;
  nk_glyph glyph;
  int      byte_len;

  NK_ASSERT(str);

  if (!str || !runes) {
    return 0;
  }

  while (runes[i] != '\0') {
    byte_len = nk_utf_encode(runes[i], glyph, NK_UTF_SIZE);
    nk_str_append_text_char(str, glyph, byte_len);
    i++;
  }
  return i;
}

NK_API int
nk_str_insert_at_char(struct nk_str *s, int pos, const char *str, int len)
{
  int   i;
  void *mem;
  char *src;
  char *dst;

  int copylen;

  NK_ASSERT(s);
  NK_ASSERT(str);
  NK_ASSERT(len >= 0);

  if (!s || !str || !len || (nk_size)pos > s->buffer.allocated) {
    return 0;
  }

  if ((s->buffer.allocated + (nk_size)len >= s->buffer.memory.size) && (s->buffer.type == NK_BUFFER_FIXED)) {
    return 0;
  }

  copylen = (int)s->buffer.allocated - pos;

  if (!copylen) {
    nk_str_append_text_char(s, str, len);
    return 1;
  }

  mem = nk_buffer_alloc(&s->buffer, NK_BUFFER_FRONT, (nk_size)len * sizeof(char), 0);

  if (!mem) {
    return 0;
  }

  /* memmove */
  NK_ASSERT(((int)pos + (int)len + ((int)copylen - 1)) >= 0);
  NK_ASSERT(((int)pos + ((int)copylen - 1)) >= 0);
  dst = nk_ptr_add(char, s->buffer.memory.ptr, pos + len + (copylen - 1));
  src = nk_ptr_add(char, s->buffer.memory.ptr, pos + (copylen - 1));

  for (i = 0; i < copylen; ++i) {
    *dst-- = *src--;
  }

  mem = nk_ptr_add(void, s->buffer.memory.ptr, pos);
  NK_MEMCPY(mem, str, (nk_size)len * sizeof(char));
  s->len = nk_utf_len((char *)s->buffer.memory.ptr, (int)s->buffer.allocated);
  return 1;
}

NK_API int
nk_str_insert_at_rune(struct nk_str *str, int pos, const char *cstr, int len)
{
  int         glyph_len;
  nk_rune     unicode;
  const char *begin;
  const char *buffer;

  NK_ASSERT(str);
  NK_ASSERT(cstr);
  NK_ASSERT(len);

  if (!str || !cstr || !len) {
    return 0;
  }

  begin = nk_str_at_rune(str, pos, &unicode, &glyph_len);

  if (!str->len) {
    return nk_str_append_text_char(str, cstr, len);
  }

  buffer = nk_str_get_const(str);

  if (!begin) {
    return 0;
  }
  return nk_str_insert_at_char(str, (int)(begin - buffer), cstr, len);
}

NK_API int
nk_str_insert_text_char(struct nk_str *str, int pos, const char *text, int len)
{
  return nk_str_insert_at_rune(str, pos, text, len) ? len : 0;
}

NK_API int
nk_str_insert_str_char(struct nk_str *str, int pos, const char *text)
{
  return nk_str_insert_text_char(str, pos, text, nk_strlen(text));
}

NK_API int
nk_str_insert_text_utf8(struct nk_str *str, int pos, const char *text, int len)
{
  int     i        = 0;
  int     byte_len = 0;
  nk_rune unicode;

  NK_ASSERT(str);
  NK_ASSERT(text);

  if (!str || !text || !len) {
    return 0;
  }

  for (i = 0; i < len; ++i) {
    byte_len += nk_utf_decode(text + byte_len, &unicode, 4);
  }

  nk_str_insert_at_rune(str, pos, text, byte_len);
  return len;
}

NK_API int
nk_str_insert_str_utf8(struct nk_str *str, int pos, const char *text)
{
  int     byte_len  = 0;
  int     num_runes = 0;
  int     glyph_len = 0;
  nk_rune unicode;

  if (!str || !text) {
    return 0;
  }

  glyph_len = byte_len = nk_utf_decode(text + byte_len, &unicode, 4);

  while (unicode != '\0' && glyph_len) {
    glyph_len = nk_utf_decode(text + byte_len, &unicode, 4);
    byte_len += glyph_len;
    num_runes++;
  }

  nk_str_insert_at_rune(str, pos, text, byte_len);
  return num_runes;
}

NK_API int
nk_str_insert_text_runes(struct nk_str *str, int pos, const nk_rune *runes, int len)
{
  int      i        = 0;
  int      byte_len = 0;
  nk_glyph glyph;

  NK_ASSERT(str);

  if (!str || !runes || !len) {
    return 0;
  }

  for (i = 0; i < len; ++i) {
    byte_len = nk_utf_encode(runes[i], glyph, NK_UTF_SIZE);

    if (!byte_len) {
      break;
    }

    nk_str_insert_at_rune(str, pos + i, glyph, byte_len);
  }
  return len;
}

NK_API int
nk_str_insert_str_runes(struct nk_str *str, int pos, const nk_rune *runes)
{
  int      i = 0;
  nk_glyph glyph;
  int      byte_len;

  NK_ASSERT(str);

  if (!str || !runes) {
    return 0;
  }

  while (runes[i] != '\0') {
    byte_len = nk_utf_encode(runes[i], glyph, NK_UTF_SIZE);
    nk_str_insert_at_rune(str, pos + i, glyph, byte_len);
    i++;
  }
  return i;
}

NK_API void
nk_str_remove_chars(struct nk_str *s, int len)
{
  NK_ASSERT(s);
  NK_ASSERT(len >= 0);

  if (!s || len < 0 || (nk_size)len > s->buffer.allocated) {
    return;
  }

  NK_ASSERT(((int)s->buffer.allocated - (int)len) >= 0);
  s->buffer.allocated -= (nk_size)len;
  s->len               = nk_utf_len((char *)s->buffer.memory.ptr, (int)s->buffer.allocated);
}

NK_API void
nk_str_remove_runes(struct nk_str *str, int len)
{
  int         index;
  const char *begin;
  const char *end;
  nk_rune     unicode;

  NK_ASSERT(str);
  NK_ASSERT(len >= 0);

  if (!str || len < 0) {
    return;
  }

  if (len >= str->len) {
    str->len = 0;
    return;
  }

  index = str->len - len;
  begin = nk_str_at_rune(str, index, &unicode, &len);
  end   = (const char *)str->buffer.memory.ptr + str->buffer.allocated;
  nk_str_remove_chars(str, (int)(end - begin) + 1);
}

NK_API void
nk_str_delete_chars(struct nk_str *s, int pos, int len)
{
  NK_ASSERT(s);

  if (!s || !len || (nk_size)pos > s->buffer.allocated || (nk_size)(pos + len) > s->buffer.allocated) {
    return;
  }

  if ((nk_size)(pos + len) < s->buffer.allocated) {
    char *dst = nk_ptr_add(char, s->buffer.memory.ptr, pos);
    char *src = nk_ptr_add(char, s->buffer.memory.ptr, pos + len);

    NK_MEMMOVE(dst, src, s->buffer.allocated - (nk_size)(pos + len));
    NK_ASSERT(((int)s->buffer.allocated - (int)len) >= 0);
    s->buffer.allocated -= (nk_size)len;
  } else {
    nk_str_remove_chars(s, len);
  }

  s->len = nk_utf_len((char *)s->buffer.memory.ptr, (int)s->buffer.allocated);
}

NK_API void
nk_str_delete_runes(struct nk_str *s, int pos, int len)
{
  char   *temp;
  nk_rune unicode;
  char   *begin;
  char   *end;
  int     unused;

  NK_ASSERT(s);
  NK_ASSERT(s->len >= pos + len);

  if (s->len < pos + len) {
    len = NK_CLAMP(0, (s->len - pos), s->len);
  }

  if (!len) {
    return;
  }

  temp  = (char *)s->buffer.memory.ptr;
  begin = nk_str_at_rune(s, pos, &unicode, &unused);

  if (!begin) {
    return;
  }

  s->buffer.memory.ptr = begin;
  end                  = nk_str_at_rune(s, len, &unicode, &unused);
  s->buffer.memory.ptr = temp;

  if (!end) {
    return;
  }

  nk_str_delete_chars(s, (int)(begin - temp), (int)(end - begin));
}

NK_API char *
nk_str_at_char(struct nk_str *s, int pos)
{
  NK_ASSERT(s);

  if (!s || pos > (int)s->buffer.allocated) {
    return 0;
  }
  return nk_ptr_add(char, s->buffer.memory.ptr, pos);
}

NK_API char *
nk_str_at_rune(struct nk_str *str, int pos, nk_rune *unicode, int *len)
{
  int   i         = 0;
  int   src_len   = 0;
  int   glyph_len = 0;
  char *text;
  int   text_len;

  NK_ASSERT(str);
  NK_ASSERT(unicode);
  NK_ASSERT(len);

  if (!str || !unicode || !len) {
    return 0;
  }

  if (pos < 0) {
    *unicode = 0;
    *len     = 0;
    return 0;
  }

  text      = (char *)str->buffer.memory.ptr;
  text_len  = (int)str->buffer.allocated;
  glyph_len = nk_utf_decode(text, unicode, text_len);

  while (glyph_len) {
    if (i == pos) {
      *len = glyph_len;
      break;
    }

    i++;
    src_len   = src_len + glyph_len;
    glyph_len = nk_utf_decode(text + src_len, unicode, text_len - src_len);
  }

  if (i != pos) {
    return 0;
  }
  return text + src_len;
}

NK_API const char *
nk_str_at_char_const(const struct nk_str *s, int pos)
{
  NK_ASSERT(s);

  if (!s || pos > (int)s->buffer.allocated) {
    return 0;
  }
  return nk_ptr_add(char, s->buffer.memory.ptr, pos);
}

NK_API const char *
nk_str_at_const(const struct nk_str *str, int pos, nk_rune *unicode, int *len)
{
  int   i         = 0;
  int   src_len   = 0;
  int   glyph_len = 0;
  char *text;
  int   text_len;

  NK_ASSERT(str);
  NK_ASSERT(unicode);
  NK_ASSERT(len);

  if (!str || !unicode || !len) {
    return 0;
  }

  if (pos < 0) {
    *unicode = 0;
    *len     = 0;
    return 0;
  }

  text      = (char *)str->buffer.memory.ptr;
  text_len  = (int)str->buffer.allocated;
  glyph_len = nk_utf_decode(text, unicode, text_len);

  while (glyph_len) {
    if (i == pos) {
      *len = glyph_len;
      break;
    }

    i++;
    src_len   = src_len + glyph_len;
    glyph_len = nk_utf_decode(text + src_len, unicode, text_len - src_len);
  }

  if (i != pos) {
    return 0;
  }
  return text + src_len;
}

NK_API nk_rune
nk_str_rune_at(const struct nk_str *str, int pos)
{
  int     len;
  nk_rune unicode = 0;

  nk_str_at_const(str, pos, &unicode, &len);
  return unicode;
}

NK_API char *
nk_str_get(struct nk_str *s)
{
  NK_ASSERT(s);

  if (!s || !s->len || !s->buffer.allocated) {
    return 0;
  }
  return (char *)s->buffer.memory.ptr;
}

NK_API const char *
nk_str_get_const(const struct nk_str *s)
{
  NK_ASSERT(s);

  if (!s || !s->len || !s->buffer.allocated) {
    return 0;
  }
  return (const char *)s->buffer.memory.ptr;
}

NK_API int
nk_str_len(const struct nk_str *s)
{
  NK_ASSERT(s);

  if (!s || !s->len || !s->buffer.allocated) {
    return 0;
  }
  return s->len;
}

NK_API int
nk_str_len_char(const struct nk_str *s)
{
  NK_ASSERT(s);

  if (!s || !s->len || !s->buffer.allocated) {
    return 0;
  }
  return (int)s->buffer.allocated;
}

NK_API void
nk_str_clear(struct nk_str *str)
{
  NK_ASSERT(str);
  nk_buffer_clear(&str->buffer);
  str->len = 0;
}

NK_API void
nk_str_free(struct nk_str *str)
{
  NK_ASSERT(str);
  nk_buffer_free(&str->buffer);
  str->len = 0;
}

/* ==============================================================
 *
 *                          DRAW
 *
 * ===============================================================*/
NK_LIB void
nk_command_buffer_init(struct nk_command_buffer *cb, struct nk_buffer *b, enum nk_command_clipping clip)
{
  NK_ASSERT(cb);
  NK_ASSERT(b);

  if (!cb || !b) {
    return;
  }

  cb->base         = b;
  cb->context      = NULL;
  cb->use_clipping = (int)clip;
  cb->begin        = b->allocated;
  cb->end          = b->allocated;
  cb->last         = b->allocated;
}

NK_LIB void
nk_command_buffer_reset(struct nk_command_buffer *b)
{
  NK_ASSERT(b);

  if (!b) {
    return;
  }

  b->begin = 0;
  b->end   = 0;
  b->last  = 0;
  b->clip  = nk_null_rect;
#ifdef NK_INCLUDE_COMMAND_USERDATA
  b->userdata.ptr = 0;
#endif
}

NK_LIB void *
nk_command_buffer_push(struct nk_command_buffer *b, enum nk_command_type t, nk_size size)
{
  NK_STORAGE const nk_size align = NK_ALIGNOF(struct nk_command);
  struct nk_command       *cmd;
  nk_size                  alignment;
  void                    *unaligned;
  void                    *memory;

  NK_ASSERT(b);
  NK_ASSERT(b->base);

  if (!b) {
    return 0;
  }

  cmd = (struct nk_command *)nk_buffer_alloc(b->base, NK_BUFFER_FRONT, size, align);

  if (!cmd) {
    return 0;
  }

  /* make sure the offset to the next command is aligned */
  b->last   = (nk_size)((nk_byte *)cmd - (nk_byte *)b->base->memory.ptr);
  unaligned = (nk_byte *)cmd + size;
  memory    = NK_ALIGN_PTR(unaligned, align);
  alignment = (nk_size)((nk_byte *)memory - (nk_byte *)unaligned);
#ifdef NK_ZERO_COMMAND_MEMORY
  NK_MEMSET(cmd, 0, size + alignment);
#endif

  cmd->type = t;
  cmd->next = b->base->allocated + alignment;
#ifdef NK_INCLUDE_COMMAND_USERDATA
  cmd->userdata = b->userdata;
#endif
  b->end = cmd->next;
  return cmd;
}

NK_API void
nk_push_scissor(struct nk_command_buffer *b, struct nk_rect r)
{
  struct nk_command_scissor *cmd;

  NK_ASSERT(b);

  if (!b) {
    return;
  }

  b->clip.x = r.x;
  b->clip.y = r.y;
  b->clip.w = r.w;
  b->clip.h = r.h;
  cmd       = (struct nk_command_scissor *)nk_command_buffer_push(b, NK_COMMAND_SCISSOR, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->x = (short)r.x;
  cmd->y = (short)r.y;
  cmd->w = (unsigned short)NK_MAX(0, r.w);
  cmd->h = (unsigned short)NK_MAX(0, r.h);
}

NK_API void
nk_stroke_line(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float line_thickness, struct nk_color c)
{
  struct nk_command_line *cmd;

  NK_ASSERT(b);

  if (!b || line_thickness <= 0) {
    return;
  }

  cmd = (struct nk_command_line *)nk_command_buffer_push(b, NK_COMMAND_LINE, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->begin.x        = (short)x0;
  cmd->begin.y        = (short)y0;
  cmd->end.x          = (short)x1;
  cmd->end.y          = (short)y1;
  cmd->color          = c;
}

NK_API void
nk_stroke_curve(
  struct nk_command_buffer *b, float ax, float ay, float ctrl0x, float ctrl0y, float ctrl1x, float ctrl1y, float bx, float by, float line_thickness, struct nk_color col)
{
  struct nk_command_curve *cmd;

  NK_ASSERT(b);

  if (!b || col.a == 0 || line_thickness <= 0) {
    return;
  }

  cmd = (struct nk_command_curve *)nk_command_buffer_push(b, NK_COMMAND_CURVE, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->begin.x        = (short)ax;
  cmd->begin.y        = (short)ay;
  cmd->ctrl[0].x      = (short)ctrl0x;
  cmd->ctrl[0].y      = (short)ctrl0y;
  cmd->ctrl[1].x      = (short)ctrl1x;
  cmd->ctrl[1].y      = (short)ctrl1y;
  cmd->end.x          = (short)bx;
  cmd->end.y          = (short)by;
  cmd->color          = col;
}

NK_API void
nk_stroke_rect(struct nk_command_buffer *b, struct nk_rect rect, float rounding, float line_thickness, struct nk_color c)
{
  struct nk_command_rect *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0 || rect.w == 0 || rect.h == 0 || line_thickness <= 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_rect *)nk_command_buffer_push(b, NK_COMMAND_RECT, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->rounding       = (unsigned short)rounding;
  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->x              = (short)rect.x;
  cmd->y              = (short)rect.y;
  cmd->w              = (unsigned short)NK_MAX(0, rect.w);
  cmd->h              = (unsigned short)NK_MAX(0, rect.h);
  cmd->color          = c;
}

NK_API void
nk_fill_rect(struct nk_command_buffer *b, struct nk_rect rect, float rounding, struct nk_color c)
{
  struct nk_command_rect_filled *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0 || rect.w == 0 || rect.h == 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_rect_filled *)nk_command_buffer_push(b, NK_COMMAND_RECT_FILLED, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->rounding = (unsigned short)rounding;
  cmd->x        = (short)rect.x;
  cmd->y        = (short)rect.y;
  cmd->w        = (unsigned short)NK_MAX(0, rect.w);
  cmd->h        = (unsigned short)NK_MAX(0, rect.h);
  cmd->color    = c;
}

NK_API void
nk_fill_rect_multi_color(struct nk_command_buffer *b, struct nk_rect rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom)
{
  struct nk_command_rect_multi_color *cmd;

  NK_ASSERT(b);

  if (!b || rect.w == 0 || rect.h == 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INTERSECT(rect.x, rect.y, rect.w, rect.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_rect_multi_color *)nk_command_buffer_push(b, NK_COMMAND_RECT_MULTI_COLOR, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->x      = (short)rect.x;
  cmd->y      = (short)rect.y;
  cmd->w      = (unsigned short)NK_MAX(0, rect.w);
  cmd->h      = (unsigned short)NK_MAX(0, rect.h);
  cmd->left   = left;
  cmd->top    = top;
  cmd->right  = right;
  cmd->bottom = bottom;
}

NK_API void
nk_stroke_circle(struct nk_command_buffer *b, struct nk_rect r, float line_thickness, struct nk_color c)
{
  struct nk_command_circle *cmd;

  if (!b || r.w == 0 || r.h == 0 || line_thickness <= 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INTERSECT(r.x, r.y, r.w, r.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_circle *)nk_command_buffer_push(b, NK_COMMAND_CIRCLE, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->x              = (short)r.x;
  cmd->y              = (short)r.y;
  cmd->w              = (unsigned short)NK_MAX(r.w, 0);
  cmd->h              = (unsigned short)NK_MAX(r.h, 0);
  cmd->color          = c;
}

NK_API void
nk_fill_circle(struct nk_command_buffer *b, struct nk_rect r, struct nk_color c)
{
  struct nk_command_circle_filled *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0 || r.w == 0 || r.h == 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INTERSECT(r.x, r.y, r.w, r.h, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_circle_filled *)nk_command_buffer_push(b, NK_COMMAND_CIRCLE_FILLED, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->x     = (short)r.x;
  cmd->y     = (short)r.y;
  cmd->w     = (unsigned short)NK_MAX(r.w, 0);
  cmd->h     = (unsigned short)NK_MAX(r.h, 0);
  cmd->color = c;
}

NK_API void
nk_stroke_arc(struct nk_command_buffer *b, float cx, float cy, float radius, float a_min, float a_max, float line_thickness, struct nk_color c)
{
  struct nk_command_arc *cmd;

  if (!b || c.a == 0 || line_thickness <= 0) {
    return;
  }

  cmd = (struct nk_command_arc *)nk_command_buffer_push(b, NK_COMMAND_ARC, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->cx             = (short)cx;
  cmd->cy             = (short)cy;
  cmd->r              = (unsigned short)radius;
  cmd->a[0]           = a_min;
  cmd->a[1]           = a_max;
  cmd->color          = c;
}

NK_API void
nk_fill_arc(struct nk_command_buffer *b, float cx, float cy, float radius, float a_min, float a_max, struct nk_color c)
{
  struct nk_command_arc_filled *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0) {
    return;
  }

  cmd = (struct nk_command_arc_filled *)nk_command_buffer_push(b, NK_COMMAND_ARC_FILLED, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->cx    = (short)cx;
  cmd->cy    = (short)cy;
  cmd->r     = (unsigned short)radius;
  cmd->a[0]  = a_min;
  cmd->a[1]  = a_max;
  cmd->color = c;
}

NK_API void
nk_stroke_triangle(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float x2, float y2, float line_thickness, struct nk_color c)
{
  struct nk_command_triangle *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0 || line_thickness <= 0) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INBOX(x0, y0, clip->x, clip->y, clip->w, clip->h) && !NK_INBOX(x1, y1, clip->x, clip->y, clip->w, clip->h) && !NK_INBOX(x2, y2, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_triangle *)nk_command_buffer_push(b, NK_COMMAND_TRIANGLE, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->a.x            = (short)x0;
  cmd->a.y            = (short)y0;
  cmd->b.x            = (short)x1;
  cmd->b.y            = (short)y1;
  cmd->c.x            = (short)x2;
  cmd->c.y            = (short)y2;
  cmd->color          = c;
}

NK_API void
nk_fill_triangle(struct nk_command_buffer *b, float x0, float y0, float x1, float y1, float x2, float y2, struct nk_color c)
{
  struct nk_command_triangle_filled *cmd;

  NK_ASSERT(b);

  if (!b || c.a == 0) {
    return;
  }

  if (!b) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *clip = &b->clip;

    if (!NK_INBOX(x0, y0, clip->x, clip->y, clip->w, clip->h) && !NK_INBOX(x1, y1, clip->x, clip->y, clip->w, clip->h) && !NK_INBOX(x2, y2, clip->x, clip->y, clip->w, clip->h)) {
      return;
    }
  }

  cmd = (struct nk_command_triangle_filled *)nk_command_buffer_push(b, NK_COMMAND_TRIANGLE_FILLED, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->a.x   = (short)x0;
  cmd->a.y   = (short)y0;
  cmd->b.x   = (short)x1;
  cmd->b.y   = (short)y1;
  cmd->c.x   = (short)x2;
  cmd->c.y   = (short)y2;
  cmd->color = c;
}

NK_API void
nk_stroke_polygon(struct nk_command_buffer *b, const float *points, int point_count, float line_thickness, struct nk_color col)
{
  int                        i;
  nk_size                    size = 0;
  struct nk_command_polygon *cmd;

  NK_ASSERT(b);

  if (!b || col.a == 0 || line_thickness <= 0) {
    return;
  }

  size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
  cmd  = (struct nk_command_polygon *)nk_command_buffer_push(b, NK_COMMAND_POLYGON, size);

  if (!cmd) {
    return;
  }

  cmd->color          = col;
  cmd->line_thickness = (unsigned short)line_thickness;
  cmd->point_count    = (unsigned short)point_count;

  for (i = 0; i < point_count; ++i) {
    cmd->points[i].x = (short)points[i * 2];
    cmd->points[i].y = (short)points[i * 2 + 1];
  }
}

NK_API void
nk_fill_polygon(struct nk_command_buffer *b, const float *points, int point_count, struct nk_color col)
{
  int                               i;
  nk_size                           size = 0;
  struct nk_command_polygon_filled *cmd;

  NK_ASSERT(b);

  if (!b || col.a == 0) {
    return;
  }

  size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
  cmd  = (struct nk_command_polygon_filled *)nk_command_buffer_push(b, NK_COMMAND_POLYGON_FILLED, size);

  if (!cmd) {
    return;
  }

  cmd->color       = col;
  cmd->point_count = (unsigned short)point_count;

  for (i = 0; i < point_count; ++i) {
    cmd->points[i].x = (short)points[i * 2 + 0];
    cmd->points[i].y = (short)points[i * 2 + 1];
  }
}

NK_API void
nk_stroke_polyline(struct nk_command_buffer *b, const float *points, int point_count, float line_thickness, struct nk_color col)
{
  int                         i;
  nk_size                     size = 0;
  struct nk_command_polyline *cmd;

  NK_ASSERT(b);

  if (!b || col.a == 0 || line_thickness <= 0) {
    return;
  }

  size = sizeof(*cmd) + sizeof(short) * 2 * (nk_size)point_count;
  cmd  = (struct nk_command_polyline *)nk_command_buffer_push(b, NK_COMMAND_POLYLINE, size);

  if (!cmd) {
    return;
  }

  cmd->color          = col;
  cmd->point_count    = (unsigned short)point_count;
  cmd->line_thickness = (unsigned short)line_thickness;

  for (i = 0; i < point_count; ++i) {
    cmd->points[i].x = (short)points[i * 2];
    cmd->points[i].y = (short)points[i * 2 + 1];
  }
}

NK_API void
nk_draw_image(struct nk_command_buffer *b, struct nk_rect r, const struct nk_image *img, struct nk_color col)
{
  struct nk_command_image *cmd;

  NK_ASSERT(b);

  if (!b) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *c = &b->clip;

    if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h)) {
      return;
    }
  }

  cmd = (struct nk_command_image *)nk_command_buffer_push(b, NK_COMMAND_IMAGE, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->x   = (short)r.x;
  cmd->y   = (short)r.y;
  cmd->w   = (unsigned short)NK_MAX(0, r.w);
  cmd->h   = (unsigned short)NK_MAX(0, r.h);
  cmd->img = *img;
  cmd->col = col;
}

NK_API void
nk_draw_nine_slice(struct nk_command_buffer *b, struct nk_rect r, const struct nk_nine_slice *slc, struct nk_color col)
{
  struct nk_image        img;
  const struct nk_image *slcimg = (const struct nk_image *)slc;
  nk_ushort              rgnX, rgnY, rgnW, rgnH;

  rgnX = slcimg->region[0];
  rgnY = slcimg->region[1];
  rgnW = slcimg->region[2];
  rgnH = slcimg->region[3];

  /* top-left */
  img.handle    = slcimg->handle;
  img.w         = slcimg->w;
  img.h         = slcimg->h;
  img.region[0] = rgnX;
  img.region[1] = rgnY;
  img.region[2] = slc->l;
  img.region[3] = slc->t;

  nk_draw_image(b, nk_rect(r.x, r.y, (float)slc->l, (float)slc->t), &img, col);

#define IMG_RGN(x, y, w, h)       \
  img.region[0] = (nk_ushort)(x); \
  img.region[1] = (nk_ushort)(y); \
  img.region[2] = (nk_ushort)(w); \
  img.region[3] = (nk_ushort)(h);

  /* top-center */
  IMG_RGN(rgnX + slc->l, rgnY, rgnW - slc->l - slc->r, slc->t);
  nk_draw_image(b, nk_rect(r.x + (float)slc->l, r.y, (float)(r.w - slc->l - slc->r), (float)slc->t), &img, col);

  /* top-right */
  IMG_RGN(rgnX + rgnW - slc->r, rgnY, slc->r, slc->t);
  nk_draw_image(b, nk_rect(r.x + r.w - (float)slc->r, r.y, (float)slc->r, (float)slc->t), &img, col);

  /* center-left */
  IMG_RGN(rgnX, rgnY + slc->t, slc->l, rgnH - slc->t - slc->b);
  nk_draw_image(b, nk_rect(r.x, r.y + (float)slc->t, (float)slc->l, (float)(r.h - slc->t - slc->b)), &img, col);

  /* center */
  IMG_RGN(rgnX + slc->l, rgnY + slc->t, rgnW - slc->l - slc->r, rgnH - slc->t - slc->b);
  nk_draw_image(b, nk_rect(r.x + (float)slc->l, r.y + (float)slc->t, (float)(r.w - slc->l - slc->r), (float)(r.h - slc->t - slc->b)), &img, col);

  /* center-right */
  IMG_RGN(rgnX + rgnW - slc->r, rgnY + slc->t, slc->r, rgnH - slc->t - slc->b);
  nk_draw_image(b, nk_rect(r.x + r.w - (float)slc->r, r.y + (float)slc->t, (float)slc->r, (float)(r.h - slc->t - slc->b)), &img, col);

  /* bottom-left */
  IMG_RGN(rgnX, rgnY + rgnH - slc->b, slc->l, slc->b);
  nk_draw_image(b, nk_rect(r.x, r.y + r.h - (float)slc->b, (float)slc->l, (float)slc->b), &img, col);

  /* bottom-center */
  IMG_RGN(rgnX + slc->l, rgnY + rgnH - slc->b, rgnW - slc->l - slc->r, slc->b);
  nk_draw_image(b, nk_rect(r.x + (float)slc->l, r.y + r.h - (float)slc->b, (float)(r.w - slc->l - slc->r), (float)slc->b), &img, col);

  /* bottom-right */
  IMG_RGN(rgnX + rgnW - slc->r, rgnY + rgnH - slc->b, slc->r, slc->b);
  nk_draw_image(b, nk_rect(r.x + r.w - (float)slc->r, r.y + r.h - (float)slc->b, (float)slc->r, (float)slc->b), &img, col);

#undef IMG_RGN
}

NK_API void
nk_push_custom(struct nk_command_buffer *b, struct nk_rect r, nk_command_custom_callback cb, nk_handle usr)
{
  struct nk_command_custom *cmd;

  NK_ASSERT(b);

  if (!b) {
    return;
  }

  if (b->use_clipping) {
    const struct nk_rect *c = &b->clip;

    if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h)) {
      return;
    }
  }

  cmd = (struct nk_command_custom *)nk_command_buffer_push(b, NK_COMMAND_CUSTOM, sizeof(*cmd));

  if (!cmd) {
    return;
  }

  cmd->x             = (short)r.x;
  cmd->y             = (short)r.y;
  cmd->w             = (unsigned short)NK_MAX(0, r.w);
  cmd->h             = (unsigned short)NK_MAX(0, r.h);
  cmd->callback_data = usr;
  cmd->callback      = cb;
}

NK_INTERN void
nk_push_draw_text(struct nk_command_buffer *b, struct nk_rect r, const char *string, int length,
                  const struct nk_user_font *font, float height, struct nk_color bg, struct nk_color fg)
{
  struct nk_command_text *cmd;

  if (!length) {
    return;
  }

  cmd = (struct nk_command_text *)nk_command_buffer_push(b, NK_COMMAND_TEXT, sizeof(*cmd) + (nk_size)(length + 1));

  if (!cmd) {
    return;
  }

  cmd->x          = (short)r.x;
  cmd->y          = (short)r.y;
  cmd->w          = (unsigned short)r.w;
  cmd->h          = (unsigned short)r.h;
  cmd->background = bg;
  cmd->foreground = fg;
  cmd->font       = font;
  cmd->length     = length;
  cmd->height     = height;
  NK_MEMCPY(cmd->string, string, (nk_size)length);
  cmd->string[length] = '\0';
}

NK_API void
nk_draw_text(struct nk_command_buffer *b, struct nk_rect r, const char *string, int length,
             const struct nk_user_font *font, struct nk_color bg, struct nk_color fg)
{
  nk_draw_text_shaped(b, r, string, length, font, bg, fg, NULL);
}

/* shape may be NULL */
NK_LIB void
nk_draw_text_shaped(struct nk_command_buffer *b, struct nk_rect r, const char *string, int length,
                    const struct nk_user_font *font, struct nk_color bg, struct nk_color fg, const struct nk_text_shape *shape)
{
  float text_width;

  NK_ASSERT(b);
  NK_ASSERT(font);
  NK_ASSERT(b->context && "text needs a buffer started by a window (nk_start_buffer)");

  if (!b || !b->context || !string || !length || (bg.a == 0 && fg.a == 0)) {
    return;
  }

  if (b->use_clipping) {
    struct nk_rect *c = &b->clip;

    if (c->w == 0 || c->h == 0 || !NK_INTERSECT(r.x, r.y, r.w, r.h, c->x, c->y, c->w, c->h)) {
      return;
    }
  }

  if (!shape) {
    shape = nk_text_shape_build_default(b->context, font, b->context->style.font_size, string, length);
  }

  text_width = shape ? shape->advance.x : 0.0f;

  if (text_width > r.w) {
    int   glyphs = 0;
    float width  = text_width;

    length = nk_text_clamp_shape(shape, r.w, &glyphs, &width);
  }

  nk_push_draw_text(b, r, string, length, font, b->context->style.font_size, bg, fg);
}

/* ===============================================================
 *
 *                              VERTEX
 *
 * ===============================================================*/
#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT
NK_API void
nk_draw_list_init(struct nk_draw_list *list)
{
  nk_size i = 0;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  nk_zero(list, sizeof(*list));

  for (i = 0; i < NK_LEN(list->circle_vtx); ++i) {
    const int   size = NK_LEN(list->circle_vtx);
    const float a    = ((float)i / (float)size) * 2 * NK_PI;

    list->circle_vtx[i].x = (float)NK_COS(a);
    list->circle_vtx[i].y = (float)NK_SIN(a);
  }
}

NK_API void
nk_draw_list_setup(struct nk_draw_list            *canvas,
                   const struct nk_convert_config *config,
                   struct nk_buffer               *cmds,
                   struct nk_buffer               *vertices,
                   struct nk_buffer               *elements,
                   enum nk_anti_aliasing           line_aa,
                   enum nk_anti_aliasing           shape_aa)
{
  NK_ASSERT(canvas);
  NK_ASSERT(config);
  NK_ASSERT(cmds);
  NK_ASSERT(vertices);
  NK_ASSERT(elements);

  if (!canvas || !config || !cmds || !vertices || !elements) {
    return;
  }

  canvas->buffer    = cmds;
  canvas->config    = *config;
  canvas->elements  = elements;
  canvas->vertices  = vertices;
  canvas->line_AA   = line_aa;
  canvas->shape_AA  = shape_aa;
  canvas->clip_rect = nk_null_rect;

  canvas->cmd_offset    = 0;
  canvas->element_count = 0;
  canvas->vertex_count  = 0;
  canvas->cmd_offset    = 0;
  canvas->cmd_count     = 0;
  canvas->path_count    = 0;
}

NK_API const struct nk_draw_command *
nk__draw_list_begin(const struct nk_draw_list *canvas, const struct nk_buffer *buffer)
{
  nk_byte                      *memory;
  nk_size                       offset;
  const struct nk_draw_command *cmd;

  NK_ASSERT(buffer);

  if (!buffer || !buffer->size || !canvas->cmd_count) {
    return 0;
  }

  memory = (nk_byte *)buffer->memory.ptr;
  offset = buffer->memory.size - canvas->cmd_offset;
  cmd    = nk_ptr_add(const struct nk_draw_command, memory, offset);
  return cmd;
}

NK_API const struct nk_draw_command *
nk__draw_list_end(const struct nk_draw_list *canvas, const struct nk_buffer *buffer)
{
  nk_size                       size;
  nk_size                       offset;
  nk_byte                      *memory;
  const struct nk_draw_command *end;

  NK_ASSERT(buffer);
  NK_ASSERT(canvas);

  if (!buffer || !canvas) {
    return 0;
  }

  memory = (nk_byte *)buffer->memory.ptr;
  size   = buffer->memory.size;
  offset = size - canvas->cmd_offset;
  end    = nk_ptr_add(const struct nk_draw_command, memory, offset);
  end   -= (canvas->cmd_count - 1);
  return end;
}

NK_API const struct nk_draw_command *
nk__draw_list_next(const struct nk_draw_command *cmd, const struct nk_buffer *buffer, const struct nk_draw_list *canvas)
{
  const struct nk_draw_command *end;

  NK_ASSERT(buffer);
  NK_ASSERT(canvas);

  if (!cmd || !buffer || !canvas) {
    return 0;
  }

  end = nk__draw_list_end(canvas, buffer);

  if (cmd <= end) {
    return 0;
  }
  return (cmd - 1);
}

NK_INTERN struct nk_vec2 *
nk_draw_list_alloc_path(struct nk_draw_list *list, int count)
{
  struct nk_vec2          *points;
  NK_STORAGE const nk_size point_align = NK_ALIGNOF(struct nk_vec2);
  NK_STORAGE const nk_size point_size  = sizeof(struct nk_vec2);

  points = (struct nk_vec2 *)nk_buffer_alloc(list->buffer, NK_BUFFER_FRONT, point_size * (nk_size)count, point_align);

  if (!points) {
    return 0;
  }

  if (!list->path_offset) {
    void *memory = nk_buffer_memory(list->buffer);

    list->path_offset = (unsigned int)((nk_byte *)points - (nk_byte *)memory);
  }

  list->path_count += (unsigned int)count;
  return points;
}

NK_INTERN struct nk_vec2
nk_draw_list_path_last(struct nk_draw_list *list)
{
  void           *memory;
  struct nk_vec2 *point;

  NK_ASSERT(list->path_count);
  memory = nk_buffer_memory(list->buffer);
  point  = nk_ptr_add(struct nk_vec2, memory, list->path_offset);
  point += (list->path_count - 1);
  return *point;
}

NK_INTERN struct nk_draw_command *
nk_draw_list_push_command(struct nk_draw_list *list, struct nk_rect clip, nk_handle texture)
{
  NK_STORAGE const nk_size cmd_align = NK_ALIGNOF(struct nk_draw_command);
  NK_STORAGE const nk_size cmd_size  = sizeof(struct nk_draw_command);
  struct nk_draw_command  *cmd;

  NK_ASSERT(list);
  cmd = (struct nk_draw_command *)nk_buffer_alloc(list->buffer, NK_BUFFER_BACK, cmd_size, cmd_align);

  if (!cmd) {
    return 0;
  }

  if (!list->cmd_count) {
    nk_byte *memory = (nk_byte *)nk_buffer_memory(list->buffer);
    nk_size  total  = nk_buffer_total(list->buffer);

    memory           = nk_ptr_add(nk_byte, memory, total);
    list->cmd_offset = (nk_size)(memory - (nk_byte *)cmd);
  }

  cmd->elem_count = 0;
  cmd->clip_rect  = clip;
  cmd->texture    = texture;
#  ifdef NK_INCLUDE_COMMAND_USERDATA
  cmd->userdata = list->userdata;
#  endif

  list->cmd_count++;
  list->clip_rect = clip;
  return cmd;
}

NK_INTERN struct nk_draw_command *
nk_draw_list_command_last(struct nk_draw_list *list)
{
  void                   *memory;
  nk_size                 size;
  struct nk_draw_command *cmd;

  NK_ASSERT(list->cmd_count);

  memory = nk_buffer_memory(list->buffer);
  size   = nk_buffer_total(list->buffer);
  cmd    = nk_ptr_add(struct nk_draw_command, memory, size - list->cmd_offset);
  return (cmd - (list->cmd_count - 1));
}

NK_INTERN void
nk_draw_list_add_clip(struct nk_draw_list *list, struct nk_rect rect)
{
  NK_ASSERT(list);

  if (!list) {
    return;
  }

  if (!list->cmd_count) {
    nk_draw_list_push_command(list, rect, list->config.tex_null.texture);
  } else {
    struct nk_draw_command *prev = nk_draw_list_command_last(list);

    if (prev->elem_count == 0) {
      prev->clip_rect = rect;
    }

    nk_draw_list_push_command(list, rect, prev->texture);
  }
}

NK_INTERN void
nk_draw_list_push_image(struct nk_draw_list *list, nk_handle texture)
{
  NK_ASSERT(list);

  if (!list) {
    return;
  }

  if (!list->cmd_count) {
    nk_draw_list_push_command(list, nk_null_rect, texture);
  } else {
    struct nk_draw_command *prev = nk_draw_list_command_last(list);

    if (prev->elem_count == 0) {
      prev->texture = texture;
#  ifdef NK_INCLUDE_COMMAND_USERDATA
      prev->userdata = list->userdata;
#  endif
    } else if (prev->texture.id != texture.id
#  ifdef NK_INCLUDE_COMMAND_USERDATA
               || prev->userdata.id != list->userdata.id
#  endif
    ) {
      nk_draw_list_push_command(list, prev->clip_rect, texture);
    }
  }
}

#  ifdef NK_INCLUDE_COMMAND_USERDATA
NK_API void
nk_draw_list_push_userdata(struct nk_draw_list *list, nk_handle userdata)
{
  list->userdata = userdata;
}
#  endif

NK_INTERN void *
nk_draw_list_alloc_vertices(struct nk_draw_list *list, nk_size count)
{
  void *vtx;

  NK_ASSERT(list);

  if (!list) {
    return 0;
  }

  vtx = nk_buffer_alloc(list->vertices, NK_BUFFER_FRONT, list->config.vertex_size * count, list->config.vertex_alignment);

  if (!vtx) {
    return 0;
  }

  list->vertex_count += (unsigned int)count;

  /* This assert triggers because your are drawing a lot of stuff and nuklear
   * defined `nk_draw_index` as `nk_ushort` to safe space be default.
   *
   * So you reached the maximum number of indices or rather vertexes.
   * To solve this issue please change typedef `nk_draw_index` to `nk_uint`
   * and don't forget to specify the new element size in your drawing
   * backend (OpenGL, DirectX, ...). For example in OpenGL for `glDrawElements`
   * instead of specifying `GL_UNSIGNED_SHORT` you have to define `GL_UNSIGNED_INT`.
   * Sorry for the inconvenience. */
  if (sizeof(nk_draw_index) == 2) {
    NK_ASSERT((list->vertex_count < NK_USHORT_MAX && "To many vertices for 16-bit vertex indices. Please read comment above on how to solve this problem"));
  }
  return vtx;
}

NK_INTERN nk_draw_index *
nk_draw_list_alloc_elements(struct nk_draw_list *list, nk_size count)
{
  nk_draw_index           *ids;
  struct nk_draw_command  *cmd;
  NK_STORAGE const nk_size elem_align = NK_ALIGNOF(nk_draw_index);
  NK_STORAGE const nk_size elem_size  = sizeof(nk_draw_index);

  NK_ASSERT(list);

  if (!list) {
    return 0;
  }

  ids = (nk_draw_index *)nk_buffer_alloc(list->elements, NK_BUFFER_FRONT, elem_size * count, elem_align);

  if (!ids) {
    return 0;
  }

  cmd                  = nk_draw_list_command_last(list);
  list->element_count += (unsigned int)count;
  cmd->elem_count     += (unsigned int)count;
  return ids;
}

NK_INTERN int
nk_draw_vertex_layout_element_is_end_of_layout(const struct nk_draw_vertex_layout_element *element)
{
  return (element->attribute == NK_VERTEX_ATTRIBUTE_COUNT || element->format == NK_FORMAT_COUNT);
}

NK_INTERN void
nk_draw_vertex_color(void *attr, const float *vals, enum nk_draw_vertex_layout_format format)
{
  /* if this triggers you tried to provide a value format for a color */
  float val[4];

  NK_ASSERT(format >= NK_FORMAT_COLOR_BEGIN);
  NK_ASSERT(format <= NK_FORMAT_COLOR_END);

  if (format < NK_FORMAT_COLOR_BEGIN || format > NK_FORMAT_COLOR_END) {
    return;
  }

  val[0] = NK_SATURATE(vals[0]);
  val[1] = NK_SATURATE(vals[1]);
  val[2] = NK_SATURATE(vals[2]);
  val[3] = NK_SATURATE(vals[3]);

  switch (format) {
    default:
      NK_ASSERT(0 && "Invalid vertex layout color format");
      break;

    case NK_FORMAT_R8G8B8A8:
    case NK_FORMAT_R8G8B8: {
      struct nk_color col = nk_rgba_fv(val);

      NK_MEMCPY(attr, &col.r, sizeof(col));
    } break;
    case NK_FORMAT_B8G8R8A8: {
      struct nk_color col  = nk_rgba_fv(val);
      struct nk_color bgra = nk_rgba(col.b, col.g, col.r, col.a);

      NK_MEMCPY(attr, &bgra, sizeof(bgra));
    } break;
    case NK_FORMAT_R16G15B16: {
      nk_ushort col[3];

      col[0] = (nk_ushort)(val[0] * (float)NK_USHORT_MAX);
      col[1] = (nk_ushort)(val[1] * (float)NK_USHORT_MAX);
      col[2] = (nk_ushort)(val[2] * (float)NK_USHORT_MAX);
      NK_MEMCPY(attr, col, sizeof(col));
    } break;
    case NK_FORMAT_R16G15B16A16: {
      nk_ushort col[4];

      col[0] = (nk_ushort)(val[0] * (float)NK_USHORT_MAX);
      col[1] = (nk_ushort)(val[1] * (float)NK_USHORT_MAX);
      col[2] = (nk_ushort)(val[2] * (float)NK_USHORT_MAX);
      col[3] = (nk_ushort)(val[3] * (float)NK_USHORT_MAX);
      NK_MEMCPY(attr, col, sizeof(col));
    } break;
    case NK_FORMAT_R32G32B32: {
      nk_uint col[3];

      col[0] = (nk_uint)(val[0] * (float)NK_UINT_MAX);
      col[1] = (nk_uint)(val[1] * (float)NK_UINT_MAX);
      col[2] = (nk_uint)(val[2] * (float)NK_UINT_MAX);
      NK_MEMCPY(attr, col, sizeof(col));
    } break;
    case NK_FORMAT_R32G32B32A32: {
      nk_uint col[4];

      col[0] = (nk_uint)(val[0] * (float)NK_UINT_MAX);
      col[1] = (nk_uint)(val[1] * (float)NK_UINT_MAX);
      col[2] = (nk_uint)(val[2] * (float)NK_UINT_MAX);
      col[3] = (nk_uint)(val[3] * (float)NK_UINT_MAX);
      NK_MEMCPY(attr, col, sizeof(col));
    } break;
    case NK_FORMAT_R32G32B32A32_FLOAT:
      NK_MEMCPY(attr, val, sizeof(float) * 4);
      break;

    case NK_FORMAT_R32G32B32A32_DOUBLE: {
      double col[4];

      col[0] = (double)val[0];
      col[1] = (double)val[1];
      col[2] = (double)val[2];
      col[3] = (double)val[3];
      NK_MEMCPY(attr, col, sizeof(col));
    } break;
    case NK_FORMAT_RGB32:
    case NK_FORMAT_RGBA32: {
      struct nk_color col   = nk_rgba_fv(val);
      nk_uint         color = nk_color_u32(col);

      NK_MEMCPY(attr, &color, sizeof(color));
    } break;
  }
}

NK_INTERN void
nk_draw_vertex_element(void *dst, const float *values, int value_count, enum nk_draw_vertex_layout_format format)
{
  int   value_index;
  void *attribute = dst;

  /* if this triggers you tried to provide a color format for a value */
  NK_ASSERT(format < NK_FORMAT_COLOR_BEGIN);

  if (format >= NK_FORMAT_COLOR_BEGIN && format <= NK_FORMAT_COLOR_END) {
    return;
  }

  for (value_index = 0; value_index < value_count; ++value_index) {
    switch (format) {
      default:
        NK_ASSERT(0 && "invalid vertex layout format");
        break;

      case NK_FORMAT_SCHAR: {
        char value = (char)NK_CLAMP((float)NK_SCHAR_MIN, values[value_index], (float)NK_SCHAR_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(char));
      } break;
      case NK_FORMAT_SSHORT: {
        nk_short value = (nk_short)NK_CLAMP((float)NK_SSHORT_MIN, values[value_index], (float)NK_SSHORT_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(value));
      } break;
      case NK_FORMAT_SINT: {
        nk_int value = (nk_int)NK_CLAMP((float)NK_SINT_MIN, values[value_index], (float)NK_SINT_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(nk_int));
      } break;
      case NK_FORMAT_UCHAR: {
        unsigned char value = (unsigned char)NK_CLAMP((float)NK_UCHAR_MIN, values[value_index], (float)NK_UCHAR_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(unsigned char));
      } break;
      case NK_FORMAT_USHORT: {
        nk_ushort value = (nk_ushort)NK_CLAMP((float)NK_USHORT_MIN, values[value_index], (float)NK_USHORT_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(value));
      } break;
      case NK_FORMAT_UINT: {
        nk_uint value = (nk_uint)NK_CLAMP((float)NK_UINT_MIN, values[value_index], (float)NK_UINT_MAX);

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(nk_uint));
      } break;
      case NK_FORMAT_FLOAT:
        NK_MEMCPY(attribute, &values[value_index], sizeof(values[value_index]));
        attribute = (void *)((char *)attribute + sizeof(float));
        break;

      case NK_FORMAT_DOUBLE: {
        double value = (double)values[value_index];

        NK_MEMCPY(attribute, &value, sizeof(value));
        attribute = (void *)((char *)attribute + sizeof(double));
      } break;
    }
  }
}

NK_INTERN void *
nk_draw_vertex(void *dst, const struct nk_convert_config *config, struct nk_vec2 pos, struct nk_vec2 uv, struct nk_colorf color)
{
  void                                       *result    = (void *)((char *)dst + config->vertex_size);
  const struct nk_draw_vertex_layout_element *elem_iter = config->vertex_layout;

  while (!nk_draw_vertex_layout_element_is_end_of_layout(elem_iter)) {
    void *address = (void *)((char *)dst + elem_iter->offset);

    switch (elem_iter->attribute) {
      case NK_VERTEX_ATTRIBUTE_COUNT:
      default:
        NK_ASSERT(0 && "wrong element attribute");
        break;

      case NK_VERTEX_POSITION:
        nk_draw_vertex_element(address, &pos.x, 2, elem_iter->format);
        break;

      case NK_VERTEX_TEXCOORD:
        nk_draw_vertex_element(address, &uv.x, 2, elem_iter->format);
        break;

      case NK_VERTEX_COLOR:
        nk_draw_vertex_color(address, &color.r, elem_iter->format);
        break;
    }

    elem_iter++;
  }
  return result;
}

NK_API void
nk_draw_list_stroke_poly_line(struct nk_draw_list     *list,
                              const struct nk_vec2    *points,
                              const unsigned int       points_count,
                              struct nk_color          color,
                              enum nk_draw_list_stroke closed,
                              float                    thickness,
                              enum nk_anti_aliasing    aliasing)
{
  nk_size          count;
  int              thick_line;
  struct nk_colorf col;
  struct nk_colorf col_trans;

  NK_ASSERT(list);

  if (!list || points_count < 2) {
    return;
  }

  color.a = (nk_byte)((float)color.a * list->config.global_alpha);
  count   = points_count;

  if (!closed) {
    count = points_count - 1;
  }

  thick_line = thickness > 1.0f;

#  ifdef NK_INCLUDE_COMMAND_USERDATA
  nk_draw_list_push_userdata(list, list->userdata);
#  endif

  color.a = (nk_byte)((float)color.a * list->config.global_alpha);
  nk_color_fv(&col.r, color);
  col_trans   = col;
  col_trans.a = 0;

  if (aliasing == NK_ANTI_ALIASING_ON) {
    /* ANTI-ALIASED STROKE */
    const float              AA_SIZE   = 1.0f;
    NK_STORAGE const nk_size pnt_align = NK_ALIGNOF(struct nk_vec2);
    NK_STORAGE const nk_size pnt_size  = sizeof(struct nk_vec2);

    /* allocate vertices and elements  */
    nk_size i1 = 0;
    nk_size vertex_offset;
    nk_size index = list->vertex_count;

    const nk_size idx_count = (thick_line) ? (count * 18) : (count * 12);
    const nk_size vtx_count = (thick_line) ? (points_count * 4) : (points_count * 3);

    void          *vtx = nk_draw_list_alloc_vertices(list, vtx_count);
    nk_draw_index *ids = nk_draw_list_alloc_elements(list, idx_count);

    nk_size         size;
    struct nk_vec2 *normals, *temp;

    if (!vtx || !ids) {
      return;
    }

    /* temporary allocate normals + points */
    vertex_offset = (nk_size)((nk_byte *)vtx - (nk_byte *)list->vertices->memory.ptr);
    nk_buffer_mark(list->vertices, NK_BUFFER_FRONT);
    size    = pnt_size * ((thick_line) ? 5 : 3) * points_count;
    normals = (struct nk_vec2 *)nk_buffer_alloc(list->vertices, NK_BUFFER_FRONT, size, pnt_align);

    if (!normals) {
      return;
    }

    temp = normals + points_count;

    /* make sure vertex pointer is still correct */
    vtx = (void *)((nk_byte *)list->vertices->memory.ptr + vertex_offset);

    /* calculate normals */
    for (i1 = 0; i1 < count; ++i1) {
      const nk_size  i2   = ((i1 + 1) == points_count) ? 0 : (i1 + 1);
      struct nk_vec2 diff = nk_vec2_sub(points[i2], points[i1]);
      float          len;

      /* vec2 inverted length  */
      len = nk_vec2_len_sqr(diff);

      if (len != 0.0f) {
        len = NK_INV_SQRT(len);
      } else {
        len = 1.0f;
      }

      diff          = nk_vec2_muls(diff, len);
      normals[i1].x = diff.y;
      normals[i1].y = -diff.x;
    }

    if (!closed) {
      normals[points_count - 1] = normals[points_count - 2];
    }

    if (!thick_line) {
      nk_size idx1, i;

      if (!closed) {
        struct nk_vec2 d;

        temp[0] = nk_vec2_add(points[0], nk_vec2_muls(normals[0], AA_SIZE));
        temp[1] = nk_vec2_sub(points[0], nk_vec2_muls(normals[0], AA_SIZE));
        d       = nk_vec2_muls(normals[points_count - 1], AA_SIZE);
        temp[(points_count - 1) * 2 + 0] = nk_vec2_add(points[points_count - 1], d);
        temp[(points_count - 1) * 2 + 1] = nk_vec2_sub(points[points_count - 1], d);
      }

      /* fill elements */
      idx1 = index;

      for (i1 = 0; i1 < count; i1++) {
        struct nk_vec2 dm;
        float          dmr2;
        nk_size        i2   = ((i1 + 1) == points_count) ? 0 : (i1 + 1);
        nk_size        idx2 = ((i1 + 1) == points_count) ? index : (idx1 + 3);

        /* average normals */
        dm   = nk_vec2_muls(nk_vec2_add(normals[i1], normals[i2]), 0.5f);
        dmr2 = dm.x * dm.x + dm.y * dm.y;

        if (dmr2 > 0.000001f) {
          float scale = 1.0f / dmr2;

          scale = NK_MIN(100.0f, scale);
          dm    = nk_vec2_muls(dm, scale);
        }

        dm = nk_vec2_muls(dm, AA_SIZE);
        temp[i2 * 2 + 0] = nk_vec2_add(points[i2], dm);
        temp[i2 * 2 + 1] = nk_vec2_sub(points[i2], dm);

        ids[0]  = (nk_draw_index)(idx2 + 0);
        ids[1]  = (nk_draw_index)(idx1 + 0);
        ids[2]  = (nk_draw_index)(idx1 + 2);
        ids[3]  = (nk_draw_index)(idx1 + 2);
        ids[4]  = (nk_draw_index)(idx2 + 2);
        ids[5]  = (nk_draw_index)(idx2 + 0);
        ids[6]  = (nk_draw_index)(idx2 + 1);
        ids[7]  = (nk_draw_index)(idx1 + 1);
        ids[8]  = (nk_draw_index)(idx1 + 0);
        ids[9]  = (nk_draw_index)(idx1 + 0);
        ids[10] = (nk_draw_index)(idx2 + 0);
        ids[11] = (nk_draw_index)(idx2 + 1);
        ids    += 12;
        idx1    = idx2;
      }

      /* fill vertices */
      for (i = 0; i < points_count; ++i) {
        const struct nk_vec2 uv = list->config.tex_null.uv;

        vtx = nk_draw_vertex(vtx, &list->config, points[i], uv, col);
        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 2 + 0], uv, col_trans);
        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 2 + 1], uv, col_trans);
      }
    } else {
      nk_size     idx1, i;
      const float half_inner_thickness = (thickness - AA_SIZE) * 0.5f;

      if (!closed) {
        struct nk_vec2 d1 = nk_vec2_muls(normals[0], half_inner_thickness + AA_SIZE);
        struct nk_vec2 d2 = nk_vec2_muls(normals[0], half_inner_thickness);

        temp[0] = nk_vec2_add(points[0], d1);
        temp[1] = nk_vec2_add(points[0], d2);
        temp[2] = nk_vec2_sub(points[0], d2);
        temp[3] = nk_vec2_sub(points[0], d1);

        d1 = nk_vec2_muls(normals[points_count - 1], half_inner_thickness + AA_SIZE);
        d2 = nk_vec2_muls(normals[points_count - 1], half_inner_thickness);

        temp[(points_count - 1) * 4 + 0] = nk_vec2_add(points[points_count - 1], d1);
        temp[(points_count - 1) * 4 + 1] = nk_vec2_add(points[points_count - 1], d2);
        temp[(points_count - 1) * 4 + 2] = nk_vec2_sub(points[points_count - 1], d2);
        temp[(points_count - 1) * 4 + 3] = nk_vec2_sub(points[points_count - 1], d1);
      }

      /* add all elements */
      idx1 = index;

      for (i1 = 0; i1 < count; ++i1) {
        struct nk_vec2 dm_out, dm_in;
        const nk_size  i2   = ((i1 + 1) == points_count) ? 0 : (i1 + 1);
        nk_size        idx2 = ((i1 + 1) == points_count) ? index : (idx1 + 4);

        /* average normals */
        struct nk_vec2 dm   = nk_vec2_muls(nk_vec2_add(normals[i1], normals[i2]), 0.5f);
        float          dmr2 = dm.x * dm.x + dm.y * dm.y;

        if (dmr2 > 0.000001f) {
          float scale = 1.0f / dmr2;

          scale = NK_MIN(100.0f, scale);
          dm    = nk_vec2_muls(dm, scale);
        }

        dm_out = nk_vec2_muls(dm, ((half_inner_thickness) + AA_SIZE));
        dm_in  = nk_vec2_muls(dm, half_inner_thickness);
        temp[i2 * 4 + 0] = nk_vec2_add(points[i2], dm_out);
        temp[i2 * 4 + 1] = nk_vec2_add(points[i2], dm_in);
        temp[i2 * 4 + 2] = nk_vec2_sub(points[i2], dm_in);
        temp[i2 * 4 + 3] = nk_vec2_sub(points[i2], dm_out);

        /* add indexes */
        ids[0]  = (nk_draw_index)(idx2 + 1);
        ids[1]  = (nk_draw_index)(idx1 + 1);
        ids[2]  = (nk_draw_index)(idx1 + 2);
        ids[3]  = (nk_draw_index)(idx1 + 2);
        ids[4]  = (nk_draw_index)(idx2 + 2);
        ids[5]  = (nk_draw_index)(idx2 + 1);
        ids[6]  = (nk_draw_index)(idx2 + 1);
        ids[7]  = (nk_draw_index)(idx1 + 1);
        ids[8]  = (nk_draw_index)(idx1 + 0);
        ids[9]  = (nk_draw_index)(idx1 + 0);
        ids[10] = (nk_draw_index)(idx2 + 0);
        ids[11] = (nk_draw_index)(idx2 + 1);
        ids[12] = (nk_draw_index)(idx2 + 2);
        ids[13] = (nk_draw_index)(idx1 + 2);
        ids[14] = (nk_draw_index)(idx1 + 3);
        ids[15] = (nk_draw_index)(idx1 + 3);
        ids[16] = (nk_draw_index)(idx2 + 3);
        ids[17] = (nk_draw_index)(idx2 + 2);
        ids    += 18;
        idx1    = idx2;
      }

      /* add vertices */
      for (i = 0; i < points_count; ++i) {
        const struct nk_vec2 uv = list->config.tex_null.uv;

        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 4 + 0], uv, col_trans);
        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 4 + 1], uv, col);
        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 4 + 2], uv, col);
        vtx = nk_draw_vertex(vtx, &list->config, temp[i * 4 + 3], uv, col_trans);
      }
    }

    /* free temporary normals + points */
    nk_buffer_reset(list->vertices, NK_BUFFER_FRONT);
  } else {
    /* NON ANTI-ALIASED STROKE */
    nk_size        i1        = 0;
    nk_size        idx       = list->vertex_count;
    const nk_size  idx_count = count * 6;
    const nk_size  vtx_count = count * 4;
    void          *vtx       = nk_draw_list_alloc_vertices(list, vtx_count);
    nk_draw_index *ids       = nk_draw_list_alloc_elements(list, idx_count);

    if (!vtx || !ids) {
      return;
    }

    for (i1 = 0; i1 < count; ++i1) {
      float                dx, dy;
      const struct nk_vec2 uv   = list->config.tex_null.uv;
      const nk_size        i2   = ((i1 + 1) == points_count) ? 0 : i1 + 1;
      const struct nk_vec2 p1   = points[i1];
      const struct nk_vec2 p2   = points[i2];
      struct nk_vec2       diff = nk_vec2_sub(p2, p1);
      float                len;

      /* vec2 inverted length  */
      len = nk_vec2_len_sqr(diff);

      if (len != 0.0f) {
        len = NK_INV_SQRT(len);
      } else {
        len = 1.0f;
      }

      diff = nk_vec2_muls(diff, len);

      /* add vertices */
      dx = diff.x * (thickness * 0.5f);
      dy = diff.y * (thickness * 0.5f);

      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(p1.x + dy, p1.y - dx), uv, col);
      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(p2.x + dy, p2.y - dx), uv, col);
      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(p2.x - dy, p2.y + dx), uv, col);
      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(p1.x - dy, p1.y + dx), uv, col);

      ids[0] = (nk_draw_index)(idx + 0);
      ids[1] = (nk_draw_index)(idx + 1);
      ids[2] = (nk_draw_index)(idx + 2);
      ids[3] = (nk_draw_index)(idx + 0);
      ids[4] = (nk_draw_index)(idx + 2);
      ids[5] = (nk_draw_index)(idx + 3);

      ids += 6;
      idx += 4;
    }
  }
}

NK_API void
nk_draw_list_fill_poly_convex(struct nk_draw_list *list, const struct nk_vec2 *points, const unsigned int points_count, struct nk_color color, enum nk_anti_aliasing aliasing)
{
  struct nk_colorf col;
  struct nk_colorf col_trans;

  NK_STORAGE const nk_size pnt_align = NK_ALIGNOF(struct nk_vec2);
  NK_STORAGE const nk_size pnt_size  = sizeof(struct nk_vec2);

  NK_ASSERT(list);

  if (!list || points_count < 3) {
    return;
  }

#  ifdef NK_INCLUDE_COMMAND_USERDATA
  nk_draw_list_push_userdata(list, list->userdata);
#  endif

  color.a = (nk_byte)((float)color.a * list->config.global_alpha);
  nk_color_fv(&col.r, color);
  col_trans   = col;
  col_trans.a = 0;

  if (aliasing == NK_ANTI_ALIASING_ON) {
    nk_size i  = 0;
    nk_size i0 = 0;
    nk_size i1 = 0;

    const float AA_SIZE       = 1.0f;
    nk_size     vertex_offset = 0;
    nk_size     index         = list->vertex_count;

    const nk_size idx_count = (points_count - 2) * 3 + points_count * 6;
    const nk_size vtx_count = (points_count * 2);

    void          *vtx = nk_draw_list_alloc_vertices(list, vtx_count);
    nk_draw_index *ids = nk_draw_list_alloc_elements(list, idx_count);

    nk_size         size          = 0;
    struct nk_vec2 *normals       = 0;
    unsigned int    vtx_inner_idx = (unsigned int)(index + 0);
    unsigned int    vtx_outer_idx = (unsigned int)(index + 1);

    if (!vtx || !ids) {
      return;
    }

    /* temporary allocate normals */
    vertex_offset = (nk_size)((nk_byte *)vtx - (nk_byte *)list->vertices->memory.ptr);
    nk_buffer_mark(list->vertices, NK_BUFFER_FRONT);
    size    = pnt_size * points_count;
    normals = (struct nk_vec2 *)nk_buffer_alloc(list->vertices, NK_BUFFER_FRONT, size, pnt_align);

    if (!normals) {
      return;
    }

    vtx = (void *)((nk_byte *)list->vertices->memory.ptr + vertex_offset);

    /* add elements */
    for (i = 2; i < points_count; i++) {
      ids[0] = (nk_draw_index)(vtx_inner_idx);
      ids[1] = (nk_draw_index)(vtx_inner_idx + ((i - 1) << 1));
      ids[2] = (nk_draw_index)(vtx_inner_idx + (i << 1));
      ids   += 3;
    }

    /* compute normals */
    for (i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++) {
      struct nk_vec2 p0   = points[i0];
      struct nk_vec2 p1   = points[i1];
      struct nk_vec2 diff = nk_vec2_sub(p1, p0);

      /* vec2 inverted length  */
      float len = nk_vec2_len_sqr(diff);

      if (len != 0.0f) {
        len = NK_INV_SQRT(len);
      } else {
        len = 1.0f;
      }

      diff = nk_vec2_muls(diff, len);

      normals[i0].x = diff.y;
      normals[i0].y = -diff.x;
    }

    /* add vertices + indexes */
    for (i0 = points_count - 1, i1 = 0; i1 < points_count; i0 = i1++) {
      const struct nk_vec2 uv   = list->config.tex_null.uv;
      struct nk_vec2       n0   = normals[i0];
      struct nk_vec2       n1   = normals[i1];
      struct nk_vec2       dm   = nk_vec2_muls(nk_vec2_add(n0, n1), 0.5f);
      float                dmr2 = dm.x * dm.x + dm.y * dm.y;

      if (dmr2 > 0.000001f) {
        float scale = 1.0f / dmr2;

        scale = NK_MIN(scale, 100.0f);
        dm    = nk_vec2_muls(dm, scale);
      }

      dm = nk_vec2_muls(dm, AA_SIZE * 0.5f);

      /* add vertices */
      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2_sub(points[i1], dm), uv, col);
      vtx = nk_draw_vertex(vtx, &list->config, nk_vec2_add(points[i1], dm), uv, col_trans);

      /* add indexes */
      ids[0] = (nk_draw_index)(vtx_inner_idx + (i1 << 1));
      ids[1] = (nk_draw_index)(vtx_inner_idx + (i0 << 1));
      ids[2] = (nk_draw_index)(vtx_outer_idx + (i0 << 1));
      ids[3] = (nk_draw_index)(vtx_outer_idx + (i0 << 1));
      ids[4] = (nk_draw_index)(vtx_outer_idx + (i1 << 1));
      ids[5] = (nk_draw_index)(vtx_inner_idx + (i1 << 1));
      ids   += 6;
    }

    /* free temporary normals + points */
    nk_buffer_reset(list->vertices, NK_BUFFER_FRONT);
  } else {
    nk_size        i         = 0;
    nk_size        index     = list->vertex_count;
    const nk_size  idx_count = (points_count - 2) * 3;
    const nk_size  vtx_count = points_count;
    void          *vtx       = nk_draw_list_alloc_vertices(list, vtx_count);
    nk_draw_index *ids       = nk_draw_list_alloc_elements(list, idx_count);

    if (!vtx || !ids) {
      return;
    }

    for (i = 0; i < vtx_count; ++i) {
      vtx = nk_draw_vertex(vtx, &list->config, points[i], list->config.tex_null.uv, col);
    }

    for (i = 2; i < points_count; ++i) {
      ids[0] = (nk_draw_index)index;
      ids[1] = (nk_draw_index)(index + i - 1);
      ids[2] = (nk_draw_index)(index + i);
      ids   += 3;
    }
  }
}

NK_API void
nk_draw_list_path_clear(struct nk_draw_list *list)
{
  NK_ASSERT(list);

  if (!list) {
    return;
  }

  nk_buffer_reset(list->buffer, NK_BUFFER_FRONT);
  list->path_count  = 0;
  list->path_offset = 0;
}

NK_API void
nk_draw_list_path_line_to(struct nk_draw_list *list, struct nk_vec2 pos)
{
  struct nk_vec2         *points = 0;
  struct nk_draw_command *cmd    = 0;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  if (!list->cmd_count) {
    nk_draw_list_add_clip(list, nk_null_rect);
  }

  cmd = nk_draw_list_command_last(list);

  if (cmd && cmd->texture.ptr != list->config.tex_null.texture.ptr) {
    nk_draw_list_push_image(list, list->config.tex_null.texture);
  }

  points = nk_draw_list_alloc_path(list, 1);

  if (!points) {
    return;
  }

  points[0] = pos;
}

NK_API void
nk_draw_list_path_arc_to_fast(struct nk_draw_list *list, struct nk_vec2 center, float radius, int a_min, int a_max)
{
  int a = 0;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  if (a_min <= a_max) {
    for (a = a_min; a <= a_max; a++) {
      const struct nk_vec2 c = list->circle_vtx[(nk_size)a % NK_LEN(list->circle_vtx)];
      const float          x = center.x + c.x * radius;
      const float          y = center.y + c.y * radius;

      nk_draw_list_path_line_to(list, nk_vec2(x, y));
    }
  }
}

NK_API void
nk_draw_list_path_arc_to(struct nk_draw_list *list, struct nk_vec2 center, float radius, float a_min, float a_max, unsigned int segments)
{
  unsigned int i = 0;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  if (radius == 0.0f) {
    return;
  }

  /*  This algorithm for arc drawing relies on these two trigonometric identities[1]:
          sin(a + b) = sin(a) * cos(b) + cos(a) * sin(b)
          cos(a + b) = cos(a) * cos(b) - sin(a) * sin(b)

      Two coordinates (x, y) of a point on a circle centered on
      the origin can be written in polar form as:
          x = r * cos(a)
          y = r * sin(a)
      where r is the radius of the circle,
          a is the angle between (x, y) and the origin.

      This allows us to rotate the coordinates around the
      origin by an angle b using the following transformation:
          x' = r * cos(a + b) = x * cos(b) - y * sin(b)
          y' = r * sin(a + b) = y * cos(b) + x * sin(b)

      [1] https://en.wikipedia.org/wiki/List_of_trigonometric_identities#Angle_sum_and_difference_identities
  */
  {
    const float d_angle = (a_max - a_min) / (float)segments;
    const float sin_d   = (float)NK_SIN(d_angle);
    const float cos_d   = (float)NK_COS(d_angle);

    float cx = (float)NK_COS(a_min) * radius;
    float cy = (float)NK_SIN(a_min) * radius;

    for (i = 0; i <= segments; ++i) {
      float       new_cx, new_cy;
      const float x = center.x + cx;
      const float y = center.y + cy;

      nk_draw_list_path_line_to(list, nk_vec2(x, y));

      new_cx = cx * cos_d - cy * sin_d;
      new_cy = cy * cos_d + cx * sin_d;
      cx     = new_cx;
      cy     = new_cy;
    }
  }
}

NK_API void
nk_draw_list_path_rect_to(struct nk_draw_list *list, struct nk_vec2 a, struct nk_vec2 b, float rounding)
{
  float r;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  r = rounding;
  r = NK_MIN(r, ((b.x - a.x) < 0) ? -(b.x - a.x) : (b.x - a.x));
  r = NK_MIN(r, ((b.y - a.y) < 0) ? -(b.y - a.y) : (b.y - a.y));

  if (r == 0.0f) {
    nk_draw_list_path_line_to(list, a);
    nk_draw_list_path_line_to(list, nk_vec2(b.x, a.y));
    nk_draw_list_path_line_to(list, b);
    nk_draw_list_path_line_to(list, nk_vec2(a.x, b.y));
  } else {
    nk_draw_list_path_arc_to_fast(list, nk_vec2(a.x + r, a.y + r), r, 6, 9);
    nk_draw_list_path_arc_to_fast(list, nk_vec2(b.x - r, a.y + r), r, 9, 12);
    nk_draw_list_path_arc_to_fast(list, nk_vec2(b.x - r, b.y - r), r, 0, 3);
    nk_draw_list_path_arc_to_fast(list, nk_vec2(a.x + r, b.y - r), r, 3, 6);
  }
}

NK_API void
nk_draw_list_path_curve_to(struct nk_draw_list *list, struct nk_vec2 p2, struct nk_vec2 p3, struct nk_vec2 p4, unsigned int num_segments)
{
  float          t_step;
  unsigned int   i_step;
  struct nk_vec2 p1;

  NK_ASSERT(list);
  NK_ASSERT(list->path_count);

  if (!list || !list->path_count) {
    return;
  }

  num_segments = NK_MAX(num_segments, 1);

  p1     = nk_draw_list_path_last(list);
  t_step = 1.0f / (float)num_segments;

  for (i_step = 1; i_step <= num_segments; ++i_step) {
    float t  = t_step * (float)i_step;
    float u  = 1.0f - t;
    float w1 = u * u * u;
    float w2 = 3 * u * u * t;
    float w3 = 3 * u * t * t;
    float w4 = t * t * t;
    float x  = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x;
    float y  = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y;

    nk_draw_list_path_line_to(list, nk_vec2(x, y));
  }
}

NK_API void
nk_draw_list_path_fill(struct nk_draw_list *list, struct nk_color color)
{
  struct nk_vec2 *points;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  points = (struct nk_vec2 *)nk_buffer_memory(list->buffer);
  nk_draw_list_fill_poly_convex(list, points, list->path_count, color, list->config.shape_AA);
  nk_draw_list_path_clear(list);
}

NK_API void
nk_draw_list_path_stroke(struct nk_draw_list *list, struct nk_color color, enum nk_draw_list_stroke closed, float thickness)
{
  struct nk_vec2 *points;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  points = (struct nk_vec2 *)nk_buffer_memory(list->buffer);
  nk_draw_list_stroke_poly_line(list, points, list->path_count, color, closed, thickness, list->config.line_AA);
  nk_draw_list_path_clear(list);
}

NK_API void
nk_draw_list_stroke_line(struct nk_draw_list *list, struct nk_vec2 a, struct nk_vec2 b, struct nk_color col, float thickness)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  if (list->line_AA == NK_ANTI_ALIASING_ON) {
    nk_draw_list_path_line_to(list, a);
    nk_draw_list_path_line_to(list, b);
  } else {
    nk_draw_list_path_line_to(list, nk_vec2_sub(a, nk_vec2(0.5f, 0.5f)));
    nk_draw_list_path_line_to(list, nk_vec2_sub(b, nk_vec2(0.5f, 0.5f)));
  }

  nk_draw_list_path_stroke(list, col, NK_STROKE_OPEN, thickness);
}

NK_API void
nk_draw_list_fill_rect(struct nk_draw_list *list, struct nk_rect rect, struct nk_color col, float rounding)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  if (list->line_AA == NK_ANTI_ALIASING_ON) {
    nk_draw_list_path_rect_to(list, nk_vec2(rect.x, rect.y), nk_vec2(rect.x + rect.w, rect.y + rect.h), rounding);
  } else {
    nk_draw_list_path_rect_to(list, nk_vec2(rect.x - 0.5f, rect.y - 0.5f), nk_vec2(rect.x + rect.w, rect.y + rect.h), rounding);
  }

  nk_draw_list_path_fill(list, col);
}

NK_API void
nk_draw_list_stroke_rect(struct nk_draw_list *list, struct nk_rect rect, struct nk_color col, float rounding, float thickness)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  if (list->line_AA == NK_ANTI_ALIASING_ON) {
    nk_draw_list_path_rect_to(list, nk_vec2(rect.x, rect.y), nk_vec2(rect.x + rect.w, rect.y + rect.h), rounding);
  } else {
    nk_draw_list_path_rect_to(list, nk_vec2(rect.x - 0.5f, rect.y - 0.5f), nk_vec2(rect.x + rect.w, rect.y + rect.h), rounding);
  }

  nk_draw_list_path_stroke(list, col, NK_STROKE_CLOSED, thickness);
}

NK_API void
nk_draw_list_fill_rect_multi_color(struct nk_draw_list *list, struct nk_rect rect, struct nk_color left, struct nk_color top, struct nk_color right, struct nk_color bottom)
{
  void            *vtx;
  struct nk_colorf col_left, col_top;
  struct nk_colorf col_right, col_bottom;
  nk_draw_index   *idx;
  nk_draw_index    index;

  nk_color_fv(&col_left.r, left);
  nk_color_fv(&col_right.r, right);
  nk_color_fv(&col_top.r, top);
  nk_color_fv(&col_bottom.r, bottom);

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  nk_draw_list_push_image(list, list->config.tex_null.texture);
  index = (nk_draw_index)list->vertex_count;
  vtx   = nk_draw_list_alloc_vertices(list, 4);
  idx   = nk_draw_list_alloc_elements(list, 6);

  if (!vtx || !idx) {
    return;
  }

  idx[0] = (nk_draw_index)(index + 0);
  idx[1] = (nk_draw_index)(index + 1);
  idx[2] = (nk_draw_index)(index + 2);
  idx[3] = (nk_draw_index)(index + 0);
  idx[4] = (nk_draw_index)(index + 2);
  idx[5] = (nk_draw_index)(index + 3);

  vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(rect.x, rect.y), list->config.tex_null.uv, col_left);
  vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(rect.x + rect.w, rect.y), list->config.tex_null.uv, col_top);
  vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(rect.x + rect.w, rect.y + rect.h), list->config.tex_null.uv, col_right);
  vtx = nk_draw_vertex(vtx, &list->config, nk_vec2(rect.x, rect.y + rect.h), list->config.tex_null.uv, col_bottom);
}

NK_API void
nk_draw_list_fill_triangle(struct nk_draw_list *list, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, struct nk_color col)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  nk_draw_list_path_line_to(list, a);
  nk_draw_list_path_line_to(list, b);
  nk_draw_list_path_line_to(list, c);
  nk_draw_list_path_fill(list, col);
}

NK_API void
nk_draw_list_stroke_triangle(struct nk_draw_list *list, struct nk_vec2 a, struct nk_vec2 b, struct nk_vec2 c, struct nk_color col, float thickness)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  nk_draw_list_path_line_to(list, a);
  nk_draw_list_path_line_to(list, b);
  nk_draw_list_path_line_to(list, c);
  nk_draw_list_path_stroke(list, col, NK_STROKE_CLOSED, thickness);
}

NK_API void
nk_draw_list_fill_circle(struct nk_draw_list *list, struct nk_vec2 center, float radius, struct nk_color col, unsigned int segs)
{
  float a_max;

  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  a_max = NK_PI * 2.0f * ((float)segs - 1.0f) / (float)segs;
  nk_draw_list_path_arc_to(list, center, radius, 0.0f, a_max, segs);
  nk_draw_list_path_fill(list, col);
}

NK_API void
nk_draw_list_stroke_circle(struct nk_draw_list *list, struct nk_vec2 center, float radius, struct nk_color col, unsigned int segs, float thickness)
{
  float a_max;

  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  a_max = NK_PI * 2.0f * ((float)segs - 1.0f) / (float)segs;
  nk_draw_list_path_arc_to(list, center, radius, 0.0f, a_max, segs);
  nk_draw_list_path_stroke(list, col, NK_STROKE_CLOSED, thickness);
}

NK_API void
nk_draw_list_stroke_curve(
  struct nk_draw_list *list, struct nk_vec2 p0, struct nk_vec2 cp0, struct nk_vec2 cp1, struct nk_vec2 p1, struct nk_color col, unsigned int segments, float thickness)
{
  NK_ASSERT(list);

  if (!list || !col.a) {
    return;
  }

  nk_draw_list_path_line_to(list, p0);
  nk_draw_list_path_curve_to(list, cp0, cp1, p1, segments);
  nk_draw_list_path_stroke(list, col, NK_STROKE_OPEN, thickness);
}

NK_INTERN void
nk_draw_list_push_rect_uv(struct nk_draw_list *list, struct nk_vec2 a, struct nk_vec2 c, struct nk_vec2 uva, struct nk_vec2 uvc, struct nk_color color)
{
  void          *vtx;
  struct nk_vec2 uvb;
  struct nk_vec2 uvd;
  struct nk_vec2 b;
  struct nk_vec2 d;

  struct nk_colorf col;
  nk_draw_index   *idx;
  nk_draw_index    index;

  NK_ASSERT(list);

  if (!list) {
    return;
  }

  nk_color_fv(&col.r, color);
  uvb = nk_vec2(uvc.x, uva.y);
  uvd = nk_vec2(uva.x, uvc.y);
  b   = nk_vec2(c.x, a.y);
  d   = nk_vec2(a.x, c.y);

  index = (nk_draw_index)list->vertex_count;
  vtx   = nk_draw_list_alloc_vertices(list, 4);
  idx   = nk_draw_list_alloc_elements(list, 6);

  if (!vtx || !idx) {
    return;
  }

  idx[0] = (nk_draw_index)(index + 0);
  idx[1] = (nk_draw_index)(index + 1);
  idx[2] = (nk_draw_index)(index + 2);
  idx[3] = (nk_draw_index)(index + 0);
  idx[4] = (nk_draw_index)(index + 2);
  idx[5] = (nk_draw_index)(index + 3);

  vtx = nk_draw_vertex(vtx, &list->config, a, uva, col);
  vtx = nk_draw_vertex(vtx, &list->config, b, uvb, col);
  vtx = nk_draw_vertex(vtx, &list->config, c, uvc, col);
  vtx = nk_draw_vertex(vtx, &list->config, d, uvd, col);
}

NK_API void
nk_draw_list_add_image(struct nk_draw_list *list, struct nk_image texture, struct nk_rect rect, struct nk_color color)
{
  NK_ASSERT(list);

  if (!list) {
    return;
  }

  /* push new command with given texture */
  nk_draw_list_push_image(list, texture.handle);

  if (nk_image_is_subimage(&texture)) {
    /* add region inside of the texture  */
    struct nk_vec2 uv[2];

    uv[0].x = (float)texture.region[0] / (float)texture.w;
    uv[0].y = (float)texture.region[1] / (float)texture.h;
    uv[1].x = (float)(texture.region[0] + texture.region[2]) / (float)texture.w;
    uv[1].y = (float)(texture.region[1] + texture.region[3]) / (float)texture.h;
    nk_draw_list_push_rect_uv(list, nk_vec2(rect.x, rect.y), nk_vec2(rect.x + rect.w, rect.y + rect.h), uv[0], uv[1], color);
  } else {
    nk_draw_list_push_rect_uv(list, nk_vec2(rect.x, rect.y), nk_vec2(rect.x + rect.w, rect.y + rect.h), nk_vec2(0.0f, 0.0f), nk_vec2(1.0f, 1.0f), color);
  }
}

/* Text vertices: 4 per glyph quad; the common vertex layout is written with SIMD (NK_NO_SIMD turns it off) */
#if !defined(NK_NO_SIMD) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#  include <emmintrin.h>
#  define NK_TEXT_SSE2
#elif !defined(NK_NO_SIMD) && (defined(__ARM_NEON) || defined(_M_ARM64))
#  include <arm_neon.h>
#  define NK_TEXT_NEON
#endif

struct nk_text_vertex_writer {
  nk_size pos_offset;
  nk_size color_offset;
  nk_bool packed;
};

NK_INTERN struct nk_text_vertex_writer
nk_text_vertex_writer_make(const struct nk_convert_config *config)
{
  struct nk_text_vertex_writer                writer;
  const struct nk_draw_vertex_layout_element *element;
  nk_size                                     uv_offset = 0;
  int                                         found     = 0;
  int                                         elements  = 0;

  NK_MEMSET(&writer, 0, sizeof(writer));

  for (element = config->vertex_layout; !nk_draw_vertex_layout_element_is_end_of_layout(element); ++element) {
    elements += 1;

    if (element->attribute == NK_VERTEX_POSITION && element->format == NK_FORMAT_FLOAT) {
      writer.pos_offset = element->offset;
      found            |= 1;
    } else if (element->attribute == NK_VERTEX_TEXCOORD && element->format == NK_FORMAT_FLOAT) {
      uv_offset = element->offset;
      found    |= 2;
    } else if (element->attribute == NK_VERTEX_COLOR && element->format == NK_FORMAT_R8G8B8A8) {
      writer.color_offset = element->offset;
      found              |= 4;
    }
  }

  writer.packed = found == 7 && elements == 3 && uv_offset == writer.pos_offset + 8 && config->vertex_alignment >= 4 &&
                  !(config->vertex_size % 4) && !(writer.pos_offset % 4) && !(writer.color_offset % 4);
  return writer;
}

NK_INTERN nk_byte *
nk_text_quads_write(nk_byte *vertex, const struct nk_convert_config *config, const struct nk_text_vertex_writer *writer,
                    const struct nk_text_geometry_quad *quad, int count, float x, float y, struct nk_color color, struct nk_colorf colorf)
{
  nk_size  stride = config->vertex_size;
  nk_byte *end    = vertex + stride * 4 * (nk_size)count;

  if (!writer->packed) {
    for (; vertex != end; ++quad) {
      struct nk_vec2 a = nk_vec2(x + quad->min.x, y + quad->min.y);
      struct nk_vec2 c = nk_vec2(x + quad->max.x, y + quad->max.y);

      vertex = (nk_byte *)nk_draw_vertex(vertex, config, a, quad->uv_min, colorf);
      vertex = (nk_byte *)nk_draw_vertex(vertex, config, nk_vec2(c.x, a.y), nk_vec2(quad->uv_max.x, quad->uv_min.y), colorf);
      vertex = (nk_byte *)nk_draw_vertex(vertex, config, c, quad->uv_max, colorf);
      vertex = (nk_byte *)nk_draw_vertex(vertex, config, nk_vec2(a.x, c.y), nk_vec2(quad->uv_min.x, quad->uv_max.y), colorf);
    }
    return vertex;
  }

  {
    nk_byte *pos = vertex + writer->pos_offset;
    nk_byte *col = vertex + writer->color_offset;
#if defined(NK_TEXT_SSE2)
    const __m128 offset = _mm_setr_ps(x, y, x, y);

    for (; count--; ++quad, pos += stride * 4) {
      /* p = (x0, y0, x1, y1), t = (u0, v0, u1, v1) */
      __m128 p = _mm_add_ps(_mm_loadu_ps(&quad->min.x), offset);
      __m128 t = _mm_loadu_ps(&quad->uv_min.x);

      _mm_storeu_ps((float *)(void *)(pos + stride * 0), _mm_movelh_ps(p, t));
      _mm_storeu_ps((float *)(void *)(pos + stride * 1), _mm_shuffle_ps(p, t, _MM_SHUFFLE(1, 2, 1, 2)));
      _mm_storeu_ps((float *)(void *)(pos + stride * 2), _mm_movehl_ps(t, p));
      _mm_storeu_ps((float *)(void *)(pos + stride * 3), _mm_shuffle_ps(p, t, _MM_SHUFFLE(3, 0, 3, 0)));
    }
#elif defined(NK_TEXT_NEON)
    float32x2_t       xy     = vset_lane_f32(y, vdup_n_f32(x), 1);
    const float32x4_t offset = vcombine_f32(xy, xy);

    for (; count--; ++quad, pos += stride * 4) {
      float32x4_t p     = vaddq_f32(vld1q_f32(&quad->min.x), offset);
      float32x4_t t     = vld1q_f32(&quad->uv_min.x);
      float32x2_t p_low = vget_low_f32(p), p_high = vget_high_f32(p);
      float32x2_t t_low = vget_low_f32(t), t_high = vget_high_f32(t);

      vst1q_f32((float *)(void *)pos, vcombine_f32(p_low, t_low));
      vst1q_f32((float *)(void *)(pos + stride), vcombine_f32(vset_lane_f32(vgetq_lane_f32(p, 2), p_low, 0), vset_lane_f32(vgetq_lane_f32(t, 2), t_low, 0)));
      vst1q_f32((float *)(void *)(pos + stride * 2), vcombine_f32(p_high, t_high));
      vst1q_f32((float *)(void *)(pos + stride * 3), vcombine_f32(vset_lane_f32(vgetq_lane_f32(p, 0), p_high, 0), vset_lane_f32(vgetq_lane_f32(t, 0), t_high, 0)));
    }
#else
    for (; count--; ++quad, pos += stride * 4) {
      float  x0 = x + quad->min.x, y0 = y + quad->min.y, x1 = x + quad->max.x, y1 = y + quad->max.y;
      float *v  = (float *)(void *)pos;

      v[0] = x0, v[1] = y0, v[2] = quad->uv_min.x, v[3] = quad->uv_min.y;
      v    = (float *)(void *)(pos + stride);
      v[0] = x1, v[1] = y0, v[2] = quad->uv_max.x, v[3] = quad->uv_min.y;
      v    = (float *)(void *)(pos + stride * 2);
      v[0] = x1, v[1] = y1, v[2] = quad->uv_max.x, v[3] = quad->uv_max.y;
      v    = (float *)(void *)(pos + stride * 3);
      v[0] = x0, v[1] = y1, v[2] = quad->uv_min.x, v[3] = quad->uv_max.y;
    }
#endif
    for (; col < end; col += stride) {
      *(struct nk_color *)(void *)col = color;
    }
  }
  return end;
}

/* two triangles per quad */
NK_INTERN void
nk_text_quad_indices(nk_draw_index *indices, nk_draw_index first, int quads)
{
#if defined(NK_TEXT_SSE2)
  if (sizeof(nk_draw_index) == 4) {
    __m128i a    = _mm_add_epi32(_mm_set1_epi32((int)first), _mm_setr_epi32(0, 1, 2, 0));
    __m128i b    = _mm_add_epi32(_mm_set1_epi32((int)first), _mm_setr_epi32(2, 3, 0, 0));
    __m128i step = _mm_set1_epi32(4);

    for (; quads--; indices += 6) {
      _mm_storeu_si128((__m128i *)(void *)indices, a);
      _mm_storel_epi64((__m128i *)(void *)(indices + 4), b);
      a = _mm_add_epi32(a, step);
      b = _mm_add_epi32(b, step);
    }
    return;
  }
#endif
  for (; quads--; indices += 6, first = (nk_draw_index)(first + 4)) {
    indices[0] = first;
    indices[1] = (nk_draw_index)(first + 1);
    indices[2] = (nk_draw_index)(first + 2);
    indices[3] = first;
    indices[4] = (nk_draw_index)(first + 2);
    indices[5] = (nk_draw_index)(first + 3);
  }
}

/* NULL when the buffers are full */
NK_INTERN nk_byte *
nk_text_quads_begin(struct nk_draw_list *list, nk_handle texture, int quads)
{
  nk_draw_index  first;
  nk_byte       *vertices;
  nk_draw_index *indices;

  nk_draw_list_push_image(list, texture);
  first    = (nk_draw_index)list->vertex_count;
  vertices = (nk_byte *)nk_draw_list_alloc_vertices(list, (nk_size)quads * 4);
  indices  = vertices ? nk_draw_list_alloc_elements(list, (nk_size)quads * 6) : NULL;

  if (!indices) {
    return NULL;
  }

  nk_text_quad_indices(indices, first, quads);
  return vertices;
}

NK_INTERN nk_bool
nk_text_draw_visible(const struct nk_draw_list *list, struct nk_rect rect)
{
  return NK_INTERSECT(rect.x, rect.y, rect.w, rect.h, list->clip_rect.x, list->clip_rect.y, list->clip_rect.w, list->clip_rect.h);
}

NK_INTERN void
nk_draw_list_add_text_geometry(struct nk_draw_list *list, const struct nk_text_geometry *geometry, struct nk_rect rect, struct nk_color fg)
{
  struct nk_text_vertex_writer writer = nk_text_vertex_writer_make(&list->config);
  struct nk_colorf             colorf;
  int                          run_idx = 0;

  if (!nk_text_draw_visible(list, rect)) {
    return;
  }

  fg.a = (nk_byte)((float)fg.a * list->config.global_alpha);
  nk_color_fv(&colorf.r, fg);

  while (run_idx < geometry->num_runs) {
    nk_handle texture = geometry->runs[run_idx].texture;
    int       end     = run_idx;
    int       quads   = 0;
    nk_byte  *vertex;

    while (end < geometry->num_runs && geometry->runs[end].texture.ptr == texture.ptr) {
      quads += geometry->runs[end].quad_count;
      end   += 1;
    }

    vertex = quads ? nk_text_quads_begin(list, texture, quads) : NULL;

    for (; vertex && run_idx < end; ++run_idx) {
      const struct nk_text_geometry_run *run = &geometry->runs[run_idx];

      vertex = nk_text_quads_write(vertex, &list->config, &writer, geometry->quads + run->quad_begin, run->quad_count, rect.x, rect.y, fg, colorf);
    }

    if (!vertex && quads) {
      return;
    }

    run_idx = end;
  }
}

NK_API void
nk_draw_list_add_text_shape(struct nk_draw_list *list, const struct nk_text_shape *shape, struct nk_rect rect, float font_height, struct nk_color fg)
{
  struct nk_text_vertex_writer writer = nk_text_vertex_writer_make(&list->config);
  struct nk_colorf             colorf;
  struct nk_vec2               pen = nk_vec2(0.0f, 0.0f);
  int                          run_idx;

  NK_ASSERT(list);
  NK_ASSERT(shape);

  if (!nk_text_draw_visible(list, rect)) {
    return;
  }

  fg.a = (nk_byte)((float)fg.a * list->config.global_alpha);
  nk_color_fv(&colorf.r, fg);

  for (run_idx = 0; run_idx < shape->num_runs; ++run_idx) {
    const struct nk_text_run *run    = &shape->runs[run_idx];
    nk_byte                  *vertex = run->glyph_count ? nk_text_quads_begin(list, run->font->texture, run->glyph_count) : NULL;
    int                       glyph_idx;

    if (!vertex) {
      if (run->glyph_count) {
        return;
      }
      continue;
    }

    for (glyph_idx = run->glyph_begin; glyph_idx < run->glyph_begin + run->glyph_count; ++glyph_idx) {
      const struct nk_text_glyph  *glyph = &shape->glyphs[glyph_idx];
      struct nk_user_font_glyph    g;
      struct nk_text_geometry_quad quad;

      NK_PROFILE_SCOPE_BEGIN("text.glyph_query", glyph_query);

      {
        run->font->query_glyph(run->font->userdata, font_height, &g, glyph->id);
      }

      NK_PROFILE_SCOPE_END(glyph_query);

      quad.min    = nk_vec2(pen.x + glyph->offset_x + g.offset.x, pen.y + shape->baseline + glyph->offset_y + g.offset.y);
      quad.max    = nk_vec2(quad.min.x + g.width, quad.min.y + g.height);
      quad.uv_min = g.uv[0];
      quad.uv_max = g.uv[1];
      vertex      = nk_text_quads_write(vertex, &list->config, &writer, &quad, 1, rect.x, rect.y, fg, colorf);

      pen.x += glyph->advance_x;
      pen.y += glyph->advance_y;
    }
  }
}

NK_API nk_flags
nk_convert(struct nk_context *ctx, struct nk_buffer *cmds, struct nk_buffer *vertices, struct nk_buffer *elements, const struct nk_convert_config *config)
{
  nk_flags                 res = NK_CONVERT_SUCCESS;
  const struct nk_command *cmd;

  NK_ASSERT(ctx);
  NK_ASSERT(cmds);
  NK_ASSERT(vertices);
  NK_ASSERT(elements);
  NK_ASSERT(config);
  NK_ASSERT(config->vertex_layout);
  NK_ASSERT(config->vertex_size);

  if (!ctx || !cmds || !vertices || !elements || !config || !config->vertex_layout) {
    return NK_CONVERT_INVALID_PARAM;
  }

  nk_draw_list_setup(&ctx->draw_list, config, cmds, vertices, elements, config->line_AA, config->shape_AA);
  nk_foreach(cmd, ctx)
  {
#  ifdef NK_INCLUDE_COMMAND_USERDATA
    ctx->draw_list.userdata = cmd->userdata;
#  endif
    switch (cmd->type) {
      case NK_COMMAND_NOP:
        break;
      case NK_COMMAND_SCISSOR: {
        const struct nk_command_scissor *s = (const struct nk_command_scissor *)cmd;
        nk_draw_list_add_clip(&ctx->draw_list, nk_rect(s->x, s->y, s->w, s->h));
      } break;
      case NK_COMMAND_LINE: {
        const struct nk_command_line *l = (const struct nk_command_line *)cmd;
        nk_draw_list_stroke_line(&ctx->draw_list, nk_vec2(l->begin.x, l->begin.y), nk_vec2(l->end.x, l->end.y), l->color, l->line_thickness);
      } break;
      case NK_COMMAND_CURVE: {
        const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
        nk_draw_list_stroke_curve(&ctx->draw_list,
                                  nk_vec2(q->begin.x, q->begin.y),
                                  nk_vec2(q->ctrl[0].x, q->ctrl[0].y),
                                  nk_vec2(q->ctrl[1].x, q->ctrl[1].y),
                                  nk_vec2(q->end.x, q->end.y),
                                  q->color,
                                  config->curve_segment_count,
                                  q->line_thickness);
      } break;
      case NK_COMMAND_RECT: {
        const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
        nk_draw_list_stroke_rect(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h), r->color, (float)r->rounding, r->line_thickness);
      } break;
      case NK_COMMAND_RECT_FILLED: {
        const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
        nk_draw_list_fill_rect(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h), r->color, (float)r->rounding);
      } break;
      case NK_COMMAND_RECT_MULTI_COLOR: {
        const struct nk_command_rect_multi_color *r = (const struct nk_command_rect_multi_color *)cmd;
        nk_draw_list_fill_rect_multi_color(&ctx->draw_list, nk_rect(r->x, r->y, r->w, r->h), r->left, r->top, r->right, r->bottom);
      } break;
      case NK_COMMAND_CIRCLE: {
        const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
        nk_draw_list_stroke_circle(
          &ctx->draw_list, nk_vec2((float)c->x + (float)c->w / 2, (float)c->y + (float)c->h / 2), (float)c->w / 2, c->color, config->circle_segment_count, c->line_thickness);
      } break;
      case NK_COMMAND_CIRCLE_FILLED: {
        const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
        nk_draw_list_fill_circle(&ctx->draw_list, nk_vec2((float)c->x + (float)c->w / 2, (float)c->y + (float)c->h / 2), (float)c->w / 2, c->color, config->circle_segment_count);
      } break;
      case NK_COMMAND_ARC: {
        const struct nk_command_arc *c = (const struct nk_command_arc *)cmd;
        nk_draw_list_path_line_to(&ctx->draw_list, nk_vec2(c->cx, c->cy));
        nk_draw_list_path_arc_to(&ctx->draw_list, nk_vec2(c->cx, c->cy), c->r, c->a[0], c->a[1], config->arc_segment_count);
        nk_draw_list_path_stroke(&ctx->draw_list, c->color, NK_STROKE_CLOSED, c->line_thickness);
      } break;
      case NK_COMMAND_ARC_FILLED: {
        const struct nk_command_arc_filled *c = (const struct nk_command_arc_filled *)cmd;
        nk_draw_list_path_line_to(&ctx->draw_list, nk_vec2(c->cx, c->cy));
        nk_draw_list_path_arc_to(&ctx->draw_list, nk_vec2(c->cx, c->cy), c->r, c->a[0], c->a[1], config->arc_segment_count);
        nk_draw_list_path_fill(&ctx->draw_list, c->color);
      } break;
      case NK_COMMAND_TRIANGLE: {
        const struct nk_command_triangle *t = (const struct nk_command_triangle *)cmd;
        nk_draw_list_stroke_triangle(&ctx->draw_list, nk_vec2(t->a.x, t->a.y), nk_vec2(t->b.x, t->b.y), nk_vec2(t->c.x, t->c.y), t->color, t->line_thickness);
      } break;
      case NK_COMMAND_TRIANGLE_FILLED: {
        const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
        nk_draw_list_fill_triangle(&ctx->draw_list, nk_vec2(t->a.x, t->a.y), nk_vec2(t->b.x, t->b.y), nk_vec2(t->c.x, t->c.y), t->color);
      } break;
      case NK_COMMAND_POLYGON: {
        int                              i;
        const struct nk_command_polygon *p = (const struct nk_command_polygon *)cmd;
        for (i = 0; i < p->point_count; ++i) {
          struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
          nk_draw_list_path_line_to(&ctx->draw_list, pnt);
        }
        nk_draw_list_path_stroke(&ctx->draw_list, p->color, NK_STROKE_CLOSED, p->line_thickness);
      } break;
      case NK_COMMAND_POLYGON_FILLED: {
        int                                     i;
        const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled *)cmd;
        for (i = 0; i < p->point_count; ++i) {
          struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
          nk_draw_list_path_line_to(&ctx->draw_list, pnt);
        }
        nk_draw_list_path_fill(&ctx->draw_list, p->color);
      } break;
      case NK_COMMAND_POLYLINE: {
        int                               i;
        const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
        for (i = 0; i < p->point_count; ++i) {
          struct nk_vec2 pnt = nk_vec2((float)p->points[i].x, (float)p->points[i].y);
          nk_draw_list_path_line_to(&ctx->draw_list, pnt);
        }
        nk_draw_list_path_stroke(&ctx->draw_list, p->color, NK_STROKE_OPEN, p->line_thickness);
      } break;
      case NK_COMMAND_TEXT: {
        const struct nk_command_text *t       = (const struct nk_command_text *)cmd;
        struct nk_text_request        request = {0};
        const struct nk_text_geometry *geometry;
        const struct nk_text_shape    *shape = NULL;

        request.source      = t->string;
        request.source_len  = t->length;
        request.font        = t->font;
        request.font_height = t->height;

        NK_PROFILE_SCOPE_BEGIN("text.geometry", geometry_prepare);
        {
          geometry = nk_text_geometry_get(ctx, &request, &shape);
        }
        NK_PROFILE_SCOPE_END(geometry_prepare);

        if (geometry) {
          NK_PROFILE_SCOPE_BEGIN("text.geometry_cached", geometry_cached);
          {
            nk_draw_list_add_text_geometry(&ctx->draw_list, geometry, nk_rect(t->x, t->y, t->w, t->h), t->foreground);
          }
          NK_PROFILE_SCOPE_END(geometry_cached);
        } else if (shape) {
          NK_PROFILE_SCOPE_BEGIN("text.geometry_uncached", geometry_uncached);
          {
            nk_draw_list_add_text_shape(&ctx->draw_list, shape, nk_rect(t->x, t->y, t->w, t->h), t->height, t->foreground);
          }
          NK_PROFILE_SCOPE_END(geometry_uncached);
        }
      } break;
      case NK_COMMAND_IMAGE: {
        const struct nk_command_image *i = (const struct nk_command_image *)cmd;
        nk_draw_list_add_image(&ctx->draw_list, i->img, nk_rect(i->x, i->y, i->w, i->h), i->col);
      } break;
      case NK_COMMAND_CUSTOM: {
        const struct nk_command_custom *c = (const struct nk_command_custom *)cmd;
        c->callback(&ctx->draw_list, c->x, c->y, c->w, c->h, c->callback_data);
      } break;
      default:
        break;
    }
  }
  res |= (cmds->needed > cmds->allocated + (cmds->memory.size - cmds->size)) ? NK_CONVERT_COMMAND_BUFFER_FULL : 0;
  res |= (vertices->needed > vertices->allocated) ? NK_CONVERT_VERTEX_BUFFER_FULL : 0;
  res |= (elements->needed > elements->allocated) ? NK_CONVERT_ELEMENT_BUFFER_FULL : 0;
  return res;
}

NK_API const struct nk_draw_command *
nk__draw_begin(const struct nk_context *ctx, const struct nk_buffer *buffer)
{
  return nk__draw_list_begin(&ctx->draw_list, buffer);
}

NK_API const struct nk_draw_command *
nk__draw_end(const struct nk_context *ctx, const struct nk_buffer *buffer)
{
  return nk__draw_list_end(&ctx->draw_list, buffer);
}

NK_API const struct nk_draw_command *
nk__draw_next(const struct nk_draw_command *cmd, const struct nk_buffer *buffer, const struct nk_context *ctx)
{
  return nk__draw_list_next(cmd, buffer, &ctx->draw_list);
}
#endif

#ifdef NK_INCLUDE_GLYPH_ATLAS
/* NK_NO_STB_TRUETYPE_IMPLEMENTATION: stb_truetype is implemented elsewhere */
#  ifndef NK_NO_STB_TRUETYPE_IMPLEMENTATION
#    define STB_TRUETYPE_IMPLEMENTATION
#    define STBTT_STATIC
#  endif

/* Allow consumer to define own STBTT_malloc/STBTT_free, and use the glyph atlas' scratch allocator otherwise */
#  ifndef STBTT_malloc
static void *
nk_stbtt_malloc(nk_size size, void *user_data)
{
  struct nk_allocator *alloc = (struct nk_allocator *)user_data;
  return alloc->alloc(alloc->userdata, 0, size);
}

static void
nk_stbtt_free(void *ptr, void *user_data)
{
  struct nk_allocator *alloc = (struct nk_allocator *)user_data;

  alloc->free(alloc->userdata, ptr);
}

#    define STBTT_malloc(x, u) nk_stbtt_malloc(x, u)
#    define STBTT_free(x, u)   nk_stbtt_free(x, u)

#  endif /* STBTT_malloc */
#endif

/*  stb_truetype.h - v1.26 - public domain */
/*  authored from 2009-2021 by Sean Barrett / RAD Game Tools; license at the end of the file */
/*  */
/*  Trimmed for Nuklear's glyph atlas: parsing TrueType/OpenType (CFF) faces and rendering */
/*  glyphs by glyph index. Character mapping, kerning, packing, baking, SDF and SVG are gone; */
/*  the text shaper maps characters to glyphs and positions them. */
/*  */
/*     NO SECURITY GUARANTEE -- DO NOT USE THIS ON UNTRUSTED FONT FILES */
/*  */
/*  This library does no range checking of the offsets found in the file, */
/*  meaning an attacker can use it to read arbitrary memory. */

#ifdef STB_TRUETYPE_IMPLEMENTATION
/*  #define your own (u)stbtt_int8/16/32 before including to override this */
#  ifndef stbtt_uint8
typedef unsigned char  stbtt_uint8;
typedef signed char    stbtt_int8;
typedef unsigned short stbtt_uint16;
typedef signed short   stbtt_int16;
typedef unsigned int   stbtt_uint32;
typedef signed int     stbtt_int32;
#  endif

typedef char stbtt__check_size32[sizeof(stbtt_int32) == 4 ? 1 : -1];
typedef char stbtt__check_size16[sizeof(stbtt_int16) == 2 ? 1 : -1];

/*  e.g. #define your own STBTT_ifloor/STBTT_iceil() to avoid math.h */
#  ifndef STBTT_ifloor
#    include <math.h>
#    define STBTT_ifloor(x) ((int)floor(x))
#    define STBTT_iceil(x)  ((int)ceil(x))
#  endif

#  ifndef STBTT_sqrt
#    include <math.h>
#    define STBTT_sqrt(x) sqrt(x)
#  endif

#  ifndef STBTT_fabs
#    include <math.h>
#    define STBTT_fabs(x) fabs(x)
#  endif

/*  #define your own functions "STBTT_malloc" / "STBTT_free" to avoid malloc.h */
#  ifndef STBTT_malloc
#    include <stdlib.h>
#    define STBTT_malloc(x, u) ((void)(u), malloc(x))
#    define STBTT_free(x, u)   ((void)(u), free(x))
#  endif

#  ifndef STBTT_assert
#    include <assert.h>
#    define STBTT_assert(x) assert(x)
#  endif

#  ifndef STBTT_memcpy
#    include <string.h>
#    define STBTT_memcpy memcpy
#    define STBTT_memset memset
#  endif
#endif

/* ///////////////////////////////////////////////////////////////////////////// */
/* ///////////////////////////////////////////////////////////////////////////// */
/* // */
/* //   INTERFACE */
/* // */
/* // */

#ifndef __STB_INCLUDE_STB_TRUETYPE_H__
#  define __STB_INCLUDE_STB_TRUETYPE_H__

#  ifdef STBTT_STATIC
#    define STBTT_DEF static
#  else
#    define STBTT_DEF extern
#  endif

#  ifdef __cplusplus
extern "C" {
#  endif

/*  private structure */
typedef struct {
  unsigned char *data;
  int            cursor;
  int            size;
} stbtt__buf;

typedef struct stbtt_fontinfo stbtt_fontinfo;

/*  The offset of the index-th font in a file; for a single-font file only index 0 is valid. -1 on error. */
STBTT_DEF int
stbtt_GetFontOffsetForIndex(const unsigned char *data, int index);

/*  The following structure is defined publicly so you can declare one on */
/*  the stack or as a global or etc, but you should treat it as opaque. */
struct stbtt_fontinfo {
  void          *userdata;
  unsigned char *data;      /*  pointer to .ttf file */
  int            fontstart; /*  offset of start of font */

  int numGlyphs; /*  number of glyphs, needed for range checking */

  int loca, head, glyf, hhea, hmtx, kern, gpos, svg; /*  table locations as offset from start of .ttf */
  int index_map;                                     /*  a cmap mapping for our chosen character encoding */
  int indexToLocFormat;                              /*  format needed to map from glyph index to glyph */

  stbtt__buf cff;         /*  cff font data */
  stbtt__buf charstrings; /*  the charstring index */
  stbtt__buf gsubrs;      /*  global charstring subroutines index */
  stbtt__buf subrs;       /*  private charstring subroutines index */
  stbtt__buf fontdicts;   /*  array of font dicts */
  stbtt__buf fdselect;    /*  map from glyph to fontdict */
};

/*  Prepares a font for use; the data is not copied and must stay valid. 0 on failure. */
STBTT_DEF int
stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset);

/*  The scale for a font size in pixels per em, the meaning of CSS font sizes. */
STBTT_DEF float
stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels);

/*  Unscaled advance width and left side bearing of a glyph. */
STBTT_DEF void
stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing);

/*  Unscaled bounding box of a glyph; 0 for a glyph without outline. */
STBTT_DEF int
stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

#  ifndef STBTT_vmove /*  you can predefine these to use different values (but why?) */
enum { STBTT_vmove = 1, STBTT_vline, STBTT_vcurve, STBTT_vcubic };
#  endif

#  ifndef stbtt_vertex              /*  you can predefine this to use different values */
                                    /*  (we share this with other code at RAD) */
#    define stbtt_vertex_type short /*  can't use stbtt_int16 because that's not visible in the header file */
typedef struct {
  stbtt_vertex_type x, y, cx, cy, cx1, cy1;
  unsigned char     type, padding;
} stbtt_vertex;
#  endif

/*  The outline of a glyph as lines and curves; the vertices are allocated with STBTT_malloc. */
STBTT_DEF int
stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **vertices);

/*  Renders a glyph into a one-channel bitmap of out_w x out_h, sized with stbtt_GetGlyphBitmapBox. */
STBTT_DEF void
stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph);
STBTT_DEF void
stbtt_MakeGlyphBitmapSubpixel(
  const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph);

/*  The bitmap box of a glyph at a scale, relative to its origin on the baseline; y grows downwards. */
STBTT_DEF void
stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1);
STBTT_DEF void
stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1);

/*  @TODO: don't expose this structure */
typedef struct {
  int            w, h, stride;
  unsigned char *pixels;
} stbtt__bitmap;

/*  rasterize a shape with quadratic beziers into a bitmap */
STBTT_DEF void
stbtt_Rasterize(stbtt__bitmap *result,             /*  1-channel bitmap to draw into */
                float          flatness_in_pixels, /*  allowable error of curve in pixels */
                stbtt_vertex  *vertices,           /*  array of vertices defining shape */
                int            num_verts,          /*  number of vertices in above array */
                float          scale_x,
                float          scale_y, /*  scale applied to input vertices */
                float          shift_x,
                float          shift_y, /*  translation applied to input vertices */
                int            x_off,
                int            y_off,     /*  another translation applied to input */
                int            invert,    /*  if non-zero, vertically flip shape */
                void          *userdata); /*  context for to STBTT_MALLOC */

/*  cmap platform and encoding IDs stbtt_InitFont looks for */
enum { /*  platformID */
       STBTT_PLATFORM_ID_UNICODE   = 0,
       STBTT_PLATFORM_ID_MAC       = 1,
       STBTT_PLATFORM_ID_ISO       = 2,
       STBTT_PLATFORM_ID_MICROSOFT = 3
};

enum { /*  encodingID for STBTT_PLATFORM_ID_MICROSOFT */
       STBTT_MS_EID_SYMBOL       = 0,
       STBTT_MS_EID_UNICODE_BMP  = 1,
       STBTT_MS_EID_SHIFTJIS     = 2,
       STBTT_MS_EID_UNICODE_FULL = 10
};

#  ifdef __cplusplus
}
#  endif

#endif /*  __STB_INCLUDE_STB_TRUETYPE_H__ */

/* ///////////////////////////////////////////////////////////////////////////// */
/* ///////////////////////////////////////////////////////////////////////////// */
/* // */
/* //   IMPLEMENTATION */
/* // */
/* // */

#ifdef STB_TRUETYPE_IMPLEMENTATION

#  ifndef STBTT_RASTERIZER_VERSION
#    define STBTT_RASTERIZER_VERSION 2
#  endif

#  ifdef _MSC_VER
#    define STBTT__NOTUSED(v) (void)(v)
#  else
#    define STBTT__NOTUSED(v) (void)sizeof(v)
#  endif

/* //////////////////////////////////////////////////////////////////////// */
/*  */
/*  stbtt__buf helpers to parse data from file */
/*  */

static stbtt_uint8
stbtt__buf_get8(stbtt__buf *b)
{
  if (b->cursor >= b->size) {
    return 0;
  }
  return b->data[b->cursor++];
}

static stbtt_uint8
stbtt__buf_peek8(stbtt__buf *b)
{
  if (b->cursor >= b->size) {
    return 0;
  }
  return b->data[b->cursor];
}

static void
stbtt__buf_seek(stbtt__buf *b, int o)
{
  STBTT_assert(!(o > b->size || o < 0));
  b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void
stbtt__buf_skip(stbtt__buf *b, int o)
{
  stbtt__buf_seek(b, b->cursor + o);
}

static stbtt_uint32
stbtt__buf_get(stbtt__buf *b, int n)
{
  stbtt_uint32 v = 0;
  int          i;
  STBTT_assert(n >= 1 && n <= 4);
  for (i = 0; i < n; i++) {
    v = (v << 8) | stbtt__buf_get8(b);
  }
  return v;
}

static stbtt__buf
stbtt__new_buf(const void *p, size_t size)
{
  stbtt__buf r;
  STBTT_assert(size < 0x40000000);
  r.data   = (stbtt_uint8 *)p;
  r.size   = (int)size;
  r.cursor = 0;
  return r;
}

#  define stbtt__buf_get16(b) stbtt__buf_get((b), 2)
#  define stbtt__buf_get32(b) stbtt__buf_get((b), 4)

static stbtt__buf
stbtt__buf_range(const stbtt__buf *b, int o, int s)
{
  stbtt__buf r = stbtt__new_buf(NULL, 0);
  if (o < 0 || s < 0 || o > b->size || s > b->size - o) {
    return r;
  }
  r.data = b->data + o;
  r.size = s;
  return r;
}

static stbtt__buf
stbtt__cff_get_index(stbtt__buf *b)
{
  int count, start, offsize;
  start = b->cursor;
  count = stbtt__buf_get16(b);
  if (count) {
    offsize = stbtt__buf_get8(b);
    STBTT_assert(offsize >= 1 && offsize <= 4);
    stbtt__buf_skip(b, offsize * count);
    stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
  }
  return stbtt__buf_range(b, start, b->cursor - start);
}

static stbtt_uint32
stbtt__cff_int(stbtt__buf *b)
{
  int b0 = stbtt__buf_get8(b);
  if (b0 >= 32 && b0 <= 246) {
    return b0 - 139;
  } else if (b0 >= 247 && b0 <= 250) {
    return (b0 - 247) * 256 + stbtt__buf_get8(b) + 108;
  } else if (b0 >= 251 && b0 <= 254) {
    return -(b0 - 251) * 256 - stbtt__buf_get8(b) - 108;
  } else if (b0 == 28) {
    return stbtt__buf_get16(b);
  } else if (b0 == 29) {
    return stbtt__buf_get32(b);
  }
  STBTT_assert(0);
  return 0;
}

static void
stbtt__cff_skip_operand(stbtt__buf *b)
{
  int v, b0 = stbtt__buf_peek8(b);
  STBTT_assert(b0 >= 28);
  if (b0 == 30) {
    stbtt__buf_skip(b, 1);
    while (b->cursor < b->size) {
      v = stbtt__buf_get8(b);
      if ((v & 0xF) == 0xF || (v >> 4) == 0xF) {
        break;
      }
    }
  } else {
    stbtt__cff_int(b);
  }
}

static stbtt__buf
stbtt__dict_get(stbtt__buf *b, int key)
{
  stbtt__buf_seek(b, 0);
  while (b->cursor < b->size) {
    int start = b->cursor, end, op;
    while (stbtt__buf_peek8(b) >= 28) {
      stbtt__cff_skip_operand(b);
    }
    end = b->cursor;
    op  = stbtt__buf_get8(b);
    if (op == 12) {
      op = stbtt__buf_get8(b) | 0x100;
    }

    if (op == key) {
      return stbtt__buf_range(b, start, end - start);
    }
  }
  return stbtt__buf_range(b, 0, 0);
}

static void
stbtt__dict_get_ints(stbtt__buf *b, int key, int outcount, stbtt_uint32 *out)
{
  int        i;
  stbtt__buf operands = stbtt__dict_get(b, key);
  for (i = 0; i < outcount && operands.cursor < operands.size; i++) {
    out[i] = stbtt__cff_int(&operands);
  }
}

static int
stbtt__cff_index_count(stbtt__buf *b)
{
  stbtt__buf_seek(b, 0);
  return stbtt__buf_get16(b);
}

static stbtt__buf
stbtt__cff_index_get(stbtt__buf b, int i)
{
  int count, offsize, start, end;
  stbtt__buf_seek(&b, 0);
  count   = stbtt__buf_get16(&b);
  offsize = stbtt__buf_get8(&b);
  STBTT_assert(i >= 0 && i < count);
  STBTT_assert(offsize >= 1 && offsize <= 4);
  stbtt__buf_skip(&b, i * offsize);
  start = stbtt__buf_get(&b, offsize);
  end   = stbtt__buf_get(&b, offsize);
  return stbtt__buf_range(&b, 2 + (count + 1) * offsize + start, end - start);
}

/* //////////////////////////////////////////////////////////////////////// */
/*  */
/*  accessors to parse data from file */
/*  */

/*  on platforms that don't allow misaligned reads, if we want to allow */
/*  truetype fonts that aren't padded to alignment, define ALLOW_UNALIGNED_TRUETYPE */

#  define ttBYTE(p)  (*(stbtt_uint8 *)(p))
#  define ttCHAR(p)  (*(stbtt_int8 *)(p))
#  define ttFixed(p) ttLONG(p)

static stbtt_uint16
ttUSHORT(stbtt_uint8 *p)
{
  return p[0] * 256 + p[1];
}
static stbtt_int16
ttSHORT(stbtt_uint8 *p)
{
  return p[0] * 256 + p[1];
}
static stbtt_uint32
ttULONG(stbtt_uint8 *p)
{
  return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}
static stbtt_int32
ttLONG(stbtt_uint8 *p)
{
  return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

#  define stbtt_tag4(p, c0, c1, c2, c3) ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#  define stbtt_tag(p, str)             stbtt_tag4(p, str[0], str[1], str[2], str[3])

static int
stbtt__isfont(stbtt_uint8 *font)
{
  /*  check the version number */
  if (stbtt_tag4(font, '1', 0, 0, 0)) {
    return 1; /*  TrueType 1 */
  }

  if (stbtt_tag(font, "typ1")) {
    return 1; /*  TrueType with type 1 font -- we don't support this! */
  }

  if (stbtt_tag(font, "OTTO")) {
    return 1; /*  OpenType with CFF */
  }

  if (stbtt_tag4(font, 0, 1, 0, 0)) {
    return 1; /*  OpenType 1.0 */
  }

  if (stbtt_tag(font, "true")) {
    return 1; /*  Apple specification for TrueType fonts */
  }
  return 0;
}

/*  @OPTIMIZE: binary search */
static stbtt_uint32
stbtt__find_table(stbtt_uint8 *data, stbtt_uint32 fontstart, const char *tag)
{
  stbtt_int32  num_tables = ttUSHORT(data + fontstart + 4);
  stbtt_uint32 tabledir   = fontstart + 12;
  stbtt_int32  i;
  for (i = 0; i < num_tables; ++i) {
    stbtt_uint32 loc = tabledir + 16 * i;
    if (stbtt_tag(data + loc + 0, tag)) {
      return ttULONG(data + loc + 8);
    }
  }
  return 0;
}

static int
stbtt_GetFontOffsetForIndex_internal(unsigned char *font_collection, int index)
{
  /*  if it's just a font, there's only one valid index */
  if (stbtt__isfont(font_collection)) {
    return index == 0 ? 0 : -1;
  }

  /*  check if it's a TTC */
  if (stbtt_tag(font_collection, "ttcf")) {
    /*  version 1? */
    if (ttULONG(font_collection + 4) == 0x00010000 || ttULONG(font_collection + 4) == 0x00020000) {
      stbtt_int32 n = ttLONG(font_collection + 8);
      if (index >= n) {
        return -1;
      }
      return ttULONG(font_collection + 12 + index * 4);
    }
  }
  return -1;
}

static stbtt__buf
stbtt__get_subrs(stbtt__buf cff, stbtt__buf fontdict)
{
  stbtt_uint32 subrsoff = 0, private_loc[2] = {0, 0};
  stbtt__buf   pdict;
  stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
  if (!private_loc[1] || !private_loc[0]) {
    return stbtt__new_buf(NULL, 0);
  }
  pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
  stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
  if (!subrsoff) {
    return stbtt__new_buf(NULL, 0);
  }
  stbtt__buf_seek(&cff, private_loc[1] + subrsoff);
  return stbtt__cff_get_index(&cff);
}

static int
stbtt_InitFont_internal(stbtt_fontinfo *info, unsigned char *data, int fontstart)
{
  stbtt_uint32 cmap, t;
  stbtt_int32  i, numTables;

  info->data      = data;
  info->fontstart = fontstart;
  info->cff       = stbtt__new_buf(NULL, 0);

  cmap       = stbtt__find_table(data, fontstart, "cmap"); /*  required */
  info->loca = stbtt__find_table(data, fontstart, "loca"); /*  required */
  info->head = stbtt__find_table(data, fontstart, "head"); /*  required */
  info->glyf = stbtt__find_table(data, fontstart, "glyf"); /*  required */
  info->hhea = stbtt__find_table(data, fontstart, "hhea"); /*  required */
  info->hmtx = stbtt__find_table(data, fontstart, "hmtx"); /*  required */
  info->kern = stbtt__find_table(data, fontstart, "kern"); /*  not required */
  info->gpos = stbtt__find_table(data, fontstart, "GPOS"); /*  not required */

  if (!cmap || !info->head || !info->hhea || !info->hmtx) {
    return 0;
  }

  if (info->glyf) {
    /*  required for truetype */
    if (!info->loca) {
      return 0;
    }
  } else {
    /*  initialization for CFF / Type2 fonts (OTF) */
    stbtt__buf   b, topdict, topdictidx;
    stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0, fdselectoff = 0;
    stbtt_uint32 cff;

    cff = stbtt__find_table(data, fontstart, "CFF ");
    if (!cff) {
      return 0;
    }

    info->fontdicts = stbtt__new_buf(NULL, 0);
    info->fdselect  = stbtt__new_buf(NULL, 0);

    /*  @TODO this should use size from table (not 512MB) */
    info->cff = stbtt__new_buf(data + cff, 512 * 1024 * 1024);
    b         = info->cff;

    /*  read the header */
    stbtt__buf_skip(&b, 2);
    stbtt__buf_seek(&b, stbtt__buf_get8(&b)); /*  hdrsize */

    /*  @TODO the name INDEX could list multiple fonts, */
    /*  but we just use the first one. */
    stbtt__cff_get_index(&b); /*  name INDEX */
    topdictidx = stbtt__cff_get_index(&b);
    topdict    = stbtt__cff_index_get(topdictidx, 0);
    stbtt__cff_get_index(&b); /*  string INDEX */
    info->gsubrs = stbtt__cff_get_index(&b);

    stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
    stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
    stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
    stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
    info->subrs = stbtt__get_subrs(b, topdict);

    /*  we only support Type 2 charstrings */
    if (cstype != 2) {
      return 0;
    }

    if (charstrings == 0) {
      return 0;
    }

    if (fdarrayoff) {
      /*  looks like a CID font */
      if (!fdselectoff) {
        return 0;
      }
      stbtt__buf_seek(&b, fdarrayoff);
      info->fontdicts = stbtt__cff_get_index(&b);
      info->fdselect  = stbtt__buf_range(&b, fdselectoff, b.size - fdselectoff);
    }

    stbtt__buf_seek(&b, charstrings);
    info->charstrings = stbtt__cff_get_index(&b);
  }

  t = stbtt__find_table(data, fontstart, "maxp");
  if (t) {
    info->numGlyphs = ttUSHORT(data + t + 4);
  } else {
    info->numGlyphs = 0xffff;
  }

  info->svg = -1;

  /*  find a cmap encoding table we understand *now* to avoid searching */
  /*  later. (todo: could make this installable) */
  /*  the same regardless of glyph. */
  numTables       = ttUSHORT(data + cmap + 2);
  info->index_map = 0;
  for (i = 0; i < numTables; ++i) {
    stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
    /*  find an encoding we understand: */
    switch (ttUSHORT(data + encoding_record)) {
    case STBTT_PLATFORM_ID_MICROSOFT:
      switch (ttUSHORT(data + encoding_record + 2)) {
      case STBTT_MS_EID_UNICODE_BMP:
      case STBTT_MS_EID_UNICODE_FULL:
        /*  MS/Unicode */
        info->index_map = cmap + ttULONG(data + encoding_record + 4);
        break;
      }
      break;
    case STBTT_PLATFORM_ID_UNICODE:
      /*  Mac/iOS has these */
      /*  all the encodingIDs are unicode, so we don't bother to check it */
      info->index_map = cmap + ttULONG(data + encoding_record + 4);
      break;
    }
  }

  if (info->index_map == 0) {
    return 0;
  }

  info->indexToLocFormat = ttUSHORT(data + info->head + 50);
  return 1;
}

static void
stbtt_setvertex(stbtt_vertex *v, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy)
{
  v->type = type;
  v->x    = (stbtt_int16)x;
  v->y    = (stbtt_int16)y;
  v->cx   = (stbtt_int16)cx;
  v->cy   = (stbtt_int16)cy;
}

static int
stbtt__GetGlyfOffset(const stbtt_fontinfo *info, int glyph_index)
{
  int g1, g2;

  STBTT_assert(!info->cff.size);

  if (glyph_index >= info->numGlyphs) {
    return -1; /*  glyph index out of range */
  }

  if (info->indexToLocFormat >= 2) {
    return -1; /*  unknown index->glyph map format */
  }

  if (info->indexToLocFormat == 0) {
    g1 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2) * 2;
    g2 = info->glyf + ttUSHORT(info->data + info->loca + glyph_index * 2 + 2) * 2;
  } else {
    g1 = info->glyf + ttULONG(info->data + info->loca + glyph_index * 4);
    g2 = info->glyf + ttULONG(info->data + info->loca + glyph_index * 4 + 4);
  }

  return g1 == g2 ? -1 : g1; /*  if length is 0, return -1 */
}

static int
stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1);

STBTT_DEF int
stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
  if (info->cff.size) {
    stbtt__GetGlyphInfoT2(info, glyph_index, x0, y0, x1, y1);
  } else {
    int g = stbtt__GetGlyfOffset(info, glyph_index);
    if (g < 0) {
      return 0;
    }

    if (x0) {
      *x0 = ttSHORT(info->data + g + 2);
    }

    if (y0) {
      *y0 = ttSHORT(info->data + g + 4);
    }

    if (x1) {
      *x1 = ttSHORT(info->data + g + 6);
    }

    if (y1) {
      *y1 = ttSHORT(info->data + g + 8);
    }
  }
  return 1;
}

static int
stbtt__close_shape(
  stbtt_vertex *vertices, int num_vertices, int was_off, int start_off, stbtt_int32 sx, stbtt_int32 sy, stbtt_int32 scx, stbtt_int32 scy, stbtt_int32 cx, stbtt_int32 cy)
{
  if (start_off) {
    if (was_off) {
      stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + scx) >> 1, (cy + scy) >> 1, cx, cy);
    }
    stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, scx, scy);
  } else {
    if (was_off) {
      stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, sx, sy, cx, cy);
    } else {
      stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, sx, sy, 0, 0);
    }
  }
  return num_vertices;
}

static int
stbtt__GetGlyphShapeTT(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
  stbtt_int16   numberOfContours;
  stbtt_uint8  *endPtsOfContours;
  stbtt_uint8  *data         = info->data;
  stbtt_vertex *vertices     = 0;
  int           num_vertices = 0;
  int           g            = stbtt__GetGlyfOffset(info, glyph_index);

  *pvertices = NULL;

  if (g < 0) {
    return 0;
  }

  numberOfContours = ttSHORT(data + g);

  if (numberOfContours > 0) {
    stbtt_uint8  flags = 0, flagcount;
    stbtt_int32  ins, i, j = 0, m, n, next_move, was_off = 0, off, start_off = 0;
    stbtt_int32  x, y, cx, cy, sx, sy, scx, scy;
    stbtt_uint8 *points;
    endPtsOfContours = (data + g + 10);
    ins              = ttUSHORT(data + g + 10 + numberOfContours * 2);
    points           = data + g + 10 + numberOfContours * 2 + 2 + ins;

    n = 1 + ttUSHORT(endPtsOfContours + numberOfContours * 2 - 2);

    m        = n + 2 * numberOfContours; /*  a loose bound on how many vertices we might need */
    vertices = (stbtt_vertex *)STBTT_malloc(m * sizeof(vertices[0]), info->userdata);
    if (vertices == 0) {
      return 0;
    }

    next_move = 0;
    flagcount = 0;

    /*  in first pass, we load uninterpreted data into the allocated array */
    /*  above, shifted to the end of the array so we won't overwrite it when */
    /*  we create our final data starting from the front */

    off = m - n; /*  starting offset for uninterpreted data, regardless of how m ends up being calculated */

    /*  first load flags */

    for (i = 0; i < n; ++i) {
      if (flagcount == 0) {
        flags = *points++;
        if (flags & 8) {
          flagcount = *points++;
        }
      } else {
        --flagcount;
      }
      vertices[off + i].type = flags;
    }

    /*  now load x coordinates */
    x = 0;
    for (i = 0; i < n; ++i) {
      flags = vertices[off + i].type;
      if (flags & 2) {
        stbtt_int16 dx  = *points++;
        x              += (flags & 16) ? dx : -dx; /*  ??? */
      } else {
        if (!(flags & 16)) {
          x       = x + (stbtt_int16)(points[0] * 256 + points[1]);
          points += 2;
        }
      }
      vertices[off + i].x = (stbtt_int16)x;
    }

    /*  now load y coordinates */
    y = 0;
    for (i = 0; i < n; ++i) {
      flags = vertices[off + i].type;
      if (flags & 4) {
        stbtt_int16 dy  = *points++;
        y              += (flags & 32) ? dy : -dy; /*  ??? */
      } else {
        if (!(flags & 32)) {
          y       = y + (stbtt_int16)(points[0] * 256 + points[1]);
          points += 2;
        }
      }
      vertices[off + i].y = (stbtt_int16)y;
    }

    /*  now convert them to our format */
    num_vertices = 0;
    sx = sy = cx = cy = scx = scy = 0;
    for (i = 0; i < n; ++i) {
      flags = vertices[off + i].type;
      x     = (stbtt_int16)vertices[off + i].x;
      y     = (stbtt_int16)vertices[off + i].y;

      if (next_move == i) {
        if (i != 0) {
          num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);
        }

        /*  now start the new one */
        start_off = !(flags & 1);
        if (start_off) {
          /*  if we start off with an off-curve point, then when we need to find a point on the curve */
          /*  where we can start, and we need to save some state for when we wraparound. */
          scx = x;
          scy = y;
          if (!(vertices[off + i + 1].type & 1)) {
            /*  next point is also a curve point, so interpolate an on-point curve */
            sx = (x + (stbtt_int32)vertices[off + i + 1].x) >> 1;
            sy = (y + (stbtt_int32)vertices[off + i + 1].y) >> 1;
          } else {
            /*  otherwise just use the next point as our start point */
            sx = (stbtt_int32)vertices[off + i + 1].x;
            sy = (stbtt_int32)vertices[off + i + 1].y;
            ++i; /*  we're using point i+1 as the starting point, so skip it */
          }
        } else {
          sx = x;
          sy = y;
        }
        stbtt_setvertex(&vertices[num_vertices++], STBTT_vmove, sx, sy, 0, 0);
        was_off   = 0;
        next_move = 1 + ttUSHORT(endPtsOfContours + j * 2);
        ++j;
      } else {
        if (!(flags & 1)) { /*  if it's a curve */
          if (was_off) {    /*  two off-curve control points in a row means interpolate an on-curve midpoint */
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, (cx + x) >> 1, (cy + y) >> 1, cx, cy);
          }
          cx      = x;
          cy      = y;
          was_off = 1;
        } else {
          if (was_off) {
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vcurve, x, y, cx, cy);
          } else {
            stbtt_setvertex(&vertices[num_vertices++], STBTT_vline, x, y, 0, 0);
          }
          was_off = 0;
        }
      }
    }
    num_vertices = stbtt__close_shape(vertices, num_vertices, was_off, start_off, sx, sy, scx, scy, cx, cy);
  } else if (numberOfContours < 0) {
    /*  Compound shapes. */
    int          more = 1;
    stbtt_uint8 *comp = data + g + 10;
    num_vertices      = 0;
    vertices          = 0;
    while (more) {
      stbtt_uint16  flags, gidx;
      int           comp_num_verts = 0, i;
      stbtt_vertex *comp_verts = 0, *tmp = 0;
      float         mtx[6] = {1, 0, 0, 1, 0, 0}, m, n;

      flags  = ttSHORT(comp);
      comp  += 2;
      gidx   = ttSHORT(comp);
      comp  += 2;

      if (flags & 2) {   /*  XY values */
        if (flags & 1) { /*  shorts */
          mtx[4]  = ttSHORT(comp);
          comp   += 2;
          mtx[5]  = ttSHORT(comp);
          comp   += 2;
        } else {
          mtx[4]  = ttCHAR(comp);
          comp   += 1;
          mtx[5]  = ttCHAR(comp);
          comp   += 1;
        }
      } else {
        /*  @TODO handle matching point */
        STBTT_assert(0);
      }

      if (flags & (1 << 3)) { /*  WE_HAVE_A_SCALE */
        mtx[0] = mtx[3]  = ttSHORT(comp) / 16384.0f;
        comp            += 2;
        mtx[1] = mtx[2] = 0;
      } else if (flags & (1 << 6)) { /*  WE_HAVE_AN_X_AND_YSCALE */
        mtx[0]  = ttSHORT(comp) / 16384.0f;
        comp   += 2;
        mtx[1] = mtx[2]  = 0;
        mtx[3]           = ttSHORT(comp) / 16384.0f;
        comp            += 2;
      } else if (flags & (1 << 7)) { /*  WE_HAVE_A_TWO_BY_TWO */
        mtx[0]  = ttSHORT(comp) / 16384.0f;
        comp   += 2;
        mtx[1]  = ttSHORT(comp) / 16384.0f;
        comp   += 2;
        mtx[2]  = ttSHORT(comp) / 16384.0f;
        comp   += 2;
        mtx[3]  = ttSHORT(comp) / 16384.0f;
        comp   += 2;
      }

      /*  Find transformation scales. */
      m = (float)STBTT_sqrt(mtx[0] * mtx[0] + mtx[1] * mtx[1]);
      n = (float)STBTT_sqrt(mtx[2] * mtx[2] + mtx[3] * mtx[3]);

      /*  Get indexed glyph. */
      comp_num_verts = stbtt_GetGlyphShape(info, gidx, &comp_verts);
      if (comp_num_verts > 0) {
        /*  Transform vertices. */
        for (i = 0; i < comp_num_verts; ++i) {
          stbtt_vertex     *v = &comp_verts[i];
          stbtt_vertex_type x, y;
          x     = v->x;
          y     = v->y;
          v->x  = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
          v->y  = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
          x     = v->cx;
          y     = v->cy;
          v->cx = (stbtt_vertex_type)(m * (mtx[0] * x + mtx[2] * y + mtx[4]));
          v->cy = (stbtt_vertex_type)(n * (mtx[1] * x + mtx[3] * y + mtx[5]));
        }
        /*  Append vertices. */
        tmp = (stbtt_vertex *)STBTT_malloc((num_vertices + comp_num_verts) * sizeof(stbtt_vertex), info->userdata);
        if (!tmp) {
          if (vertices) {
            STBTT_free(vertices, info->userdata);
          }

          if (comp_verts) {
            STBTT_free(comp_verts, info->userdata);
          }
          return 0;
        }

        if (num_vertices > 0 && vertices) {
          STBTT_memcpy(tmp, vertices, num_vertices * sizeof(stbtt_vertex));
        }
        STBTT_memcpy(tmp + num_vertices, comp_verts, comp_num_verts * sizeof(stbtt_vertex));
        if (vertices) {
          STBTT_free(vertices, info->userdata);
        }
        vertices = tmp;
        STBTT_free(comp_verts, info->userdata);
        num_vertices += comp_num_verts;
      }
      /*  More components ? */
      more = flags & (1 << 5);
    }
  } else {
    /*  numberOfCounters == 0, do nothing */
  }

  *pvertices = vertices;
  return num_vertices;
}

typedef struct {
  int         bounds;
  int         started;
  float       first_x, first_y;
  float       x, y;
  stbtt_int32 min_x, max_x, min_y, max_y;

  stbtt_vertex *pvertices;
  int           num_vertices;
} stbtt__csctx;

#  define STBTT__CSCTX_INIT(bounds) {bounds, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0}

static void
stbtt__track_vertex(stbtt__csctx *c, stbtt_int32 x, stbtt_int32 y)
{
  if (x > c->max_x || !c->started) {
    c->max_x = x;
  }

  if (y > c->max_y || !c->started) {
    c->max_y = y;
  }

  if (x < c->min_x || !c->started) {
    c->min_x = x;
  }

  if (y < c->min_y || !c->started) {
    c->min_y = y;
  }
  c->started = 1;
}

static void
stbtt__csctx_v(stbtt__csctx *c, stbtt_uint8 type, stbtt_int32 x, stbtt_int32 y, stbtt_int32 cx, stbtt_int32 cy, stbtt_int32 cx1, stbtt_int32 cy1)
{
  if (c->bounds) {
    stbtt__track_vertex(c, x, y);
    if (type == STBTT_vcubic) {
      stbtt__track_vertex(c, cx, cy);
      stbtt__track_vertex(c, cx1, cy1);
    }
  } else {
    stbtt_setvertex(&c->pvertices[c->num_vertices], type, x, y, cx, cy);
    c->pvertices[c->num_vertices].cx1 = (stbtt_int16)cx1;
    c->pvertices[c->num_vertices].cy1 = (stbtt_int16)cy1;
  }
  c->num_vertices++;
}

static void
stbtt__csctx_close_shape(stbtt__csctx *ctx)
{
  if (ctx->first_x != ctx->x || ctx->first_y != ctx->y) {
    stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->first_x, (int)ctx->first_y, 0, 0, 0, 0);
  }
}

static void
stbtt__csctx_rmove_to(stbtt__csctx *ctx, float dx, float dy)
{
  stbtt__csctx_close_shape(ctx);
  ctx->first_x = ctx->x = ctx->x + dx;
  ctx->first_y = ctx->y = ctx->y + dy;
  stbtt__csctx_v(ctx, STBTT_vmove, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void
stbtt__csctx_rline_to(stbtt__csctx *ctx, float dx, float dy)
{
  ctx->x += dx;
  ctx->y += dy;
  stbtt__csctx_v(ctx, STBTT_vline, (int)ctx->x, (int)ctx->y, 0, 0, 0, 0);
}

static void
stbtt__csctx_rccurve_to(stbtt__csctx *ctx, float dx1, float dy1, float dx2, float dy2, float dx3, float dy3)
{
  float cx1 = ctx->x + dx1;
  float cy1 = ctx->y + dy1;
  float cx2 = cx1 + dx2;
  float cy2 = cy1 + dy2;
  ctx->x    = cx2 + dx3;
  ctx->y    = cy2 + dy3;
  stbtt__csctx_v(ctx, STBTT_vcubic, (int)ctx->x, (int)ctx->y, (int)cx1, (int)cy1, (int)cx2, (int)cy2);
}

static stbtt__buf
stbtt__get_subr(stbtt__buf idx, int n)
{
  int count = stbtt__cff_index_count(&idx);
  int bias  = 107;
  if (count >= 33900) {
    bias = 32768;
  } else if (count >= 1240) {
    bias = 1131;
  }
  n += bias;
  if (n < 0 || n >= count) {
    return stbtt__new_buf(NULL, 0);
  }
  return stbtt__cff_index_get(idx, n);
}

static stbtt__buf
stbtt__cid_get_glyph_subrs(const stbtt_fontinfo *info, int glyph_index)
{
  stbtt__buf fdselect = info->fdselect;
  int        nranges, start, end, v, fmt, fdselector = -1, i;

  stbtt__buf_seek(&fdselect, 0);
  fmt = stbtt__buf_get8(&fdselect);
  if (fmt == 0) {
    /*  untested */
    stbtt__buf_skip(&fdselect, glyph_index);
    fdselector = stbtt__buf_get8(&fdselect);
  } else if (fmt == 3) {
    nranges = stbtt__buf_get16(&fdselect);
    start   = stbtt__buf_get16(&fdselect);
    for (i = 0; i < nranges; i++) {
      v   = stbtt__buf_get8(&fdselect);
      end = stbtt__buf_get16(&fdselect);
      if (glyph_index >= start && glyph_index < end) {
        fdselector = v;
        break;
      }
      start = end;
    }
  }

  if (fdselector == -1) {
    stbtt__new_buf(NULL, 0);
  }
  return stbtt__get_subrs(info->cff, stbtt__cff_index_get(info->fontdicts, fdselector));
}

static int
stbtt__run_charstring(const stbtt_fontinfo *info, int glyph_index, stbtt__csctx *c)
{
  int        in_header = 1, maskbits = 0, subr_stack_height = 0, sp = 0, v, i, b0;
  int        has_subrs = 0, clear_stack;
  float      s[48];
  stbtt__buf subr_stack[10], subrs = info->subrs, b;
  float      f;

#  define STBTT__CSERR(s) (0)

  /*  this currently ignores the initial width value, which isn't needed if we have hmtx */
  b = stbtt__cff_index_get(info->charstrings, glyph_index);
  while (b.cursor < b.size) {
    i           = 0;
    clear_stack = 1;
    b0          = stbtt__buf_get8(&b);
    switch (b0) {
    /*  @TODO implement hinting */
    case 0x13: /*  hintmask */
    case 0x14: /*  cntrmask */
      if (in_header) {
        maskbits += (sp / 2); /*  implicit "vstem" */
      }
      in_header = 0;
      stbtt__buf_skip(&b, (maskbits + 7) / 8);
      break;

    case 0x01: /*  hstem */
    case 0x03: /*  vstem */
    case 0x12: /*  hstemhm */
    case 0x17: /*  vstemhm */
      maskbits += (sp / 2);
      break;

    case 0x15: /*  rmoveto */
      in_header = 0;
      if (sp < 2) {
        return STBTT__CSERR("rmoveto stack");
      }
      stbtt__csctx_rmove_to(c, s[sp - 2], s[sp - 1]);
      break;
    case 0x04: /*  vmoveto */
      in_header = 0;
      if (sp < 1) {
        return STBTT__CSERR("vmoveto stack");
      }
      stbtt__csctx_rmove_to(c, 0, s[sp - 1]);
      break;
    case 0x16: /*  hmoveto */
      in_header = 0;
      if (sp < 1) {
        return STBTT__CSERR("hmoveto stack");
      }
      stbtt__csctx_rmove_to(c, s[sp - 1], 0);
      break;

    case 0x05: /*  rlineto */
      if (sp < 2) {
        return STBTT__CSERR("rlineto stack");
      }

      for (; i + 1 < sp; i += 2) {
        stbtt__csctx_rline_to(c, s[i], s[i + 1]);
      }
      break;

      /*  hlineto/vlineto and vhcurveto/hvcurveto alternate horizontal and vertical */
      /*  starting from a different place. */

    case 0x07: /*  vlineto */
      if (sp < 1) {
        return STBTT__CSERR("vlineto stack");
      }
      goto vlineto;
    case 0x06: /*  hlineto */
      if (sp < 1) {
        return STBTT__CSERR("hlineto stack");
      }

      for (;;) {
        if (i >= sp) {
          break;
        }
        stbtt__csctx_rline_to(c, s[i], 0);
        i++;
      vlineto:
        if (i >= sp) {
          break;
        }
        stbtt__csctx_rline_to(c, 0, s[i]);
        i++;
      }
      break;

    case 0x1F: /*  hvcurveto */
      if (sp < 4) {
        return STBTT__CSERR("hvcurveto stack");
      }
      goto hvcurveto;
    case 0x1E: /*  vhcurveto */
      if (sp < 4) {
        return STBTT__CSERR("vhcurveto stack");
      }

      for (;;) {
        if (i + 3 >= sp) {
          break;
        }
        stbtt__csctx_rccurve_to(c, 0, s[i], s[i + 1], s[i + 2], s[i + 3], (sp - i == 5) ? s[i + 4] : 0.0f);
        i += 4;
      hvcurveto:
        if (i + 3 >= sp) {
          break;
        }
        stbtt__csctx_rccurve_to(c, s[i], 0, s[i + 1], s[i + 2], (sp - i == 5) ? s[i + 4] : 0.0f, s[i + 3]);
        i += 4;
      }
      break;

    case 0x08: /*  rrcurveto */
      if (sp < 6) {
        return STBTT__CSERR("rcurveline stack");
      }

      for (; i + 5 < sp; i += 6) {
        stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
      }
      break;

    case 0x18: /*  rcurveline */
      if (sp < 8) {
        return STBTT__CSERR("rcurveline stack");
      }

      for (; i + 5 < sp - 2; i += 6) {
        stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
      }

      if (i + 1 >= sp) {
        return STBTT__CSERR("rcurveline stack");
      }
      stbtt__csctx_rline_to(c, s[i], s[i + 1]);
      break;

    case 0x19: /*  rlinecurve */
      if (sp < 8) {
        return STBTT__CSERR("rlinecurve stack");
      }

      for (; i + 1 < sp - 6; i += 2) {
        stbtt__csctx_rline_to(c, s[i], s[i + 1]);
      }

      if (i + 5 >= sp) {
        return STBTT__CSERR("rlinecurve stack");
      }
      stbtt__csctx_rccurve_to(c, s[i], s[i + 1], s[i + 2], s[i + 3], s[i + 4], s[i + 5]);
      break;

    case 0x1A: /*  vvcurveto */
    case 0x1B: /*  hhcurveto */
      if (sp < 4) {
        return STBTT__CSERR("(vv|hh)curveto stack");
      }
      f = 0.0;
      if (sp & 1) {
        f = s[i];
        i++;
      }

      for (; i + 3 < sp; i += 4) {
        if (b0 == 0x1B) {
          stbtt__csctx_rccurve_to(c, s[i], f, s[i + 1], s[i + 2], s[i + 3], 0.0);
        } else {
          stbtt__csctx_rccurve_to(c, f, s[i], s[i + 1], s[i + 2], 0.0, s[i + 3]);
        }
        f = 0.0;
      }
      break;

    case 0x0A: /*  callsubr */
      if (!has_subrs) {
        if (info->fdselect.size) {
          subrs = stbtt__cid_get_glyph_subrs(info, glyph_index);
        }
        has_subrs = 1;
      }
      /*  FALLTHROUGH */
    case 0x1D: /*  callgsubr */
      if (sp < 1) {
        return STBTT__CSERR("call(g|)subr stack");
      }
      v = (int)s[--sp];
      if (subr_stack_height >= 10) {
        return STBTT__CSERR("recursion limit");
      }
      subr_stack[subr_stack_height++] = b;
      b                               = stbtt__get_subr(b0 == 0x0A ? subrs : info->gsubrs, v);
      if (b.size == 0) {
        return STBTT__CSERR("subr not found");
      }
      b.cursor    = 0;
      clear_stack = 0;
      break;

    case 0x0B: /*  return */
      if (subr_stack_height <= 0) {
        return STBTT__CSERR("return outside subr");
      }
      b           = subr_stack[--subr_stack_height];
      clear_stack = 0;
      break;

    case 0x0E: /*  endchar */
      stbtt__csctx_close_shape(c);
      return 1;

    case 0x0C: { /*  two-byte escape */
      float dx1, dx2, dx3, dx4, dx5, dx6, dy1, dy2, dy3, dy4, dy5, dy6;
      float dx, dy;
      int   b1 = stbtt__buf_get8(&b);
      switch (b1) {
      /*  @TODO These "flex" implementations ignore the flex-depth and resolution, */
      /*  and always draw beziers. */
      case 0x22: /*  hflex */
        if (sp < 7) {
          return STBTT__CSERR("hflex stack");
        }
        dx1 = s[0];
        dx2 = s[1];
        dy2 = s[2];
        dx3 = s[3];
        dx4 = s[4];
        dx5 = s[5];
        dx6 = s[6];
        stbtt__csctx_rccurve_to(c, dx1, 0, dx2, dy2, dx3, 0);
        stbtt__csctx_rccurve_to(c, dx4, 0, dx5, -dy2, dx6, 0);
        break;

      case 0x23: /*  flex */
        if (sp < 13) {
          return STBTT__CSERR("flex stack");
        }
        dx1 = s[0];
        dy1 = s[1];
        dx2 = s[2];
        dy2 = s[3];
        dx3 = s[4];
        dy3 = s[5];
        dx4 = s[6];
        dy4 = s[7];
        dx5 = s[8];
        dy5 = s[9];
        dx6 = s[10];
        dy6 = s[11];
        /* fd is s[12] */
        stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
        stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
        break;

      case 0x24: /*  hflex1 */
        if (sp < 9) {
          return STBTT__CSERR("hflex1 stack");
        }
        dx1 = s[0];
        dy1 = s[1];
        dx2 = s[2];
        dy2 = s[3];
        dx3 = s[4];
        dx4 = s[5];
        dx5 = s[6];
        dy5 = s[7];
        dx6 = s[8];
        stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, 0);
        stbtt__csctx_rccurve_to(c, dx4, 0, dx5, dy5, dx6, -(dy1 + dy2 + dy5));
        break;

      case 0x25: /*  flex1 */
        if (sp < 11) {
          return STBTT__CSERR("flex1 stack");
        }
        dx1 = s[0];
        dy1 = s[1];
        dx2 = s[2];
        dy2 = s[3];
        dx3 = s[4];
        dy3 = s[5];
        dx4 = s[6];
        dy4 = s[7];
        dx5 = s[8];
        dy5 = s[9];
        dx6 = dy6 = s[10];
        dx        = dx1 + dx2 + dx3 + dx4 + dx5;
        dy        = dy1 + dy2 + dy3 + dy4 + dy5;
        if (STBTT_fabs(dx) > STBTT_fabs(dy)) {
          dy6 = -dy;
        } else {
          dx6 = -dx;
        }
        stbtt__csctx_rccurve_to(c, dx1, dy1, dx2, dy2, dx3, dy3);
        stbtt__csctx_rccurve_to(c, dx4, dy4, dx5, dy5, dx6, dy6);
        break;

      default:
        return STBTT__CSERR("unimplemented");
      }
    } break;

    default:
      if (b0 != 255 && b0 != 28 && b0 < 32) {
        return STBTT__CSERR("reserved operator");
      }

      /*  push immediate */
      if (b0 == 255) {
        f = (float)(stbtt_int32)stbtt__buf_get32(&b) / 0x10000;
      } else {
        stbtt__buf_skip(&b, -1);
        f = (float)(stbtt_int16)stbtt__cff_int(&b);
      }

      if (sp >= 48) {
        return STBTT__CSERR("push stack overflow");
      }
      s[sp++]     = f;
      clear_stack = 0;
      break;
    }

    if (clear_stack) {
      sp = 0;
    }
  }
  return STBTT__CSERR("no endchar");

#  undef STBTT__CSERR
}

static int
stbtt__GetGlyphShapeT2(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
  /*  runs the charstring twice, once to count and once to output (to avoid realloc) */
  stbtt__csctx count_ctx  = STBTT__CSCTX_INIT(1);
  stbtt__csctx output_ctx = STBTT__CSCTX_INIT(0);
  if (stbtt__run_charstring(info, glyph_index, &count_ctx)) {
    *pvertices           = (stbtt_vertex *)STBTT_malloc(count_ctx.num_vertices * sizeof(stbtt_vertex), info->userdata);
    output_ctx.pvertices = *pvertices;
    if (!*pvertices) {
      return 0;
    }

    if (stbtt__run_charstring(info, glyph_index, &output_ctx)) {
      STBTT_assert(output_ctx.num_vertices == count_ctx.num_vertices);
      return output_ctx.num_vertices;
    }
  }
  *pvertices = NULL;
  return 0;
}

static int
stbtt__GetGlyphInfoT2(const stbtt_fontinfo *info, int glyph_index, int *x0, int *y0, int *x1, int *y1)
{
  stbtt__csctx c = STBTT__CSCTX_INIT(1);
  int          r = stbtt__run_charstring(info, glyph_index, &c);
  if (x0) {
    *x0 = r ? c.min_x : 0;
  }

  if (y0) {
    *y0 = r ? c.min_y : 0;
  }

  if (x1) {
    *x1 = r ? c.max_x : 0;
  }

  if (y1) {
    *y1 = r ? c.max_y : 0;
  }
  return r ? c.num_vertices : 0;
}

STBTT_DEF int
stbtt_GetGlyphShape(const stbtt_fontinfo *info, int glyph_index, stbtt_vertex **pvertices)
{
  if (!info->cff.size) {
    return stbtt__GetGlyphShapeTT(info, glyph_index, pvertices);
  } else {
    return stbtt__GetGlyphShapeT2(info, glyph_index, pvertices);
  }
}

STBTT_DEF void
stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info, int glyph_index, int *advanceWidth, int *leftSideBearing)
{
  stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data + info->hhea + 34);
  if (glyph_index < numOfLongHorMetrics) {
    if (advanceWidth) {
      *advanceWidth = ttSHORT(info->data + info->hmtx + 4 * glyph_index);
    }

    if (leftSideBearing) {
      *leftSideBearing = ttSHORT(info->data + info->hmtx + 4 * glyph_index + 2);
    }
  } else {
    if (advanceWidth) {
      *advanceWidth = ttSHORT(info->data + info->hmtx + 4 * (numOfLongHorMetrics - 1));
    }

    if (leftSideBearing) {
      *leftSideBearing = ttSHORT(info->data + info->hmtx + 4 * numOfLongHorMetrics + 2 * (glyph_index - numOfLongHorMetrics));
    }
  }
}

/*  Define to STBTT_assert(x) if you want to break on unimplemented formats. */
#  define STBTT_GPOS_TODO_assert(x)

STBTT_DEF float
stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info, float pixels)
{
  int unitsPerEm = ttUSHORT(info->data + info->head + 18);
  return pixels / unitsPerEm;
}

/* //////////////////////////////////////////////////////////////////////////// */
/*  */
/*  antialiasing software rasterizer */
/*  */

STBTT_DEF void
stbtt_GetGlyphBitmapBoxSubpixel(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, float shift_x, float shift_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
  int x0 = 0, y0 = 0, x1, y1; /*  =0 suppresses compiler warning */
  if (!stbtt_GetGlyphBox(font, glyph, &x0, &y0, &x1, &y1)) {
    /*  e.g. space character */
    if (ix0) {
      *ix0 = 0;
    }

    if (iy0) {
      *iy0 = 0;
    }

    if (ix1) {
      *ix1 = 0;
    }

    if (iy1) {
      *iy1 = 0;
    }
  } else {
    /*  move to integral bboxes (treating pixels as little squares, what pixels get touched)? */
    if (ix0) {
      *ix0 = STBTT_ifloor(x0 * scale_x + shift_x);
    }

    if (iy0) {
      *iy0 = STBTT_ifloor(-y1 * scale_y + shift_y);
    }

    if (ix1) {
      *ix1 = STBTT_iceil(x1 * scale_x + shift_x);
    }

    if (iy1) {
      *iy1 = STBTT_iceil(-y0 * scale_y + shift_y);
    }
  }
}

STBTT_DEF void
stbtt_GetGlyphBitmapBox(const stbtt_fontinfo *font, int glyph, float scale_x, float scale_y, int *ix0, int *iy0, int *ix1, int *iy1)
{
  stbtt_GetGlyphBitmapBoxSubpixel(font, glyph, scale_x, scale_y, 0.0f, 0.0f, ix0, iy0, ix1, iy1);
}

/* //////////////////////////////////////////////////////////////////////////// */
/*  */
/*   Rasterizer */

typedef struct stbtt__hheap_chunk {
  struct stbtt__hheap_chunk *next;
} stbtt__hheap_chunk;

typedef struct stbtt__hheap {
  struct stbtt__hheap_chunk *head;
  void                      *first_free;
  int                        num_remaining_in_head_chunk;
} stbtt__hheap;

static void *
stbtt__hheap_alloc(stbtt__hheap *hh, size_t size, void *userdata)
{
  if (hh->first_free) {
    void *p        = hh->first_free;
    hh->first_free = *(void **)p;
    return p;
  } else {
    if (hh->num_remaining_in_head_chunk == 0) {
      int                 count = (size < 32 ? 2000 : size < 128 ? 800 : 100);
      stbtt__hheap_chunk *c     = (stbtt__hheap_chunk *)STBTT_malloc(sizeof(stbtt__hheap_chunk) + size * count, userdata);
      if (c == NULL) {
        return NULL;
      }
      c->next                         = hh->head;
      hh->head                        = c;
      hh->num_remaining_in_head_chunk = count;
    }
    --hh->num_remaining_in_head_chunk;
    return (char *)(hh->head) + sizeof(stbtt__hheap_chunk) + size * hh->num_remaining_in_head_chunk;
  }
}

static void
stbtt__hheap_free(stbtt__hheap *hh, void *p)
{
  *(void **)p    = hh->first_free;
  hh->first_free = p;
}

static void
stbtt__hheap_cleanup(stbtt__hheap *hh, void *userdata)
{
  stbtt__hheap_chunk *c = hh->head;
  while (c) {
    stbtt__hheap_chunk *n = c->next;
    STBTT_free(c, userdata);
    c = n;
  }
}

typedef struct stbtt__edge {
  float x0, y0, x1, y1;
  int   invert;
} stbtt__edge;

typedef struct stbtt__active_edge {
  struct stbtt__active_edge *next;
#  if STBTT_RASTERIZER_VERSION == 1
  int   x, dx;
  float ey;
  int   direction;
#  elif STBTT_RASTERIZER_VERSION == 2
  float fx, fdx, fdy;
  float direction;
  float sy;
  float ey;
#  else
#    error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#  endif
} stbtt__active_edge;

#  if STBTT_RASTERIZER_VERSION == 1
#    define STBTT_FIXSHIFT 10
#    define STBTT_FIX      (1 << STBTT_FIXSHIFT)
#    define STBTT_FIXMASK  (STBTT_FIX - 1)

static stbtt__active_edge *
stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
  stbtt__active_edge *z    = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
  float               dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
  STBTT_assert(z != NULL);
  if (!z) {
    return z;
  }

  /*  round dx down to avoid overshooting */
  if (dxdy < 0) {
    z->dx = -STBTT_ifloor(STBTT_FIX * -dxdy);
  } else {
    z->dx = STBTT_ifloor(STBTT_FIX * dxdy);
  }

  z->x  = STBTT_ifloor(STBTT_FIX * e->x0 + z->dx * (start_point - e->y0)); /*  use z->dx so when we offset later it's by the same amount */
  z->x -= off_x * STBTT_FIX;

  z->ey        = e->y1;
  z->next      = 0;
  z->direction = e->invert ? 1 : -1;
  return z;
}
#  elif STBTT_RASTERIZER_VERSION == 2
static stbtt__active_edge *
stbtt__new_active(stbtt__hheap *hh, stbtt__edge *e, int off_x, float start_point, void *userdata)
{
  stbtt__active_edge *z    = (stbtt__active_edge *)stbtt__hheap_alloc(hh, sizeof(*z), userdata);
  float               dxdy = (e->x1 - e->x0) / (e->y1 - e->y0);
  STBTT_assert(z != NULL);
  /* STBTT_assert(e->y0 <= start_point); */
  if (!z) {
    return z;
  }
  z->fdx        = dxdy;
  z->fdy        = dxdy != 0.0f ? (1.0f / dxdy) : 0.0f;
  z->fx         = e->x0 + dxdy * (start_point - e->y0);
  z->fx        -= off_x;
  z->direction  = e->invert ? 1.0f : -1.0f;
  z->sy         = e->y0;
  z->ey         = e->y1;
  z->next       = 0;
  return z;
}
#  else
#    error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#  endif

#  if STBTT_RASTERIZER_VERSION == 1
/*  note: this routine clips fills that extend off the edges... ideally this */
/*  wouldn't happen, but it could happen if the truetype glyph bounding boxes */
/*  are wrong, or if the user supplies a too-small bitmap */
static void
stbtt__fill_active_edges(unsigned char *scanline, int len, stbtt__active_edge *e, int max_weight)
{
  /*  non-zero winding fill */
  int x0 = 0, w = 0;

  while (e) {
    if (w == 0) {
      /*  if we're currently at zero, we need to record the edge start point */
      x0  = e->x;
      w  += e->direction;
    } else {
      int x1  = e->x;
      w      += e->direction;
      /*  if we went to zero, we need to draw */
      if (w == 0) {
        int i = x0 >> STBTT_FIXSHIFT;
        int j = x1 >> STBTT_FIXSHIFT;

        if (i < len && j >= 0) {
          if (i == j) {
            /*  x0,x1 are the same pixel, so compute combined coverage */
            scanline[i] = scanline[i] + (stbtt_uint8)((x1 - x0) * max_weight >> STBTT_FIXSHIFT);
          } else {
            if (i >= 0) { /*  add antialiasing for x0 */
              scanline[i] = scanline[i] + (stbtt_uint8)(((STBTT_FIX - (x0 & STBTT_FIXMASK)) * max_weight) >> STBTT_FIXSHIFT);
            } else {
              i = -1; /*  clip */
            }

            if (j < len) { /*  add antialiasing for x1 */
              scanline[j] = scanline[j] + (stbtt_uint8)(((x1 & STBTT_FIXMASK) * max_weight) >> STBTT_FIXSHIFT);
            } else {
              j = len; /*  clip */
            }

            for (++i; i < j; ++i) { /*  fill pixels between x0 and x1 */
              scanline[i] = scanline[i] + (stbtt_uint8)max_weight;
            }
          }
        }
      }
    }

    e = e->next;
  }
}

static void
stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
  stbtt__hheap        hh     = {0, 0, 0};
  stbtt__active_edge *active = NULL;
  int                 y, j = 0;
  int                 max_weight = (255 / vsubsample); /*  weight per vertical scanline */
  int                 s;                               /*  vertical subsample index */
  unsigned char       scanline_data[512], *scanline;

  if (result->w > 512) {
    scanline = (unsigned char *)STBTT_malloc(result->w, userdata);
    if (!scanline) {
      return;
    }
  } else {
    scanline = scanline_data;
  }

  y       = off_y * vsubsample;
  e[n].y0 = (off_y + result->h) * (float)vsubsample + 1;

  while (j < result->h) {
    STBTT_memset(scanline, 0, result->w);
    for (s = 0; s < vsubsample; ++s) {
      /*  find center of pixel for this scanline */
      float                scan_y = y + 0.5f;
      stbtt__active_edge **step   = &active;

      /*  update all active edges; */
      /*  remove all active edges that terminate before the center of this scanline */
      while (*step) {
        stbtt__active_edge *z = *step;
        if (z->ey <= scan_y) {
          *step = z->next; /*  delete from list */
          STBTT_assert(z->direction);
          z->direction = 0;
          stbtt__hheap_free(&hh, z);
        } else {
          z->x += z->dx;            /*  advance to position for current scanline */
          step  = &((*step)->next); /*  advance through list */
        }
      }

      /*  resort the list if needed */
      for (;;) {
        int changed = 0;
        step        = &active;
        while (*step && (*step)->next) {
          if ((*step)->x > (*step)->next->x) {
            stbtt__active_edge *t = *step;
            stbtt__active_edge *q = t->next;

            t->next = q->next;
            q->next = t;
            *step   = q;
            changed = 1;
          }
          step = &(*step)->next;
        }

        if (!changed) {
          break;
        }
      }

      /*  insert all edges that start before the center of this scanline -- omit ones that also end on this scanline */
      while (e->y0 <= scan_y) {
        if (e->y1 > scan_y) {
          stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y, userdata);
          if (z != NULL) {
            /*  find insertion point */
            if (active == NULL) {
              active = z;
            } else if (z->x < active->x) {
              /*  insert at front */
              z->next = active;
              active  = z;
            } else {
              /*  find thing to insert AFTER */
              stbtt__active_edge *p = active;
              while (p->next && p->next->x < z->x) {
                p = p->next;
              }
              /*  at this point, p->next->x is NOT < z->x */
              z->next = p->next;
              p->next = z;
            }
          }
        }
        ++e;
      }

      /*  now process all active edges in XOR fashion */
      if (active) {
        stbtt__fill_active_edges(scanline, result->w, active, max_weight);
      }

      ++y;
    }
    STBTT_memcpy(result->pixels + j * result->stride, scanline, result->w);
    ++j;
  }

  stbtt__hheap_cleanup(&hh, userdata);

  if (scanline != scanline_data) {
    STBTT_free(scanline, userdata);
  }
}

#  elif STBTT_RASTERIZER_VERSION == 2

/*  the edge passed in here does not cross the vertical line at x or the vertical line at x+1 */
/*  (i.e. it has already been clipped to those) */
static void
stbtt__handle_clipped_edge(float *scanline, int x, stbtt__active_edge *e, float x0, float y0, float x1, float y1)
{
  if (y0 == y1) {
    return;
  }
  STBTT_assert(y0 < y1);
  STBTT_assert(e->sy <= e->ey);
  if (y0 > e->ey) {
    return;
  }

  if (y1 < e->sy) {
    return;
  }

  if (y0 < e->sy) {
    x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
    y0  = e->sy;
  }

  if (y1 > e->ey) {
    x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
    y1  = e->ey;
  }

  if (x0 == x) {
    STBTT_assert(x1 <= x + 1);
  } else if (x0 == x + 1) {
    STBTT_assert(x1 >= x);
  } else if (x0 <= x) {
    STBTT_assert(x1 <= x);
  } else if (x0 >= x + 1) {
    STBTT_assert(x1 >= x + 1);
  } else {
    STBTT_assert(x1 >= x && x1 <= x + 1);
  }

  if (x0 <= x && x1 <= x) {
    scanline[x] += e->direction * (y1 - y0);
  } else if (x0 >= x + 1 && x1 >= x + 1)
    ;
  else {
    STBTT_assert(x0 >= x && x0 <= x + 1 && x1 >= x && x1 <= x + 1);
    scanline[x] += e->direction * (y1 - y0) * (1 - ((x0 - x) + (x1 - x)) / 2); /*  coverage = 1 - average x position */
  }
}

static float
stbtt__sized_trapezoid_area(float height, float top_width, float bottom_width)
{
  STBTT_assert(top_width >= 0);
  STBTT_assert(bottom_width >= 0);
  return (top_width + bottom_width) / 2.0f * height;
}

static float
stbtt__position_trapezoid_area(float height, float tx0, float tx1, float bx0, float bx1)
{
  return stbtt__sized_trapezoid_area(height, tx1 - tx0, bx1 - bx0);
}

static float
stbtt__sized_triangle_area(float height, float width)
{
  return height * width / 2;
}

static void
stbtt__fill_active_edges_new(float *scanline, float *scanline_fill, int len, stbtt__active_edge *e, float y_top)
{
  float y_bottom = y_top + 1;

  while (e) {
    /*  brute force every pixel */

    /*  compute intersection points with top & bottom */
    STBTT_assert(e->ey >= y_top);

    if (e->fdx == 0) {
      float x0 = e->fx;
      if (x0 < len) {
        if (x0 >= 0) {
          stbtt__handle_clipped_edge(scanline, (int)x0, e, x0, y_top, x0, y_bottom);
          stbtt__handle_clipped_edge(scanline_fill - 1, (int)x0 + 1, e, x0, y_top, x0, y_bottom);
        } else {
          stbtt__handle_clipped_edge(scanline_fill - 1, 0, e, x0, y_top, x0, y_bottom);
        }
      }
    } else {
      float x0 = e->fx;
      float dx = e->fdx;
      float xb = x0 + dx;
      float x_top, x_bottom;
      float sy0, sy1;
      float dy = e->fdy;
      STBTT_assert(e->sy <= y_bottom && e->ey >= y_top);

      /*  compute endpoints of line segment clipped to this scanline (if the */
      /*  line segment starts on this scanline. x0 is the intersection of the */
      /*  line with y_top, but that may be off the line segment. */
      if (e->sy > y_top) {
        x_top = x0 + dx * (e->sy - y_top);
        sy0   = e->sy;
      } else {
        x_top = x0;
        sy0   = y_top;
      }

      if (e->ey < y_bottom) {
        x_bottom = x0 + dx * (e->ey - y_top);
        sy1      = e->ey;
      } else {
        x_bottom = xb;
        sy1      = y_bottom;
      }

      if (x_top >= 0 && x_bottom >= 0 && x_top < len && x_bottom < len) {
        /*  from here on, we don't have to range check x values */

        if ((int)x_top == (int)x_bottom) {
          float height;
          /*  simple case, only spans one pixel */
          int   x = (int)x_top;
          height  = (sy1 - sy0) * e->direction;
          STBTT_assert(x >= 0 && x < len);
          scanline[x]      += stbtt__position_trapezoid_area(height, x_top, x + 1.0f, x_bottom, x + 1.0f);
          scanline_fill[x] += height; /*  everything right of this pixel is filled */
        } else {
          int   x, x1, x2;
          float y_crossing, y_final, step, sign, area;
          /*  covers 2+ pixels */
          if (x_top > x_bottom) {
            /*  flip scanline vertically; signed area is the same */
            float t;
            sy0 = y_bottom - (sy0 - y_top);
            sy1 = y_bottom - (sy1 - y_top);
            t = sy0, sy0 = sy1, sy1 = t;
            t = x_bottom, x_bottom = x_top, x_top = t;
            dx = -dx;
            dy = -dy;
            t = x0, x0 = xb, xb = t;
          }
          STBTT_assert(dy >= 0);
          STBTT_assert(dx >= 0);

          x1         = (int)x_top;
          x2         = (int)x_bottom;
          /*  compute intersection with y axis at x1+1 */
          y_crossing = y_top + dy * (x1 + 1 - x0);

          /*  compute intersection with y axis at x2 */
          y_final = y_top + dy * (x2 - x0);

          /*            x1    x_top                            x2    x_bottom */
          /*      y_top  +------|-----+------------+------------+--------|---+------------+ */
          /*             |            |            |            |            |            | */
          /*             |            |            |            |            |            | */
          /*        sy0  |      Txxxxx|............|............|............|............| */
          /*  y_crossing |            *xxxxx.......|............|............|............| */
          /*             |            |     xxxxx..|............|............|............| */
          /*             |            |     /-   xx*xxxx........|............|............| */
          /*             |            | dy <       |    xxxxxx..|............|............| */
          /*    y_final  |            |     \-     |          xx*xxx.........|............| */
          /*        sy1  |            |            |            |   xxxxxB...|............| */
          /*             |            |            |            |            |            | */
          /*             |            |            |            |            |            | */
          /*   y_bottom  +------------+------------+------------+------------+------------+ */
          /*  */
          /*  goal is to measure the area covered by '.' in each pixel */

          /*  if x2 is right at the right edge of x1, y_crossing can blow up, github #1057 */
          /*  @TODO: maybe test against sy1 rather than y_bottom? */
          if (y_crossing > y_bottom) {
            y_crossing = y_bottom;
          }

          sign = e->direction;

          /*  area of the rectangle covered from sy0..y_crossing */
          area = sign * (y_crossing - sy0);

          /*  area of the triangle (x_top,sy0), (x1+1,sy0), (x1+1,y_crossing) */
          scanline[x1] += stbtt__sized_triangle_area(area, x1 + 1 - x_top);

          /*  check if final y_crossing is blown up; no test case for this */
          if (y_final > y_bottom) {
            y_final = y_bottom;
            dy      = (y_final - y_crossing) / (x2 - (x1 + 1)); /*  if denom=0, y_final = y_crossing, so y_final <= y_bottom */
          }

          /*  in second pixel, area covered by line segment found in first pixel */
          /*  is always a rectangle 1 wide * the height of that line segment; this */
          /*  is exactly what the variable 'area' stores. it also gets a contribution */
          /*  from the line segment within it. the THIRD pixel will get the first */
          /*  pixel's rectangle contribution, the second pixel's rectangle contribution, */
          /*  and its own contribution. the 'own contribution' is the same in every pixel except */
          /*  the leftmost and rightmost, a trapezoid that slides down in each pixel. */
          /*  the second pixel's contribution to the third pixel will be the */
          /*  rectangle 1 wide times the height change in the second pixel, which is dy. */

          step = sign * dy * 1; /*  dy is dy/dx, change in y for every 1 change in x, */
          /*  which multiplied by 1-pixel-width is how much pixel area changes for each step in x */
          /*  so the area advances by 'step' every time */

          for (x = x1 + 1; x < x2; ++x) {
            scanline[x] += area + step / 2; /*  area of trapezoid is 1*step/2 */
            area        += step;
          }
          STBTT_assert(STBTT_fabs(area) <= 1.01f); /*  accumulated error from area += step unless we round step down */
          STBTT_assert(sy1 > y_final - 0.01f);

          /*  area covered in the last pixel is the rectangle from all the pixels to the left, */
          /*  plus the trapezoid filled by the line segment in this pixel all the way to the right edge */
          scanline[x2] += area + sign * stbtt__position_trapezoid_area(sy1 - y_final, (float)x2, x2 + 1.0f, x_bottom, x2 + 1.0f);

          /*  the rest of the line is filled based on the total height of the line segment in this pixel */
          scanline_fill[x2] += sign * (sy1 - sy0);
        }
      } else {
        /*  if edge goes outside of box we're drawing, we require */
        /*  clipping logic. since this does not match the intended use */
        /*  of this library, we use a different, very slow brute */
        /*  force implementation */
        /*  note though that this does happen some of the time because */
        /*  x_top and x_bottom can be extrapolated at the top & bottom of */
        /*  the shape and actually lie outside the bounding box */
        int x;
        for (x = 0; x < len; ++x) {
          /*  cases: */
          /*  */
          /*  there can be up to two intersections with the pixel. any intersection */
          /*  with left or right edges can be handled by splitting into two (or three) */
          /*  regions. intersections with top & bottom do not necessitate case-wise logic. */
          /*  */
          /*  the old way of doing this found the intersections with the left & right edges, */
          /*  then used some simple logic to produce up to three segments in sorted order */
          /*  from top-to-bottom. however, this had a problem: if an x edge was epsilon */
          /*  across the x border, then the corresponding y position might not be distinct */
          /*  from the other y segment, and it might ignored as an empty segment. to avoid */
          /*  that, we need to explicitly produce segments based on x positions. */

          /*  rename variables to clearly-defined pairs */
          float y0 = y_top;
          float x1 = (float)(x);
          float x2 = (float)(x + 1);
          float x3 = xb;
          float y3 = y_bottom;

          /*  x = e->x + e->dx * (y-y_top) */
          /*  (y-y_top) = (x - e->x) / e->dx */
          /*  y = (x - e->x) / e->dx + y_top */
          float y1 = (x - x0) / dx + y_top;
          float y2 = (x + 1 - x0) / dx + y_top;

          if (x0 < x1 && x3 > x2) { /*  three segments descending down-right */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
            stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x2, y2);
            stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
          } else if (x3 < x1 && x0 > x2) { /*  three segments descending down-left */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
            stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x1, y1);
            stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
          } else if (x0 < x1 && x3 > x1) { /*  two segments across x, down-right */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
            stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
          } else if (x3 < x1 && x0 > x1) { /*  two segments across x, down-left */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x1, y1);
            stbtt__handle_clipped_edge(scanline, x, e, x1, y1, x3, y3);
          } else if (x0 < x2 && x3 > x2) { /*  two segments across x+1, down-right */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
            stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
          } else if (x3 < x2 && x0 > x2) { /*  two segments across x+1, down-left */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x2, y2);
            stbtt__handle_clipped_edge(scanline, x, e, x2, y2, x3, y3);
          } else { /*  one segment */
            stbtt__handle_clipped_edge(scanline, x, e, x0, y0, x3, y3);
          }
        }
      }
    }
    e = e->next;
  }
}

/*  directly AA rasterize edges w/o supersampling */
static void
stbtt__rasterize_sorted_edges(stbtt__bitmap *result, stbtt__edge *e, int n, int vsubsample, int off_x, int off_y, void *userdata)
{
  stbtt__hheap        hh     = {0, 0, 0};
  stbtt__active_edge *active = NULL;
  int                 y, j = 0, i;
  float               scanline_data[129], *scanline, *scanline2;

  STBTT__NOTUSED(vsubsample);

  if (result->w > 64) {
    scanline = (float *)STBTT_malloc((result->w * 2 + 1) * sizeof(float), userdata);
    if (!scanline) {
      return;
    }
  } else {
    scanline = scanline_data;
  }

  scanline2 = scanline + result->w;

  y       = off_y;
  e[n].y0 = (float)(off_y + result->h) + 1;

  while (j < result->h) {
    /*  find center of pixel for this scanline */
    float                scan_y_top    = y + 0.0f;
    float                scan_y_bottom = y + 1.0f;
    stbtt__active_edge **step          = &active;

    STBTT_memset(scanline, 0, result->w * sizeof(scanline[0]));
    STBTT_memset(scanline2, 0, (result->w + 1) * sizeof(scanline[0]));

    /*  update all active edges; */
    /*  remove all active edges that terminate before the top of this scanline */
    while (*step) {
      stbtt__active_edge *z = *step;
      if (z->ey <= scan_y_top) {
        *step = z->next; /*  delete from list */
        STBTT_assert(z->direction);
        z->direction = 0;
        stbtt__hheap_free(&hh, z);
      } else {
        step = &((*step)->next); /*  advance through list */
      }
    }

    /*  insert all edges that start before the bottom of this scanline */
    while (e->y0 <= scan_y_bottom) {
      if (e->y0 != e->y1) {
        stbtt__active_edge *z = stbtt__new_active(&hh, e, off_x, scan_y_top, userdata);
        if (z != NULL) {
          if (j == 0 && off_y != 0) {
            if (z->ey < scan_y_top) {
              /*  this can happen due to subpixel positioning and some kind of fp rounding error i think */
              z->ey = scan_y_top;
            }
          }
          STBTT_assert(z->ey >= scan_y_top); /*  if we get really unlucky a tiny bit of an edge can be out of bounds */
          /*  insert at front */
          z->next = active;
          active  = z;
        }
      }
      ++e;
    }

    /*  now process all active edges */
    if (active) {
      stbtt__fill_active_edges_new(scanline, scanline2 + 1, result->w, active, scan_y_top);
    }

    {
      float sum = 0;
      for (i = 0; i < result->w; ++i) {
        float k;
        int   m;
        sum += scanline2[i];
        k    = scanline[i] + sum;
        k    = (float)STBTT_fabs(k) * 255 + 0.5f;
        m    = (int)k;
        if (m > 255) {
          m = 255;
        }
        result->pixels[j * result->stride + i] = (unsigned char)m;
      }
    }
    /*  advance all the edges */
    step = &active;
    while (*step) {
      stbtt__active_edge *z  = *step;
      z->fx                 += z->fdx;           /*  advance to position for current scanline */
      step                   = &((*step)->next); /*  advance through list */
    }

    ++y;
    ++j;
  }

  stbtt__hheap_cleanup(&hh, userdata);

  if (scanline != scanline_data) {
    STBTT_free(scanline, userdata);
  }
}
#  else
#    error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#  endif

#  define STBTT__COMPARE(a, b) ((a)->y0 < (b)->y0)

static void
stbtt__sort_edges_ins_sort(stbtt__edge *p, int n)
{
  int i, j;
  for (i = 1; i < n; ++i) {
    stbtt__edge t = p[i], *a = &t;
    j = i;
    while (j > 0) {
      stbtt__edge *b = &p[j - 1];
      int          c = STBTT__COMPARE(a, b);
      if (!c) {
        break;
      }
      p[j] = p[j - 1];
      --j;
    }

    if (i != j) {
      p[j] = t;
    }
  }
}

static void
stbtt__sort_edges_quicksort(stbtt__edge *p, int n)
{
  /* threshold for transitioning to insertion sort */
  while (n > 12) {
    stbtt__edge t;
    int         c01, c12, c, m, i, j;

    /* compute median of three */
    m   = n >> 1;
    c01 = STBTT__COMPARE(&p[0], &p[m]);
    c12 = STBTT__COMPARE(&p[m], &p[n - 1]);
    /* if 0 >= mid >= end, or 0 < mid < end, then use mid */
    if (c01 != c12) {
      /* otherwise, we'll need to swap something else to middle */
      int z;
      c    = STBTT__COMPARE(&p[0], &p[n - 1]);
      /* 0>mid && mid<n:  0>n => n; 0<n => 0 */
      /* 0<mid && mid>n:  0>n => 0; 0<n => n */
      z    = (c == c12) ? 0 : n - 1;
      t    = p[z];
      p[z] = p[m];
      p[m] = t;
    }
    /* now p[m] is the median-of-three */
    /* swap it to the beginning so it won't move around */
    t    = p[0];
    p[0] = p[m];
    p[m] = t;

    /* partition loop */
    i = 1;
    j = n - 1;
    for (;;) {
      /* handling of equality is crucial here */
      /* for sentinels & efficiency with duplicates */
      for (;; ++i) {
        if (!STBTT__COMPARE(&p[i], &p[0])) {
          break;
        }
      }

      for (;; --j) {
        if (!STBTT__COMPARE(&p[0], &p[j])) {
          break;
        }
      }
      /* make sure we haven't crossed */
      if (i >= j) {
        break;
      }
      t    = p[i];
      p[i] = p[j];
      p[j] = t;

      ++i;
      --j;
    }
    /* recurse on smaller side, iterate on larger */
    if (j < (n - i)) {
      stbtt__sort_edges_quicksort(p, j);
      p = p + i;
      n = n - i;
    } else {
      stbtt__sort_edges_quicksort(p + i, n - i);
      n = j;
    }
  }
}

static void
stbtt__sort_edges(stbtt__edge *p, int n)
{
  stbtt__sort_edges_quicksort(p, n);
  stbtt__sort_edges_ins_sort(p, n);
}

typedef struct {
  float x, y;
} stbtt__point;

static void
stbtt__rasterize(
  stbtt__bitmap *result, stbtt__point *pts, int *wcount, int windings, float scale_x, float scale_y, float shift_x, float shift_y, int off_x, int off_y, int invert, void *userdata)
{
  float        y_scale_inv = invert ? -scale_y : scale_y;
  stbtt__edge *e;
  int          n, i, j, k, m;
#  if STBTT_RASTERIZER_VERSION == 1
  int vsubsample = result->h < 8 ? 15 : 5;
#  elif STBTT_RASTERIZER_VERSION == 2
  int vsubsample = 1;
#  else
#    error "Unrecognized value of STBTT_RASTERIZER_VERSION"
#  endif
  /*  vsubsample should divide 255 evenly; otherwise we won't reach full opacity */

  /*  now we have to blow out the windings into explicit edge lists */
  n = 0;
  for (i = 0; i < windings; ++i) {
    n += wcount[i];
  }

  e = (stbtt__edge *)STBTT_malloc(sizeof(*e) * (n + 1), userdata); /*  add an extra one as a sentinel */
  if (e == 0) {
    return;
  }
  n = 0;

  m = 0;
  for (i = 0; i < windings; ++i) {
    stbtt__point *p  = pts + m;
    m               += wcount[i];
    j                = wcount[i] - 1;
    for (k = 0; k < wcount[i]; j = k++) {
      int a = k, b = j;
      /*  skip the edge if horizontal */
      if (p[j].y == p[k].y) {
        continue;
      }
      /*  add edge from j to k to the list */
      e[n].invert = 0;
      if (invert ? p[j].y > p[k].y : p[j].y < p[k].y) {
        e[n].invert = 1;
        a = j, b = k;
      }
      e[n].x0 = p[a].x * scale_x + shift_x;
      e[n].y0 = (p[a].y * y_scale_inv + shift_y) * vsubsample;
      e[n].x1 = p[b].x * scale_x + shift_x;
      e[n].y1 = (p[b].y * y_scale_inv + shift_y) * vsubsample;
      ++n;
    }
  }

  /*  now sort the edges by their highest point (should snap to integer, and then by x) */
  /* STBTT_sort(e, n, sizeof(e[0]), stbtt__edge_compare); */
  stbtt__sort_edges(e, n);

  /*  now, traverse the scanlines and find the intersections on each scanline, use xor winding rule */
  stbtt__rasterize_sorted_edges(result, e, n, vsubsample, off_x, off_y, userdata);

  STBTT_free(e, userdata);
}

static void
stbtt__add_point(stbtt__point *points, int n, float x, float y)
{
  if (!points) {
    return; /*  during first pass, it's unallocated */
  }
  points[n].x = x;
  points[n].y = y;
}

/*  tessellate until threshold p is happy... @TODO warped to compensate for non-linear stretching */
static int
stbtt__tesselate_curve(stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float objspace_flatness_squared, int n)
{
  /*  midpoint */
  float mx = (x0 + 2 * x1 + x2) / 4;
  float my = (y0 + 2 * y1 + y2) / 4;
  /*  versus directly drawn line */
  float dx = (x0 + x2) / 2 - mx;
  float dy = (y0 + y2) / 2 - my;
  if (n > 16) { /*  65536 segments on one curve better be enough! */
    return 1;
  }

  if (dx * dx + dy * dy > objspace_flatness_squared) { /*  half-pixel error allowed... need to be smaller if AA */
    stbtt__tesselate_curve(points, num_points, x0, y0, (x0 + x1) / 2.0f, (y0 + y1) / 2.0f, mx, my, objspace_flatness_squared, n + 1);
    stbtt__tesselate_curve(points, num_points, mx, my, (x1 + x2) / 2.0f, (y1 + y2) / 2.0f, x2, y2, objspace_flatness_squared, n + 1);
  } else {
    stbtt__add_point(points, *num_points, x2, y2);
    *num_points = *num_points + 1;
  }
  return 1;
}

static void
stbtt__tesselate_cubic(
  stbtt__point *points, int *num_points, float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, float objspace_flatness_squared, int n)
{
  /*  @TODO this "flatness" calculation is just made-up nonsense that seems to work well enough */
  float dx0              = x1 - x0;
  float dy0              = y1 - y0;
  float dx1              = x2 - x1;
  float dy1              = y2 - y1;
  float dx2              = x3 - x2;
  float dy2              = y3 - y2;
  float dx               = x3 - x0;
  float dy               = y3 - y0;
  float longlen          = (float)(STBTT_sqrt(dx0 * dx0 + dy0 * dy0) + STBTT_sqrt(dx1 * dx1 + dy1 * dy1) + STBTT_sqrt(dx2 * dx2 + dy2 * dy2));
  float shortlen         = (float)STBTT_sqrt(dx * dx + dy * dy);
  float flatness_squared = longlen * longlen - shortlen * shortlen;

  if (n > 16) { /*  65536 segments on one curve better be enough! */
    return;
  }

  if (flatness_squared > objspace_flatness_squared) {
    float x01 = (x0 + x1) / 2;
    float y01 = (y0 + y1) / 2;
    float x12 = (x1 + x2) / 2;
    float y12 = (y1 + y2) / 2;
    float x23 = (x2 + x3) / 2;
    float y23 = (y2 + y3) / 2;

    float xa = (x01 + x12) / 2;
    float ya = (y01 + y12) / 2;
    float xb = (x12 + x23) / 2;
    float yb = (y12 + y23) / 2;

    float mx = (xa + xb) / 2;
    float my = (ya + yb) / 2;

    stbtt__tesselate_cubic(points, num_points, x0, y0, x01, y01, xa, ya, mx, my, objspace_flatness_squared, n + 1);
    stbtt__tesselate_cubic(points, num_points, mx, my, xb, yb, x23, y23, x3, y3, objspace_flatness_squared, n + 1);
  } else {
    stbtt__add_point(points, *num_points, x3, y3);
    *num_points = *num_points + 1;
  }
}

/*  returns number of contours */
static stbtt__point *
stbtt_FlattenCurves(stbtt_vertex *vertices, int num_verts, float objspace_flatness, int **contour_lengths, int *num_contours, void *userdata)
{
  stbtt__point *points     = 0;
  int           num_points = 0;

  float objspace_flatness_squared = objspace_flatness * objspace_flatness;
  int   i, n = 0, start = 0, pass;

  /*  count how many "moves" there are to get the contour count */
  for (i = 0; i < num_verts; ++i) {
    if (vertices[i].type == STBTT_vmove) {
      ++n;
    }
  }

  *num_contours = n;
  if (n == 0) {
    return 0;
  }

  *contour_lengths = (int *)STBTT_malloc(sizeof(**contour_lengths) * n, userdata);

  if (*contour_lengths == 0) {
    *num_contours = 0;
    return 0;
  }

  /*  make two passes through the points so we don't need to realloc */
  for (pass = 0; pass < 2; ++pass) {
    float x = 0, y = 0;
    if (pass == 1) {
      points = (stbtt__point *)STBTT_malloc(num_points * sizeof(points[0]), userdata);
      if (points == NULL) {
        goto error;
      }
    }
    num_points = 0;
    n          = -1;
    for (i = 0; i < num_verts; ++i) {
      switch (vertices[i].type) {
      case STBTT_vmove:
        /*  start the next contour */
        if (n >= 0) {
          (*contour_lengths)[n] = num_points - start;
        }
        ++n;
        start = num_points;

        x = vertices[i].x, y = vertices[i].y;
        stbtt__add_point(points, num_points++, x, y);
        break;
      case STBTT_vline:
        x = vertices[i].x, y = vertices[i].y;
        stbtt__add_point(points, num_points++, x, y);
        break;
      case STBTT_vcurve:
        stbtt__tesselate_curve(points, &num_points, x, y, vertices[i].cx, vertices[i].cy, vertices[i].x, vertices[i].y, objspace_flatness_squared, 0);
        x = vertices[i].x, y = vertices[i].y;
        break;
      case STBTT_vcubic:
        stbtt__tesselate_cubic(
          points, &num_points, x, y, vertices[i].cx, vertices[i].cy, vertices[i].cx1, vertices[i].cy1, vertices[i].x, vertices[i].y, objspace_flatness_squared, 0);
        x = vertices[i].x, y = vertices[i].y;
        break;
      }
    }
    (*contour_lengths)[n] = num_points - start;
  }

  return points;
error:
  STBTT_free(points, userdata);
  STBTT_free(*contour_lengths, userdata);
  *contour_lengths = 0;
  *num_contours    = 0;
  return NULL;
}

STBTT_DEF void
stbtt_Rasterize(stbtt__bitmap *result,
                float          flatness_in_pixels,
                stbtt_vertex  *vertices,
                int            num_verts,
                float          scale_x,
                float          scale_y,
                float          shift_x,
                float          shift_y,
                int            x_off,
                int            y_off,
                int            invert,
                void          *userdata)
{
  float         scale           = scale_x > scale_y ? scale_y : scale_x;
  int           winding_count   = 0;
  int          *winding_lengths = NULL;
  stbtt__point *windings        = stbtt_FlattenCurves(vertices, num_verts, flatness_in_pixels / scale, &winding_lengths, &winding_count, userdata);
  if (windings) {
    stbtt__rasterize(result, windings, winding_lengths, winding_count, scale_x, scale_y, shift_x, shift_y, x_off, y_off, invert, userdata);
    STBTT_free(winding_lengths, userdata);
    STBTT_free(windings, userdata);
  }
}

STBTT_DEF void
stbtt_MakeGlyphBitmapSubpixel(
  const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, float shift_x, float shift_y, int glyph)
{
  int           ix0, iy0;
  stbtt_vertex *vertices;
  int           num_verts = stbtt_GetGlyphShape(info, glyph, &vertices);
  stbtt__bitmap gbm;

  stbtt_GetGlyphBitmapBoxSubpixel(info, glyph, scale_x, scale_y, shift_x, shift_y, &ix0, &iy0, 0, 0);
  gbm.pixels = output;
  gbm.w      = out_w;
  gbm.h      = out_h;
  gbm.stride = out_stride;

  if (gbm.w && gbm.h) {
    stbtt_Rasterize(&gbm, 0.35f, vertices, num_verts, scale_x, scale_y, shift_x, shift_y, ix0, iy0, 1, info->userdata);
  }

  STBTT_free(vertices, info->userdata);
}

STBTT_DEF void
stbtt_MakeGlyphBitmap(const stbtt_fontinfo *info, unsigned char *output, int out_w, int out_h, int out_stride, float scale_x, float scale_y, int glyph)
{
  stbtt_MakeGlyphBitmapSubpixel(info, output, out_w, out_h, out_stride, scale_x, scale_y, 0.0f, 0.0f, glyph);
}

#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-qual"
#  endif

STBTT_DEF int
stbtt_GetFontOffsetForIndex(const unsigned char *data, int index)
{
  return stbtt_GetFontOffsetForIndex_internal((unsigned char *)data, index);
}

STBTT_DEF int
stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data, int offset)
{
  return stbtt_InitFont_internal(info, (unsigned char *)data, offset);
}

#  if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#  endif

#endif /*  STB_TRUETYPE_IMPLEMENTATION */

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Sean Barrett
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

#ifdef NK_INCLUDE_GLYPH_ATLAS
/* ===============================================================
 *
 *                          GLYPH ATLAS
 *
 * ===============================================================*/
#  define NK_GLYPH_ATLAS_SCRATCH_SIZE (256 * 1024) /* stb_truetype work memory for one glyph */
#  define NK_GLYPH_ATLAS_PADDING      1

struct nk_glyph_atlas_slot {
  const struct nk_font     *font; /* NULL: free */
  nk_glyph_id               id;
  float                     height;
  struct nk_user_font_glyph glyph;
};

NK_INTERN void *
nk_glyph_atlas_scratch_alloc(nk_handle userdata, void *old, nk_size size)
{
  struct nk_glyph_atlas *atlas = (struct nk_glyph_atlas *)userdata.ptr;
  nk_size                start = (atlas->scratch_used + 15) & ~(nk_size)15;

  NK_UNUSED(old);

  if (start > atlas->scratch_size || size > atlas->scratch_size - start) {
    return NULL;
  }

  atlas->scratch_used = start + size;
  return atlas->scratch + start;
}

NK_INTERN void
nk_glyph_atlas_scratch_free(nk_handle userdata, void *memory)
{
  NK_UNUSED(userdata);
  NK_UNUSED(memory);
}

NK_INTERN int
nk_glyph_atlas_slot_capacity(int max_glyphs)
{
  int capacity = 64;

  while (capacity < max_glyphs * 2) {
    capacity *= 2;
  }
  return capacity;
}

NK_API nk_size
nk_glyph_atlas_memory_size(int width, int height, int max_glyphs)
{
  nk_size size = 0;

  if (width <= 0 || height <= 0 || max_glyphs <= 0) {
    return 0;
  }

  size += (nk_size)width * (nk_size)height * 4;
  size += sizeof(struct nk_glyph_atlas_slot) * (nk_size)nk_glyph_atlas_slot_capacity(max_glyphs);
  size += sizeof(stbtt_fontinfo) * NK_GLYPH_ATLAS_MAX_FONTS;
  size += NK_GLYPH_ATLAS_SCRATCH_SIZE;
  return size + 256; /* alignment */
}

NK_INTERN void
nk_glyph_atlas_mark(struct nk_glyph_atlas *atlas, int x, int y, int w, int h)
{
  if (atlas->dirty_x1 <= atlas->dirty_x0 || atlas->dirty_y1 <= atlas->dirty_y0) {
    atlas->dirty_x0 = x;
    atlas->dirty_y0 = y;
    atlas->dirty_x1 = x + w;
    atlas->dirty_y1 = y + h;
    return;
  }

  atlas->dirty_x0 = NK_MIN(atlas->dirty_x0, x);
  atlas->dirty_y0 = NK_MIN(atlas->dirty_y0, y);
  atlas->dirty_x1 = NK_MAX(atlas->dirty_x1, x + w);
  atlas->dirty_y1 = NK_MAX(atlas->dirty_y1, y + h);
}

NK_API nk_bool
nk_glyph_atlas_init(struct nk_glyph_atlas *atlas, void *memory, nk_size size, int width, int height, int max_glyphs)
{
  nk_size pixel_size;
  int     y;

  NK_ASSERT(atlas);
  NK_ASSERT(memory);

  if (!atlas || !memory || size < nk_glyph_atlas_memory_size(width, height, max_glyphs) || width < 8 || height < 8) {
    return nk_false;
  }

  nk_zero_struct(*atlas);
  nk_buffer_init_fixed(&atlas->memory, memory, size);
  atlas->width         = width;
  atlas->height        = height;
  atlas->max_glyphs    = max_glyphs;
  atlas->slot_capacity = nk_glyph_atlas_slot_capacity(max_glyphs);

  pixel_size    = (nk_size)width * (nk_size)height * 4;
  atlas->pixels = (nk_byte *)nk_buffer_alloc(&atlas->memory, NK_BUFFER_FRONT, pixel_size, 16);
  atlas->slots  = (struct nk_glyph_atlas_slot *)nk_buffer_alloc(&atlas->memory, NK_BUFFER_FRONT,
                                                               sizeof(*atlas->slots) * (nk_size)atlas->slot_capacity,
                                                               NK_ALIGNOF(struct nk_glyph_atlas_slot));
  atlas->scratch      = (nk_byte *)nk_buffer_alloc(&atlas->memory, NK_BUFFER_FRONT, NK_GLYPH_ATLAS_SCRATCH_SIZE, 16);
  atlas->scratch_size = NK_GLYPH_ATLAS_SCRATCH_SIZE;

  if (!atlas->pixels || !atlas->slots || !atlas->scratch) {
    return nk_false;
  }

  NK_MEMSET(atlas->pixels, 0, pixel_size);
  NK_MEMSET(atlas->slots, 0, sizeof(*atlas->slots) * (nk_size)atlas->slot_capacity);
  atlas->scratch_alloc.userdata.ptr = atlas;
  atlas->scratch_alloc.alloc        = nk_glyph_atlas_scratch_alloc;
  atlas->scratch_alloc.free         = nk_glyph_atlas_scratch_free;

  /* white texel for untextured draws */
  for (y = 0; y < 3; ++y) {
    NK_MEMSET(atlas->pixels + (nk_size)y * (nk_size)width * 4, 0xff, 3 * 4);
  }

  atlas->null.uv         = nk_vec2(1.5f / (float)width, 1.5f / (float)height);
  atlas->pack_x          = 3 + NK_GLYPH_ATLAS_PADDING;
  atlas->pack_y          = 0;
  atlas->pack_row_height = 3;
  nk_glyph_atlas_mark(atlas, 0, 0, width, height);
  return nk_true;
}

NK_API void
nk_glyph_atlas_set_texture(struct nk_glyph_atlas *atlas, nk_handle texture)
{
  NK_ASSERT(atlas);

  int i;

  atlas->texture      = texture;
  atlas->null.texture = texture;

  for (i = 0; i < atlas->font_count; ++i) {
    atlas->fonts[i]->handle.texture = texture;
  }
}

NK_API nk_bool
nk_glyph_atlas_dirty(const struct nk_glyph_atlas *atlas, int *x, int *y, int *w, int *h)
{
  NK_ASSERT(atlas);

  if (atlas->dirty_x1 <= atlas->dirty_x0 || atlas->dirty_y1 <= atlas->dirty_y0) {
    return nk_false;
  }

  *x = atlas->dirty_x0;
  *y = atlas->dirty_y0;
  *w = atlas->dirty_x1 - atlas->dirty_x0;
  *h = atlas->dirty_y1 - atlas->dirty_y0;
  return nk_true;
}

NK_API void
nk_glyph_atlas_clean(struct nk_glyph_atlas *atlas)
{
  NK_ASSERT(atlas);
  atlas->dirty_x0 = atlas->dirty_y0 = atlas->dirty_x1 = atlas->dirty_y1 = 0;
}

/* shelf packing */
NK_INTERN nk_bool
nk_glyph_atlas_place(struct nk_glyph_atlas *atlas, int w, int h, int *x, int *y)
{
  int packed_w = w + NK_GLYPH_ATLAS_PADDING;
  int packed_h = h + NK_GLYPH_ATLAS_PADDING;

  if (packed_w > atlas->width || packed_h > atlas->height) {
    return nk_false;
  }

  if (atlas->pack_x + packed_w > atlas->width) {
    atlas->pack_x          = 0;
    atlas->pack_y         += atlas->pack_row_height + NK_GLYPH_ATLAS_PADDING;
    atlas->pack_row_height = 0;
  }

  if (atlas->pack_y + packed_h > atlas->height) {
    return nk_false;
  }

  *x                     = atlas->pack_x;
  *y                     = atlas->pack_y;
  atlas->pack_x         += packed_w;
  atlas->pack_row_height = NK_MAX(atlas->pack_row_height, h);
  return nk_true;
}

NK_INTERN struct nk_glyph_atlas_slot *
nk_glyph_atlas_find(struct nk_glyph_atlas *atlas, const struct nk_font *font, nk_glyph_id id, float height)
{
  nk_uint key[3];
  nk_hash hash;
  int     i;

  key[0] = (nk_uint)(nk_ptr)font;
  key[1] = id;
  NK_MEMCPY(&key[2], &height, sizeof(key[2]));
  hash = nk_murmur_hash(key, (int)sizeof(key), 0x6a7e1a5);

  for (i = 0; i < atlas->slot_capacity; ++i) {
    struct nk_glyph_atlas_slot *slot = &atlas->slots[(hash + (nk_hash)i) & (nk_hash)(atlas->slot_capacity - 1)];

    if (!slot->font || (slot->font == font && slot->id == id && slot->height == height)) {
      return slot;
    }
  }
  return NULL;
}

NK_INTERN void
nk_glyph_atlas_render(struct nk_glyph_atlas *atlas, const struct nk_font *font, nk_glyph_id id, float height,
                      struct nk_user_font_glyph *out)
{
  const stbtt_fontinfo *info  = (const stbtt_fontinfo *)font->info;
  float                 scale = stbtt_ScaleForMappingEmToPixels(info, height);
  int                   x0, y0, x1, y1, advance, bearing;
  int                   w, h, x, y, row;
  nk_byte              *alpha;

  nk_zero(out, sizeof(*out));
  stbtt_GetGlyphHMetrics(info, (int)id, &advance, &bearing);
  out->xadvance = (float)advance * scale;

  stbtt_GetGlyphBitmapBox(info, (int)id, scale, scale, &x0, &y0, &x1, &y1);
  w = x1 - x0;
  h = y1 - y0;

  if (w <= 0 || h <= 0) {
    return;
  }

  if (atlas->full || !nk_glyph_atlas_place(atlas, w, h, &x, &y)) {
    atlas->full = nk_true;
    return;
  }

  atlas->scratch_used = 0;
  alpha               = (nk_byte *)nk_glyph_atlas_scratch_alloc(nk_handle_ptr(atlas), 0, (nk_size)w * (nk_size)h);

  if (!alpha) {
    return;
  }

  stbtt_MakeGlyphBitmap(info, alpha, w, h, w, scale, scale, (int)id);

  for (row = 0; row < h; ++row) {
    nk_byte       *dst = atlas->pixels + ((nk_size)(y + row) * (nk_size)atlas->width + (nk_size)x) * 4;
    const nk_byte *src = alpha + (nk_size)row * (nk_size)w;
    int            col;

    for (col = 0; col < w; ++col) {
      dst[col * 4 + 0] = 0xff;
      dst[col * 4 + 1] = 0xff;
      dst[col * 4 + 2] = 0xff;
      dst[col * 4 + 3] = src[col];
    }
  }

  atlas->scratch_used = 0;
  nk_glyph_atlas_mark(atlas, x, y, w, h);

  out->offset = nk_vec2((float)x0, (float)y0);
  out->width  = (float)w;
  out->height = (float)h;
  out->uv[0]  = nk_vec2((float)x / (float)atlas->width, (float)y / (float)atlas->height);
  out->uv[1]  = nk_vec2((float)(x + w) / (float)atlas->width, (float)(y + h) / (float)atlas->height);
}

NK_INTERN void
nk_font_query_glyph(nk_handle handle, float height, struct nk_user_font_glyph *glyph, nk_glyph_id id)
{
  struct nk_font             *font  = (struct nk_font *)handle.ptr;
  struct nk_glyph_atlas      *atlas = font->atlas;
  struct nk_glyph_atlas_slot *slot  = nk_glyph_atlas_find(atlas, font, id, height);

  if (slot && slot->font) {
    *glyph = slot->glyph;
    return;
  }

  nk_glyph_atlas_render(atlas, font, id, height, glyph);

  if (slot && atlas->slot_count < atlas->max_glyphs) {
    /* cached even when empty, so it is not rendered again */
    slot->font         = font;
    slot->id           = id;
    slot->height       = height;
    slot->glyph        = *glyph;
    atlas->slot_count += 1;
  }
}

NK_API nk_bool
nk_font_init(struct nk_font *font, struct nk_glyph_atlas *atlas, const void *ttf, nk_size ttf_size, int face_index, float height)
{
  stbtt_fontinfo *info;
  int             offset;

  NK_ASSERT(font);
  NK_ASSERT(atlas);
  NK_ASSERT(ttf);

  if (!font || !atlas || !ttf || !ttf_size || height <= 0.0f || atlas->font_count == NK_GLYPH_ATLAS_MAX_FONTS) {
    return nk_false;
  }

  offset = stbtt_GetFontOffsetForIndex((const unsigned char *)ttf, face_index);
  info   = (stbtt_fontinfo *)nk_buffer_alloc(&atlas->memory, NK_BUFFER_FRONT, sizeof(*info), NK_ALIGNOF(stbtt_fontinfo));

  if (offset < 0 || !info) {
    return nk_false;
  }

  nk_zero(info, sizeof(*info));
  info->userdata = &atlas->scratch_alloc;

  if (!stbtt_InitFont(info, (const unsigned char *)ttf, offset)) {
    return nk_false;
  }

  nk_zero_struct(*font);
  font->atlas               = atlas;
  font->ttf                 = ttf;
  font->ttf_size            = ttf_size;
  font->info                = info;
  font->handle.userdata.ptr = font;
  font->handle.height       = height;
  font->handle.query_glyph  = nk_font_query_glyph;
  font->handle.texture      = atlas->texture;
  atlas->fonts[atlas->font_count++] = font;
  return nk_true;
}
#endif /* NK_INCLUDE_GLYPH_ATLAS */

/* ===============================================================
 *
 *                          INPUT
 *
 * ===============================================================*/
NK_API void
nk_input_begin(struct nk_context *ctx)
{
  int              i;
  struct nk_input *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in = &ctx->input;

  for (i = 0; i < NK_BUTTON_MAX; ++i) {
    in->mouse.buttons[i].clicked = 0;
  }

  in->keyboard.text_len  = 0;
  in->mouse.scroll_delta = nk_vec2(0, 0);
  in->mouse.prev.x       = in->mouse.pos.x;
  in->mouse.prev.y       = in->mouse.pos.y;
  in->mouse.delta.x      = 0;
  in->mouse.delta.y      = 0;

  for (i = 0; i < NK_KEY_MAX; i++) {
    in->keyboard.keys[i].clicked = 0;
  }
}

NK_API void
nk_input_end(struct nk_context *ctx)
{
  struct nk_input *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in = &ctx->input;
  nk_nav_update(ctx);

  if (in->mouse.grab) {
    in->mouse.grab = 0;
  }

  if (in->mouse.ungrab) {
    in->mouse.grabbed = 0;
    in->mouse.ungrab  = 0;
    in->mouse.grab    = 0;
  }
}

NK_API void
nk_input_motion(struct nk_context *ctx, int x, int y)
{
  struct nk_input *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in                = &ctx->input;
  in->mouse.pos.x   = (float)x;
  in->mouse.pos.y   = (float)y;
  in->mouse.delta.x = in->mouse.pos.x - in->mouse.prev.x;
  in->mouse.delta.y = in->mouse.pos.y - in->mouse.prev.y;
}

NK_API void
nk_input_key(struct nk_context *ctx, enum nk_keys key, nk_bool down)
{
  struct nk_input *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in = &ctx->input;
#ifdef NK_KEYSTATE_BASED_INPUT
  if (in->keyboard.keys[key].down != down) {
    in->keyboard.keys[key].clicked++;
  }
#else
  /* a release without a press is no event (platforms re-send releases) */
  if (down || in->keyboard.keys[key].down) {
    in->keyboard.keys[key].clicked++;
  }
#endif
  in->keyboard.keys[key].down = down;
}

NK_API void
nk_input_button(struct nk_context *ctx, enum nk_buttons id, int x, int y, nk_bool down)
{
  struct nk_mouse_button *btn;
  struct nk_input        *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in = &ctx->input;

  if (in->mouse.buttons[id].down == down) {
    return;
  }

  btn                = &in->mouse.buttons[id];
  btn->clicked_pos.x = (float)x;
  btn->clicked_pos.y = (float)y;
  btn->down          = down;
  btn->clicked++;

  /* Fix Click-Drag for touch events. */
  in->mouse.delta.x = 0;
  in->mouse.delta.y = 0;
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
  if (down == 1 && id == NK_BUTTON_LEFT) {
    in->mouse.down_pos.x = btn->clicked_pos.x;
    in->mouse.down_pos.y = btn->clicked_pos.y;
  }
#endif
}

NK_API void
nk_input_scroll(struct nk_context *ctx, struct nk_vec2 val)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  ctx->input.mouse.scroll_delta.x += val.x;
  ctx->input.mouse.scroll_delta.y += val.y;
}

NK_API void
nk_input_glyph(struct nk_context *ctx, const nk_glyph glyph)
{
  int              len = 0;
  nk_rune          unicode;
  struct nk_input *in;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  in = &ctx->input;

  len = nk_utf_decode(glyph, &unicode, NK_UTF_SIZE);

  if (len && ((in->keyboard.text_len + len) < NK_INPUT_MAX)) {
    nk_utf_encode(unicode, &in->keyboard.text[in->keyboard.text_len], NK_INPUT_MAX - in->keyboard.text_len);
    in->keyboard.text_len += len;
  }
}

NK_API void
nk_input_char(struct nk_context *ctx, char c)
{
  nk_glyph glyph = {0};

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  glyph[0] = c;
  nk_input_glyph(ctx, glyph);
}

NK_API void
nk_input_unicode(struct nk_context *ctx, nk_rune unicode)
{
  nk_glyph rune;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  nk_utf_encode(unicode, rune, NK_UTF_SIZE);
  nk_input_glyph(ctx, rune);
}

NK_API nk_bool
nk_input_has_mouse_click(const struct nk_input *i, enum nk_buttons id)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];
  return (btn->clicked && btn->down == nk_false) ? nk_true : nk_false;
}

NK_API nk_bool
nk_input_has_mouse_click_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];

  if (!NK_INBOX(btn->clicked_pos.x, btn->clicked_pos.y, b.x, b.y, b.w, b.h)) {
    return nk_false;
  }
  return nk_true;
}

NK_API nk_bool
nk_input_has_mouse_click_in_button_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
  if (!NK_INBOX(btn->clicked_pos.x, btn->clicked_pos.y, b.x, b.y, b.w, b.h) || !NK_INBOX(i->mouse.down_pos.x, i->mouse.down_pos.y, b.x, b.y, b.w, b.h))
#else
  if (!NK_INBOX(btn->clicked_pos.x, btn->clicked_pos.y, b.x, b.y, b.w, b.h))
#endif
    return nk_false;
  return nk_true;
}

NK_API nk_bool
nk_input_has_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b, nk_bool down)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];
  return nk_input_has_mouse_click_in_rect(i, id, b) && (btn->down == down);
}

NK_API nk_bool
nk_input_is_mouse_click_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];
  return (nk_input_has_mouse_click_down_in_rect(i, id, b, nk_false) && btn->clicked) ? nk_true : nk_false;
}

NK_API nk_bool
nk_input_is_mouse_click_down_in_rect(const struct nk_input *i, enum nk_buttons id, struct nk_rect b, nk_bool down)
{
  const struct nk_mouse_button *btn;

  if (!i) {
    return nk_false;
  }

  btn = &i->mouse.buttons[id];
  return (nk_input_has_mouse_click_down_in_rect(i, id, b, down) && btn->clicked) ? nk_true : nk_false;
}

NK_API nk_bool
nk_input_any_mouse_click_in_rect(const struct nk_input *in, struct nk_rect b)
{
  int i, down = 0;

  for (i = 0; i < NK_BUTTON_MAX; ++i) {
    down = down || nk_input_is_mouse_click_in_rect(in, (enum nk_buttons)i, b);
  }
  return down;
}

NK_API nk_bool
nk_input_is_mouse_hovering_rect(const struct nk_input *i, struct nk_rect rect)
{
  if (!i) {
    return nk_false;
  }
  return NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, rect.x, rect.y, rect.w, rect.h);
}

NK_API nk_bool
nk_input_is_mouse_prev_hovering_rect(const struct nk_input *i, struct nk_rect rect)
{
  if (!i) {
    return nk_false;
  }
  return NK_INBOX(i->mouse.prev.x, i->mouse.prev.y, rect.x, rect.y, rect.w, rect.h);
}

NK_API nk_bool
nk_input_mouse_clicked(const struct nk_input *i, enum nk_buttons id, struct nk_rect rect)
{
  if (!i) {
    return nk_false;
  }

  if (!nk_input_is_mouse_hovering_rect(i, rect)) {
    return nk_false;
  }
  return nk_input_is_mouse_click_in_rect(i, id, rect);
}

NK_API nk_bool
nk_input_is_mouse_down(const struct nk_input *i, enum nk_buttons id)
{
  if (!i) {
    return nk_false;
  }
  return i->mouse.buttons[id].down;
}

NK_API nk_bool
nk_input_is_mouse_pressed(const struct nk_input *i, enum nk_buttons id)
{
  const struct nk_mouse_button *b;

  if (!i) {
    return nk_false;
  }

  b = &i->mouse.buttons[id];

  if (b->down && b->clicked) {
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_input_is_mouse_released(const struct nk_input *i, enum nk_buttons id)
{
  if (!i) {
    return nk_false;
  }
  return (!i->mouse.buttons[id].down && i->mouse.buttons[id].clicked);
}

NK_API nk_bool
nk_input_is_mouse_moved(const struct nk_input *i)
{
  if (!i) {
    return nk_false;
  }
  return i->mouse.delta.x != 0 || i->mouse.delta.y != 0;
}

NK_API nk_bool
nk_input_is_key_pressed(const struct nk_input *i, enum nk_keys key)
{
  const struct nk_key *k;

  if (!i) {
    return nk_false;
  }

  k = &i->keyboard.keys[key];

  if ((k->down && k->clicked) || (!k->down && k->clicked >= 2)) {
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_input_is_key_released(const struct nk_input *i, enum nk_keys key)
{
  const struct nk_key *k;

  if (!i) {
    return nk_false;
  }

  k = &i->keyboard.keys[key];

  if ((!k->down && k->clicked) || (k->down && k->clicked >= 2)) {
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_input_is_key_down(const struct nk_input *i, enum nk_keys key)
{
  const struct nk_key *k;

  if (!i) {
    return nk_false;
  }

  k = &i->keyboard.keys[key];

  if (k->down) {
    return nk_true;
  }
  return nk_false;
}

/* ===============================================================
 *
 *                          NAVIGATION
 *
 * ===============================================================*/
#define NK_NAV_REPEAT_DELAY 0.35f
#define NK_NAV_REPEAT_RATE  0.1f
#define NK_NAV_FAST_DELAY   1.0f   /* held this long, values change ten steps at a time */
#define NK_NAV_SCROLL_SPEED 900.0f /* pixels a second */

NK_API void
nk_nav_enable(struct nk_context *ctx, nk_bool enable)
{
  NK_ASSERT(ctx);
  ctx->nav.enabled = enable;

  if (!enable) {
    ctx->nav.active = nk_false;
    ctx->nav.focus  = 0;
  }
}

NK_API nk_bool
nk_nav_active(const struct nk_context *ctx)
{
  return ctx && ctx->nav.enabled && ctx->nav.active;
}

/* popups count as their parent window */
NK_INTERN nk_hash
nk_nav_owner(const struct nk_window *win)
{
  return (!win->name && win->parent) ? win->parent->name : win->name;
}

NK_LIB nk_bool
nk_nav_stop(struct nk_context *ctx, struct nk_rect bounds, nk_flags flags)
{
  return nk_nav_stop_keyed(ctx, bounds, flags, 0);
}

NK_LIB nk_bool
nk_nav_stop_keyed(struct nk_context *ctx, struct nk_rect bounds, nk_flags flags, nk_hash key)
{
  struct nk_nav      *nav;
  struct nk_window   *win;
  struct nk_panel    *layout;
  struct nk_nav_stop *stop;
  nk_hash             id;

  if (!ctx || !ctx->nav.enabled || ctx->nav.suppress || !ctx->current || !ctx->current->layout) {
    return nk_false;
  }

  nav    = &ctx->nav;
  win    = ctx->current;
  layout = win->layout;

  if (win->widgets_disabled || (win->flags & (NK_WINDOW_NO_INPUT | NK_WINDOW_NO_NAV)) || (win->parent && (win->parent->flags & NK_WINDOW_NO_NAV))) {
    return nk_false;
  }

  if (key) {
    id = nk_murmur_hash(&key, (int)sizeof(key), layout->text_selection_scope);
  } else {
    int place[2];

    place[0] = (flags & NK_NAV_STOP_PANEL) ? -1 : (int)(bounds.x - layout->bounds.x + (float)*layout->offset_x);
    place[1] = (flags & NK_NAV_STOP_PANEL) ? -1 : (int)(bounds.y - layout->bounds.y + (float)*layout->offset_y);
    id       = nk_murmur_hash(place, (int)sizeof(place), layout->text_selection_scope);
  }

  id = id ? id : 1;

  if (nav->count[nav->current] < NK_NAV_MAX_STOPS) {
    stop         = &nav->stops[nav->current][nav->count[nav->current]++];
    stop->id     = id;
    stop->window = win->name;

    if (!stop->window && win->parent) {
      stop->window = nk_murmur_hash(&win->parent->name, (int)sizeof(win->parent->name), NK_PANEL_POPUP);
    }

    stop->popup        = (layout->type & NK_PANEL_SET_POPUP) != 0;
    stop->flags        = flags;
    stop->bounds       = bounds;
    stop->clip         = layout->clip;
    /* NK_WINDOW_NO_SCROLLBAR is both bits */
    stop->scroll_x     = (layout->flags & NK_WINDOW_NO_SCROLLBAR_H) ? 0 : layout->offset_x;
    stop->scroll_y     = (layout->flags & NK_WINDOW_NO_SCROLLBAR_V) ? 0 : layout->offset_y;
    stop->scroll_max_x = layout->prev_max_x;
    stop->scroll_max_y = layout->prev_max_y;
  }

  if (id != nav->focus) {
    return nk_false;
  }

  nav->focus_bounds = bounds;
  nav->focus_window = nk_nav_owner(win);

  if (nav->active && !(flags & NK_NAV_STOP_HIDDEN)) {
    nk_bool        active = nav->captured || (flags & NK_NAV_STOP_ACTIVE);
    struct nk_rect old    = win->buffer.clip;
    struct nk_rect grown  = nk_rect(layout->clip.x - 3, layout->clip.y - 3, layout->clip.w + 6, layout->clip.h + 6);
    struct nk_rect clip;

    /* the outline is outside the widget: it may go past the panel, not past the window */
    nk_unify(&clip, &grown, win->bounds.x, win->bounds.y, win->bounds.x + win->bounds.w, win->bounds.y + win->bounds.h);
    nk_push_scissor(&win->buffer, clip);
    nk_stroke_rect(&win->buffer, nk_rect(bounds.x - 2, bounds.y - 2, bounds.w + 4, bounds.h + 4), 2, 2,
                   active ? ctx->style.nav_active : ctx->style.nav_highlight);
    nk_push_scissor(&win->buffer, old);
  }
  return nk_true;
}

NK_LIB void
nk_nav_next_frame(struct nk_context *ctx)
{
  ctx->nav.current                ^= 1;
  ctx->nav.count[ctx->nav.current] = 0;
}

NK_INTERN int
nk_nav_index(const struct nk_nav_stop *stops, int count, nk_hash id)
{
  int i;

  for (i = 0; id && i < count; ++i) {
    if (stops[i].id == id) {
      return i;
    }
  }
  return -1;
}

/* the distance from f towards key, aligned ones nearer; false when c is not that way */
NK_INTERN nk_bool
nk_nav_score(struct nk_rect f, struct nk_rect c, int key, float *score)
{
  float dx = c.x + c.w * 0.5f - (f.x + f.w * 0.5f);
  float dy = c.y + c.h * 0.5f - (f.y + f.h * 0.5f);
  float primary, gap, reach;

  if (key == NK_KEY_NAV_UP || key == NK_KEY_NAV_DOWN) {
    primary = key == NK_KEY_NAV_DOWN ? dy : -dy;
    gap     = NK_ABS(dx) <= NK_MAX(c.w, f.w) * 0.5f ? 0.0f : NK_ABS(dx);
    reach   = f.h * 0.5f;
  } else {
    primary = key == NK_KEY_NAV_RIGHT ? dx : -dx;
    gap     = NK_ABS(dy) <= NK_MAX(c.h, f.h) * 0.5f ? 0.0f : NK_ABS(dy);
    reach   = f.w * 0.5f;
  }

  if (primary <= reach) {
    return nk_false;
  }

  *score = primary + gap * 4.0f;
  return nk_true;
}

/* -1 when there is none */
NK_INTERN int
nk_nav_find(const struct nk_nav_stop *stops, int count, int from, int key)
{
  float best  = 0;
  int   found = -1;
  int   i;

  for (i = 0; i < count; ++i) {
    float score;

    if (i != from && stops[i].window == stops[from].window && nk_nav_score(stops[from].bounds, stops[i].bounds, key, &score) &&
        (found < 0 || score < best)) {
      best  = score;
      found = i;
    }
  }
  return found;
}

NK_INTERN struct nk_vec2
nk_nav_scroll_into_view(const struct nk_nav_stop *stop)
{
  struct nk_vec2 delta = {0, 0};

  if (stop->scroll_y) {
    float top    = stop->clip.y;
    float bottom = stop->clip.y + stop->clip.h;

    if (stop->bounds.y < top) {
      delta.y = -NK_MIN(top - stop->bounds.y, (float)*stop->scroll_y);
    } else if (stop->bounds.y + stop->bounds.h > bottom) {
      delta.y = NK_MIN(stop->bounds.y + stop->bounds.h - bottom, stop->bounds.y - top);
    }

    *stop->scroll_y = (nk_uint)((float)*stop->scroll_y + delta.y);
  }

  if (stop->scroll_x) {
    float left  = stop->clip.x;
    float right = stop->clip.x + stop->clip.w;

    if (stop->bounds.x < left) {
      delta.x = -NK_MIN(left - stop->bounds.x, (float)*stop->scroll_x);
    } else if (stop->bounds.x + stop->bounds.w > right) {
      delta.x = NK_MIN(stop->bounds.x + stop->bounds.w - right, stop->bounds.x - left);
    }

    *stop->scroll_x = (nk_uint)((float)*stop->scroll_x + delta.x);
  }
  return delta;
}

/* top-most, then left-most, visible ones first */
NK_INTERN int
nk_nav_first(const struct nk_nav_stop *stops, int count, nk_hash window)
{
  int found = -1;
  int pass, i;

  for (pass = 0; pass < 2 && found < 0; ++pass) {
    for (i = 0; i < count; ++i) {
      if (stops[i].window == window && (pass || !(stops[i].flags & NK_NAV_STOP_HIDDEN)) &&
          (found < 0 || stops[i].bounds.y < stops[found].bounds.y ||
           (stops[i].bounds.y == stops[found].bounds.y && stops[i].bounds.x < stops[found].bounds.x))) {
        found = i;
      }
    }
  }
  return found;
}

NK_INTERN struct nk_window *
nk_nav_find_window(struct nk_context *ctx, nk_hash name)
{
  struct nk_window *win;

  for (win = ctx->begin; name && win; win = win->next) {
    if (win->name == name) {
      return win;
    }
  }
  return 0;
}

NK_API nk_bool
nk_nav_cancel_unused(const struct nk_context *ctx)
{
  return ctx && ctx->nav.enabled && ctx->nav.cancel;
}

NK_API struct nk_window *
nk_nav_focus_window(struct nk_context *ctx)
{
  if (!ctx || !ctx->nav.enabled || !ctx->nav.focus) {
    return 0;
  }
  return nk_nav_find_window(ctx, ctx->nav.focus_window);
}

NK_INTERN void
nk_nav_remember(struct nk_nav *nav, nk_hash window, nk_hash focus)
{
  int i, slot = NK_NAV_MAX_WINDOWS - 1;

  for (i = 0; i < NK_NAV_MAX_WINDOWS; ++i) {
    if (nav->remembered[i].window == window || !nav->remembered[i].window) {
      slot = i;
      break;
    }
  }

  for (i = slot; i > 0; --i) {
    nav->remembered[i] = nav->remembered[i - 1];
  }

  nav->remembered[0].window = window;
  nav->remembered[0].focus  = focus;
}

NK_INTERN nk_hash
nk_nav_recall(const struct nk_nav *nav, nk_hash window)
{
  int i;

  for (i = 0; i < NK_NAV_MAX_WINDOWS; ++i) {
    if (nav->remembered[i].window == window) {
      return nav->remembered[i].focus;
    }
  }
  return 0;
}

/* 0 for none */
NK_INTERN nk_hash
nk_nav_cycle(const struct nk_nav_stop *stops, int count, nk_hash from, int step)
{
  nk_hash windows[NK_NAV_MAX_WINDOWS];
  int     n = 0, at = -1, i, j;

  for (i = 0; i < count; ++i) {
    for (j = 0; j < n && windows[j] != stops[i].window; ++j) {
    }

    if (j == n && n < NK_NAV_MAX_WINDOWS) {
      windows[n++] = stops[i].window;
    }
  }

  for (i = 0; i < n; ++i) {
    if (windows[i] == from) {
      at = i;
    }
  }

  if (n < 2 || at < 0) {
    return n && at < 0 ? windows[0] : 0;
  }
  return windows[(at + step + n) % n];
}

/* a tap within one frame counts */
NK_INTERN nk_bool
nk_nav_pressed(const struct nk_input *in, enum nk_keys key)
{
  const struct nk_key *k = &in->keyboard.keys[key];
  return k->clicked && (k->down || k->clicked > 1);
}

NK_INTERN void
nk_nav_button(struct nk_context *ctx, nk_bool *state, nk_bool down, enum nk_buttons button)
{
  struct nk_input *in = &ctx->input;

  if (*state != down) {
    *state = down;
    nk_input_button(ctx, button, (int)in->mouse.pos.x, (int)in->mouse.pos.y, down);
  }
}

NK_INTERN void
nk_nav_toggle_maximize(struct nk_context *ctx, struct nk_window *w)
{
  struct nk_rect target = ctx->window_maximize_bounds;

  if (!w || !(w->flags & NK_WINDOW_MAXIMIZABLE)) {
    return;
  }

  if (w->flags & NK_WINDOW_MAXIMIZED) {
    w->bounds = w->restore_bounds;
    w->flags &= (nk_flags)~NK_WINDOW_MAXIMIZED;
  } else if (target.w > 0.0f && target.h > 0.0f) {
    w->restore_bounds = w->bounds;
    w->bounds         = target;
    w->flags         |= NK_WINDOW_MAXIMIZED;
    w->flags         &= (nk_flags)~NK_WINDOW_MINIMIZED;
  }
}

NK_INTERN void
nk_nav_window_menu(struct nk_context *ctx, struct nk_window *win)
{
  struct nk_nav *nav = &ctx->nav;
  nk_flags       f   = win->flags;
  nk_bool        max = (f & NK_WINDOW_MAXIMIZED) != 0;
  float          row = ctx->style.font_size + 2.0f * ctx->style.button.padding.y + 4.0f;
  int            n       = !!(f & NK_WINDOW_MAXIMIZABLE) + !!(f & NK_WINDOW_MINIMIZABLE) + (!max && (f & NK_WINDOW_MOVABLE)) +
                           (!max && (f & NK_WINDOW_SCALABLE)) + !!(f & NK_WINDOW_CLOSABLE);
  float          h    = (float)n * (row + ctx->style.window.spacing.y) + 2.0f * ctx->style.window.padding.y + 4.0f;
  nk_bool        done = nk_false;
  struct nk_rect rect = nk_rect(0, 0, NK_MIN(ctx->style.font_size * 9.0f, win->bounds.w), h);

  if (!n || !nk_popup_begin(ctx, NK_POPUP_STATIC, "##nk_window_menu", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR, rect)) {
    nav->window_menu = 0;
    return;
  }

  nk_layout_row_dynamic(ctx, row, 1);

  if ((f & NK_WINDOW_MAXIMIZABLE) && nk_button_label(ctx, max ? "Restore" : "Maximize")) {
    nk_nav_toggle_maximize(ctx, win);
    done = nk_true;
  }

  if ((f & NK_WINDOW_MINIMIZABLE) && nk_button_label(ctx, "Minimize")) {
    win->flags |= NK_WINDOW_MINIMIZED;
    done        = nk_true;
  }

  if (!max && (f & NK_WINDOW_MOVABLE) && nk_button_label(ctx, "Move")) {
    nav->window_mode = 1;
    nav->mode_window = win->name;
    done             = nk_true;
  }

  if (!max && (f & NK_WINDOW_SCALABLE) && nk_button_label(ctx, "Resize")) {
    nav->window_mode = 2;
    nav->mode_window = win->name;
    done             = nk_true;
  }

  if ((f & NK_WINDOW_CLOSABLE) && nk_button_label(ctx, "Close")) {
    nav->close_window = win->name;
    done              = nk_true;
  }

  if (done) {
    nav->window_menu = 0;
    nk_popup_close(ctx);
  }

  nk_popup_end(ctx);
  win->layout->flags = (win->layout->flags & ~(nk_flags)(NK_WINDOW_MAXIMIZED | NK_WINDOW_MINIMIZED)) |
                       (win->flags & (NK_WINDOW_MAXIMIZED | NK_WINDOW_MINIMIZED));
}

NK_INTERN void
nk_osk_input(struct nk_context *ctx);

NK_LIB void
nk_nav_update(struct nk_context *ctx)
{
  struct nk_nav            *nav      = &ctx->nav;
  struct nk_input          *in       = &ctx->input;
  const struct nk_nav_stop *stops    = nav->stops[!nav->current];
  int                       count    = nav->count[!nav->current];
  nk_bool                   editing  = nav->editing;
  nk_bool                   popup    = nk_false;
  nk_bool                   started  = nk_false;
  int                       key      = 0;
  int                       cycle    = 0;
  int                       scroll   = 0;
  int                       tab      = 0;
  int                       towards  = 0;
  nk_bool                   deferred = nk_false;
  nk_bool                   maximize = nk_false;
  nk_bool                   click    = nk_true;
  int                       focus;
  nk_hash                   scope = 0;
  int                       i;

  nav->adjust       = 0;
  nav->adjust_y     = 0;
  nav->leave        = nk_false;
  nav->editing_last = editing;
  nav->editing      = nk_false;
  nav->text_request = nk_false;
  ctx->edit_blink  += ctx->delta_time_seconds;

  if (nav->osk.open) {
    /* the on-screen keyboard takes the navigation keys */
    static const enum nk_keys taken[] = {NK_KEY_ENTER, NK_KEY_UP, NK_KEY_DOWN, NK_KEY_LEFT, NK_KEY_RIGHT};

    for (i = 0; i < (int)NK_LEN(taken); ++i) {
      in->keyboard.keys[taken[i]].clicked = 0;
    }
  }

  if (nav->osk.open || nav->osk.release_count) {
    nk_osk_input(ctx);
  }

  /* a cancel that nothing used last frame closes the window */
  if (nav->enabled && nav->cancel && !nav->close_window) {
    struct nk_window *w = nav->active && nav->focus ? nk_nav_find_window(ctx, nav->focus_window) : ctx->active;

    if (w && (w->flags & (NK_WINDOW_CLOSABLE | NK_WINDOW_CLOSE_BUTTON_HIDES)) && !(w->flags & (NK_WINDOW_HIDDEN | NK_WINDOW_CLOSED))) {
      nav->close_window = w->name;
    }
  }

  if (nav->close_window) {
    struct nk_window *w = nk_nav_find_window(ctx, nav->close_window);

    nav->close_window = 0;

    if (w) {
      w->flags |= NK_WINDOW_HIDDEN;

      if (!(w->flags & NK_WINDOW_CLOSE_BUTTON_HIDES)) {
        w->flags |= NK_WINDOW_CLOSED;
      }

      w->flags &= (nk_flags)~NK_WINDOW_MINIMIZED;
    }
  }

  nav->cancel = nk_nav_pressed(in, NK_KEY_NAV_CANCEL);

  if (!nav->enabled) {
    return;
  }

  if (nav->active && (in->mouse.delta.x != 0 || in->mouse.delta.y != 0 || in->mouse.buttons[NK_BUTTON_LEFT].clicked ||
                      in->mouse.buttons[NK_BUTTON_RIGHT].clicked)) {
    nav->active     = nk_false;
    nav->activate   = nk_false;
    nav->menu       = nk_false;
    nav->captured   = nk_false;
    nav->repeat_key = 0;
    return;
  }

  /* repeated while held */
  for (i = NK_KEY_NAV_UP; i <= NK_KEY_NAV_RIGHT; ++i) {
    if (nk_nav_pressed(in, (enum nk_keys)i)) {
      key              = i;
      nav->repeat_key  = i;
      nav->repeat_time = 0;
    }
  }

  if (!key && nav->repeat_key) {
    if (!in->keyboard.keys[nav->repeat_key].down) {
      nav->repeat_key = 0;
    } else {
      float before = nav->repeat_time;

      nav->repeat_time += ctx->delta_time_seconds;
      ctx->animating    = nk_true;

      if (nav->repeat_time >= NK_NAV_REPEAT_DELAY &&
          (before < NK_NAV_REPEAT_DELAY ||
           (int)((nav->repeat_time - NK_NAV_REPEAT_DELAY) / NK_NAV_REPEAT_RATE) != (int)((before - NK_NAV_REPEAT_DELAY) / NK_NAV_REPEAT_RATE))) {
        key = nav->repeat_key;
      }
    }
  }

  /* on the on-screen keyboard they move its text cursor */
  if (!nav->osk.open) {
    cycle = nk_nav_pressed(in, NK_KEY_NAV_WINDOW_NEXT) ? 1 : nk_nav_pressed(in, NK_KEY_NAV_WINDOW_PREV) ? -1 : 0;
    tab   = nk_nav_pressed(in, NK_KEY_FOCUS_NEXT) ? 1 : nk_nav_pressed(in, NK_KEY_FOCUS_PREV) ? -1 : 0;

    for (i = NK_KEY_NAV_WINDOW_UP; i <= NK_KEY_NAV_WINDOW_RIGHT; ++i) {
      if (nk_nav_pressed(in, (enum nk_keys)i)) {
        towards = NK_KEY_NAV_UP + (i - NK_KEY_NAV_WINDOW_UP);
      }
    }

    maximize = nk_nav_pressed(in, NK_KEY_NAV_WINDOW_MAXIMIZE);
  }

  if ((tab || towards) && editing) {
    nav->leave = nk_true;
  }

  for (i = NK_KEY_NAV_SCROLL_UP; i <= NK_KEY_NAV_SCROLL_RIGHT; ++i) {
    if (in->keyboard.keys[i].down || nk_nav_pressed(in, (enum nk_keys)i)) {
      scroll = i;
    }
  }

  if (editing && !nav->osk.open) {
    key             = 0;
    cycle           = 0;
    scroll          = 0;
    nav->repeat_key = 0;
  }

  if (!nav->active && (key || cycle || scroll || tab || towards || maximize || (!editing && (nk_nav_pressed(in, NK_KEY_NAV_ACTIVATE) ||
                                                      nk_nav_pressed(in, NK_KEY_NAV_MENU))))) {
    nav->active = nk_true;
    started     = nk_true;
  }

  if (!nav->active) {
    nav->focus_request = 0;
    return;
  }

  if (!key && nav->deferred) {
    key      = nav->deferred;
    deferred = nk_true;
  }

  nav->deferred = 0;

  if (key && !deferred && !nav->activate && nk_nav_pressed(in, NK_KEY_NAV_ACTIVATE) && (!editing || nav->osk.open)) {
    nav->deferred = key;
    key           = 0;
  } else if (key && nav->activate) {
    /* the virtual click is let go where it was pressed first */
    nk_nav_button(ctx, &nav->activate, nk_false, NK_BUTTON_LEFT);
    nav->deferred = key;
    key           = 0;
  }

  for (i = 0; i < count; ++i) {
    if (stops[i].popup) {
      scope = stops[i].window;
      popup = nk_true;
    }
  }

  if (!popup && nav->osk.open && nav->osk.name) {
    scope = nav->osk.name;
  }

  focus = nk_nav_index(stops, count, nav->focus);

  if (focus < 0 && nav->focus && !popup && nk_nav_index(stops, count, nav->return_focus) < 0) {
    /* the focus is gone: the nearest stop of its window */
    float best = 0;

    for (i = 0; i < count; ++i) {
      if (stops[i].window == nav->focus_window && !(stops[i].flags & NK_NAV_STOP_HIDDEN)) {
        float dx = stops[i].bounds.x - nav->focus_bounds.x, dy = stops[i].bounds.y - nav->focus_bounds.y;
        float d  = dx * dx + dy * dy;

        if (focus < 0 || d < best) {
          best  = d;
          focus = i;
        }
      }
    }
  }

  if (started) {
    int under = -1;

    for (i = 0; i < count; ++i) {
      if ((!scope || stops[i].window == scope) && !(stops[i].flags & NK_NAV_STOP_HIDDEN) &&
          NK_INBOX(in->mouse.pos.x, in->mouse.pos.y, stops[i].bounds.x, stops[i].bounds.y, stops[i].bounds.w, stops[i].bounds.h)) {
        under = i;
      }
    }

    if (under >= 0) {
      focus = under;
    } else if (!popup && ctx->active && nk_nav_first(stops, count, ctx->active->name) >= 0 &&
               (focus < 0 || stops[focus].window != ctx->active->name)) {
      focus = nk_nav_index(stops, count, nk_nav_recall(nav, ctx->active->name));

      if (focus < 0) {
        focus = nk_nav_first(stops, count, ctx->active->name);
      }

      key = 0;
    }
  }

  /* from nk_window_set_focus, once the window has stops */
  if (nav->focus_request) {
    nk_hash want = nav->focus_request;
    int     first = nk_nav_first(stops, count, want);

    if (first >= 0 || popup || !nk_nav_find_window(ctx, want)) {
      nav->focus_request = 0;
    }

    if (first >= 0 && !popup && (focus < 0 || stops[focus].window != want)) {
      if (focus >= 0) {
        nk_nav_remember(nav, stops[focus].window, stops[focus].id);
      }

      focus = nk_nav_index(stops, count, nk_nav_recall(nav, want));

      if (focus < 0) {
        focus = first;
      }

      key = 0;
    }
  }

  if (!scope) {
    scope = focus >= 0 ? stops[focus].window : ctx->active ? ctx->active->name : count ? stops[0].window : 0;
  }

  if (focus < 0 && !popup && nk_nav_first(stops, count, scope) < 0) {
    struct nk_window *win;

    for (win = ctx->end; win; win = win->prev) {
      if (nk_nav_first(stops, count, win->name) >= 0) {
        scope = win->name;
        break;
      }
    }
  }

  if (towards && !popup) {
    struct nk_window *from       = nk_nav_find_window(ctx, scope);
    struct nk_window *best       = 0;
    float             best_score = 0;

    for (i = 0; from && i < count; ++i) {
      struct nk_window *w = (i == 0 || stops[i].window != stops[i - 1].window) ? nk_nav_find_window(ctx, stops[i].window) : 0;
      float             score;
      struct nk_rect f = from->bounds, c = w ? w->bounds : from->bounds;

      if (w && w != from && !(w->flags & NK_WINDOW_HIDDEN)) {
        f = nk_rect(f.x + f.w * 0.5f, f.y + f.h * 0.5f, 0, 0);
        c = nk_rect(c.x + c.w * 0.5f - 0.5f, c.y + c.h * 0.5f - 0.5f, 1, 1);
      }

      if (w && w != from && !(w->flags & NK_WINDOW_HIDDEN) && nk_nav_score(f, c, towards, &score) &&
          (!best || score < best_score)) {
        best       = w;
        best_score = score;
      }
    }

    if (best) {
      cycle = 0;

      if (focus >= 0) {
        nk_nav_remember(nav, stops[focus].window, stops[focus].id);
      }

      if (ctx->end != best) {
        nk_remove_window(ctx, best);
        nk_insert_window(ctx, best, NK_INSERT_BACK);
      }

      ctx->active = best;
      scope       = best->name;
      focus       = nk_nav_index(stops, count, nk_nav_recall(nav, best->name));

      if (focus < 0) {
        focus = nk_nav_first(stops, count, best->name);
      }

      key = 0;
    }
  }

  if (maximize && !popup) {
    nk_nav_toggle_maximize(ctx, nk_nav_find_window(ctx, scope));
  }

  if (nk_nav_pressed(in, NK_KEY_NAV_WINDOW_MENU) && !popup && !nav->osk.open) {
    struct nk_window *w = nk_nav_find_window(ctx, scope);

    if (w && (w->flags & NK_WINDOW_MINIMIZED)) {
      w->flags &= (nk_flags)~NK_WINDOW_MINIMIZED;
    } else if (w && (w->flags & (NK_WINDOW_MAXIMIZABLE | NK_WINDOW_MINIMIZABLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_CLOSABLE))) {
      nav->window_menu = w->name;
    }

    key = 0;
  }

  if (nav->window_mode) {
    if (!nk_nav_find_window(ctx, nav->mode_window) || nk_nav_pressed(in, NK_KEY_NAV_ACTIVATE) || nav->cancel) {
      nav->window_mode = 0;
      nav->cancel      = nk_false;
      click            = nk_false;
      key              = 0;
    } else {
      scope = nav->mode_window;
    }
  }

  if (key && (in->keyboard.keys[NK_KEY_NAV_WINDOW_MOVE].down || nav->window_mode == 1) && !popup) {
    struct nk_window *w = nk_nav_find_window(ctx, scope);

    if (w && (w->flags & NK_WINDOW_MOVABLE) && !(w->flags & NK_WINDOW_MAXIMIZED)) {
      struct nk_rect area = ctx->window_maximize_bounds;
      float          dx   = key == NK_KEY_NAV_LEFT ? -16.0f : key == NK_KEY_NAV_RIGHT ? 16.0f : 0.0f;
      float          dy   = key == NK_KEY_NAV_UP ? -16.0f : key == NK_KEY_NAV_DOWN ? 16.0f : 0.0f;

      w->bounds.x += dx;
      w->bounds.y += dy;

      if (area.w > 0 && area.h > 0) {
        w->bounds.x = NK_CLAMP(area.x - w->bounds.w + 32.0f, w->bounds.x, area.x + area.w - 32.0f);
        w->bounds.y = NK_CLAMP(area.y, w->bounds.y, area.y + area.h - 32.0f);
      }
    }

    key = 0;
  }

  if (key && (in->keyboard.keys[NK_KEY_NAV_WINDOW_SIZE].down || nav->window_mode == 2) && !popup) {
    /* right and down grow */
    struct nk_window *w    = nk_nav_find_window(ctx, scope);
    float             step = 16.0f; /* Shift is part of the resize chord */

    if (w && (w->flags & NK_WINDOW_SCALABLE) && !(w->flags & NK_WINDOW_MAXIMIZED)) {
      struct nk_vec2 min = ctx->style.window.min_size;

      if (key == NK_KEY_NAV_RIGHT || key == NK_KEY_NAV_LEFT) {
        w->bounds.w = NK_MAX(min.x, w->bounds.w + (key == NK_KEY_NAV_RIGHT ? step : -step));
      } else {
        w->bounds.h = NK_MAX(min.y, w->bounds.h + (key == NK_KEY_NAV_DOWN ? step : -step));
      }
    }

    key = 0;
  }

  if (cycle && !popup && !nav->osk.open) {
    nk_hash           next = nk_nav_cycle(stops, count, scope, cycle);
    struct nk_window *win  = nk_nav_find_window(ctx, next);

    if (next && next != scope) {
      if (focus >= 0) {
        nk_nav_remember(nav, stops[focus].window, stops[focus].id);
      }

      if (win) {
        if (ctx->end != win) {
          nk_remove_window(ctx, win);
          nk_insert_window(ctx, win, NK_INSERT_BACK);
        }

        ctx->active = win;
      }

      scope = next;
      focus = nk_nav_index(stops, count, nk_nav_recall(nav, next));

      if (focus < 0) {
        focus = nk_nav_first(stops, count, next);
      }

      key = 0;
    }
  }

  if (focus < 0 || stops[focus].window != scope) {
    int back = nk_nav_index(stops, count, nav->return_focus);

    if (back >= 0 && stops[back].window == scope) {
      focus             = back;
      nav->return_focus = 0;
    } else {
      if (focus >= 0 && nav->focus && popup) {
        nav->return_focus = nav->focus;
      }

      focus = nk_nav_index(stops, count, nk_nav_recall(nav, scope));

      if (focus < 0 || stops[focus].window != scope) {
        focus = nk_nav_first(stops, count, scope);
      }

      key = 0;
    }
  }

  if (tab && focus >= 0) {
    int n = 0, at = 0, j;

    for (j = 0; j < count; ++j) {
      n += stops[j].window == stops[focus].window;
    }

    for (j = 0; j < count; ++j) {
      if (stops[j].window == stops[focus].window) {
        if (j == focus) {
          break;
        }

        at += 1;
      }
    }

    at = (at + tab + n) % n;

    for (j = 0; j < count; ++j) {
      if (stops[j].window == stops[focus].window && at-- == 0) {
        focus = j;
        break;
      }
    }

    key = 0;
  }

  if (focus < 0 || stops[focus].id != nav->focus) {
    nav->captured = nk_false;
  }

  if (focus >= 0 && (stops[focus].flags & (NK_NAV_STOP_ADJUST | NK_NAV_STOP_CAPTURE))) {
    click = nk_false;

    if (nk_nav_pressed(in, NK_KEY_NAV_ACTIVATE) && !editing) {
      nav->captured = !nav->captured;
    } else if (nav->captured && nav->cancel) {
      nav->captured = nk_false;
      nav->cancel   = nk_false;
    }

    if (nav->captured && key) {
      int step = nav->repeat_key && nav->repeat_time >= NK_NAV_FAST_DELAY ? 10 : 1;
      int dx   = key == NK_KEY_NAV_LEFT ? -step : key == NK_KEY_NAV_RIGHT ? step : 0;
      int dy   = key == NK_KEY_NAV_DOWN ? -step : key == NK_KEY_NAV_UP ? step : 0;

      if (stops[focus].flags & NK_NAV_STOP_CAPTURE) {
        nav->adjust   = dx;
        nav->adjust_y = dy;
      } else {
        nav->adjust = dx ? dx : dy; /* up is more */
      }

      key = 0;
    }
  }

  if (focus >= 0 && (stops[focus].flags & (NK_NAV_STOP_NO_CLICK | NK_NAV_STOP_PANEL))) {
    click = nk_false;
  }

  if (key && focus >= 0 && (stops[focus].flags & NK_NAV_STOP_HIDDEN)) {
    /* the focus was scrolled away: start again from what is visible */
    int j, pass, pick = -1;

    for (pass = 0; pass < 2 && pick < 0; ++pass) {
      for (j = 0; j < count; ++j) {
        const struct nk_rect b = stops[j].bounds, c = stops[j].clip;
        nk_bool whole          = b.y >= c.y && b.y + b.h <= c.y + c.h;

        if (stops[j].window == stops[focus].window && !(stops[j].flags & NK_NAV_STOP_HIDDEN) && (pass || whole) &&
            (pick < 0 || key == NK_KEY_NAV_UP || key == NK_KEY_NAV_LEFT)) {
          pick = j;
        }
      }
    }

    if (pick >= 0) {
      focus = pick;
      key   = 0;
    }
  }

  if (key && focus >= 0) {
    int next = nk_nav_find(stops, count, focus, key);

    if (next >= 0) {
      focus = next;
    }
  }

  if (focus < 0) {
    return;
  }

  if (stops[focus].id != nav->focus) {
    nav->focus        = stops[focus].id;
    nav->focus_bounds = stops[focus].bounds;

    {
      struct nk_vec2 moved = nk_nav_scroll_into_view(&stops[focus]);

      nav->focus_bounds.x -= moved.x;
      nav->focus_bounds.y -= moved.y;
    }
  }

  nav->focus_flags = stops[focus].flags;

  {
    struct nk_window *w = nk_nav_find_window(ctx, stops[focus].window);

    if (w && !(w->flags & NK_WINDOW_BACKGROUND)) {
      if (ctx->end != w) {
        nk_remove_window(ctx, w);
        nk_insert_window(ctx, w, NK_INSERT_BACK);
      }

      /* nk_begin clears NK_WINDOW_ROM only on a click */
      w->flags   &= ~(nk_flags)NK_WINDOW_ROM;
      ctx->active = w;
    }
  }

  if (scroll) {
    nk_uint *x     = stops[focus].scroll_x, *y = stops[focus].scroll_y;
    nk_uint *max_x = stops[focus].scroll_max_x, *max_y = stops[focus].scroll_max_y;
    float    step  = NK_NAV_SCROLL_SPEED * (ctx->delta_time_seconds > 0 ? ctx->delta_time_seconds : 1.0f / 60.0f);
    nk_uint  by    = (nk_uint)NK_MAX(1.0f, step);

    for (i = 0; !y && i < count; ++i) {
      if (stops[i].window == stops[focus].window && stops[i].scroll_y) {
        x     = stops[i].scroll_x;
        y     = stops[i].scroll_y;
        max_x = stops[i].scroll_max_x;
        max_y = stops[i].scroll_max_y;
      }
    }

    /* or the panel jitters */
    if (y && scroll == NK_KEY_NAV_SCROLL_DOWN) {
      *y = NK_MIN(*y + by, max_y ? NK_MAX(*max_y, *y) : *y + by);
    } else if (y && scroll == NK_KEY_NAV_SCROLL_UP) {
      *y = *y > by ? *y - by : 0;
    } else if (x && scroll == NK_KEY_NAV_SCROLL_RIGHT) {
      *x = NK_MIN(*x + by, max_x ? NK_MAX(*max_x, *x) : *x + by);
    } else if (x && scroll == NK_KEY_NAV_SCROLL_LEFT) {
      *x = *x > by ? *x - by : 0;
    }

    ctx->animating = nk_true;
  }

  in->mouse.pos.x   = nav->focus_bounds.x + nav->focus_bounds.w * 0.5f;
  in->mouse.pos.y   = nav->focus_bounds.y + nav->focus_bounds.h * 0.5f;
  in->mouse.prev    = in->mouse.pos;
  in->mouse.delta.x = 0;
  in->mouse.delta.y = 0;

  if (nav->click_focus) {
    nav->click_focus = nk_false;
    nk_nav_button(ctx, &nav->activate, nk_true, NK_BUTTON_LEFT);
  } else {
    nk_nav_button(ctx, &nav->activate, nav->activate ? in->keyboard.keys[NK_KEY_NAV_ACTIVATE].down : nk_nav_pressed(in, NK_KEY_NAV_ACTIVATE) && (!editing || nav->osk.open) && click, NK_BUTTON_LEFT);
  }

  nk_nav_button(ctx, &nav->menu, nav->menu ? in->keyboard.keys[NK_KEY_NAV_MENU].down : nk_nav_pressed(in, NK_KEY_NAV_MENU) && !editing && !nav->osk.open, NK_BUTTON_RIGHT);
}

/* ===============================================================
 *
 *                          ON-SCREEN KEYBOARD
 *
 * ===============================================================*/
static const struct nk_osk_page nk_osk_default_pages[] = {
  {"abc",
   {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"},
   {"!@#$%^&*()", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"}},
  {"#+=",
   {"1234567890", "-/:;()$&@\"", "[]{}#%^*+=", "_\\|~<>.,?!'`"},
   {0, 0, 0, 0}},
};

NK_API nk_bool
nk_nav_text_requested(const struct nk_context *ctx, struct nk_rect *field)
{
  if (!ctx || !ctx->nav.text_request) {
    return nk_false;
  }

  if (field) {
    *field = ctx->nav.text_bounds;
  }
  return nk_true;
}

NK_API nk_bool
nk_osk_open(struct nk_context *ctx)
{
  struct nk_nav       *nav;
  struct nk_osk_state *osk;

  if (!ctx) {
    return nk_false;
  }

  nav = &ctx->nav;

  if (nav->text_request) {
  } else if (nav->editing_last && ctx->edit_revert.window) {
    nav->text_window = ctx->edit_revert.window;
    nav->text_edit   = ctx->edit_revert.edit;
  } else if (nav->enabled && nav->focus && (nav->focus_flags & NK_NAV_STOP_TEXT)) {
    nav->active          = nk_true;
    nav->click_focus     = nk_true;
    nav->osk_on_activate = nk_true;
    return nk_true;
  } else {
    /* the first text field of the focused window */
    const struct nk_nav_stop *stops  = nav->stops[nav->current];
    int                       count  = nav->count[nav->current];
    nk_hash                   window = nav->focus ? nav->focus_window : ctx->active ? ctx->active->name : 0;
    int                       i;

    for (i = 0; nav->enabled && window && i < count; ++i) {
      if (stops[i].window == window && (stops[i].flags & NK_NAV_STOP_TEXT) && !(stops[i].flags & NK_NAV_STOP_HIDDEN)) {
        nav->focus           = stops[i].id;
        nav->focus_bounds    = stops[i].bounds;
        nav->focus_flags     = stops[i].flags;
        nav->active          = nk_true;
        nav->click_focus     = nk_true;
        nav->osk_on_activate = nk_true;
        return nk_true;
      }
    }
    return nk_false;
  }

  osk                 = &nav->osk;
  osk->open           = nk_true;
  osk->closing        = nk_false;
  osk->seen           = nk_false;
  osk->frames         = 0;
  osk->window         = nav->text_window;
  osk->edit           = nav->text_edit;
  osk->return_focus   = nav->focus;
  osk->page           = 0;
  osk->shift          = 0;
  osk->field          = nav->text_bounds;
  osk->preview_len    = 0;
  osk->preview_cursor = 0;
  osk->shift_hold     = 0;
  osk->shift_held     = nk_false;
  osk->text_len       = 0;
  osk->key_count      = 0;
  nav->focus          = 0;
  return nk_true;
}

NK_API void
nk_osk_close(struct nk_context *ctx)
{
  struct nk_nav       *nav;
  struct nk_osk_state *osk;
  struct nk_window    *win;

  if (!ctx || !ctx->nav.osk.open) {
    return;
  }

  nav       = &ctx->nav;
  osk       = &nav->osk;
  osk->open = nk_false;

  win = nk_nav_find_window(ctx, osk->window);

  if (win) {
    if (win->edit.active && win->edit.name == osk->edit) {
      win->edit.active = nk_false;
    }

    if (ctx->end != win) {
      nk_remove_window(ctx, win);
      nk_insert_window(ctx, win, NK_INSERT_BACK);
    }

    ctx->active = win;
  }

  nav->focus     = osk->return_focus;
  osk->text_len  = 0;
  osk->key_count = 0;
}

NK_API nk_bool
nk_osk_is_open(const struct nk_context *ctx)
{
  return ctx && ctx->nav.osk.open;
}

NK_API void
nk_osk_layout(struct nk_context *ctx, const struct nk_osk_page *pages, int count)
{
  NK_ASSERT(ctx);

  if (ctx) {
    ctx->nav.osk.pages      = count > 0 ? pages : 0;
    ctx->nav.osk.page_count = count > 0 ? count : 0;
    ctx->nav.osk.page       = 0;
  }
}

NK_INTERN void
nk_osk_key(struct nk_osk_state *osk, enum nk_keys key)
{
  if (osk->key_count < (int)NK_LEN(osk->keys)) {
    osk->keys[osk->key_count++] = (int)key;
  }
}

NK_INTERN void
nk_osk_type(struct nk_osk_state *osk, nk_rune rune)
{
  if (osk->text_len < (int)NK_LEN(osk->text)) {
    osk->text[osk->text_len++] = rune;
  }
}

/* pressed, then repeated while held */
NK_INTERN nk_bool
nk_osk_held(struct nk_context *ctx, enum nk_keys key)
{
  struct nk_osk_state   *osk = &ctx->nav.osk;
  const struct nk_input *in  = &ctx->input;
  float                  before;

  if (nk_nav_pressed(in, key)) {
    osk->repeat_key  = (int)key;
    osk->repeat_time = 0;
    return nk_true;
  }

  if (osk->repeat_key != (int)key) {
    return nk_false;
  }

  if (!in->keyboard.keys[key].down) {
    osk->repeat_key = 0;
    return nk_false;
  }

  before            = osk->repeat_time;
  osk->repeat_time += ctx->delta_time_seconds;
  ctx->animating    = nk_true;

  return osk->repeat_time >= NK_NAV_REPEAT_DELAY &&
         (before < NK_NAV_REPEAT_DELAY ||
          (int)((osk->repeat_time - NK_NAV_REPEAT_DELAY) / NK_NAV_REPEAT_RATE) != (int)((before - NK_NAV_REPEAT_DELAY) / NK_NAV_REPEAT_RATE));
}

NK_INTERN void
nk_osk_input(struct nk_context *ctx)
{
  struct nk_osk_state *osk = &ctx->nav.osk;
  int                  i;

  for (i = 0; i < osk->release_count; ++i) {
    nk_input_key(ctx, (enum nk_keys)osk->release[i], nk_false);
  }

  osk->release_count = 0;

  for (i = 0; i < osk->text_len; ++i) {
    nk_input_unicode(ctx, osk->text[i]);
  }

  osk->text_len = 0;

  for (i = 0; i < osk->key_count; ++i) {
    nk_input_key(ctx, (enum nk_keys)osk->keys[i], nk_true);
    osk->release[osk->release_count++] = osk->keys[i];
  }

  osk->key_count = 0;
}

NK_API struct nk_rect
nk_osk_place(const struct nk_context *ctx, struct nk_rect screen, float width, float height)
{
  float          margin = 24.0f;
  float          w      = NK_MIN(width, screen.w);
  float          h      = NK_MIN(height, screen.h);
  float          x      = screen.x + (screen.w - w) * 0.5f;
  struct nk_rect bottom = nk_rect(x, screen.y + screen.h - h - margin, w, h);
  struct nk_rect top    = nk_rect(x, screen.y + margin, w, h);
  struct nk_rect f      = ctx ? ctx->nav.osk.field : nk_rect(0, 0, 0, 0);

  if (ctx && ctx->nav.osk.open && f.w > 0 && NK_INTERSECT(bottom.x, bottom.y, bottom.w, bottom.h, f.x, f.y, f.w, f.h) &&
      !NK_INTERSECT(top.x, top.y, top.w, top.h, f.x, f.y, f.w, f.h)) {
    return top;
  }
  return bottom;
}

NK_API nk_bool
nk_osk(struct nk_context *ctx, struct nk_rect bounds)
{
  static const float        bottom[] = {0.14f, 0.12f, 0.38f, 0.12f, 0.12f, 0.12f};
  struct nk_nav            *nav;
  struct nk_osk_state      *osk;
  struct nk_input          *in;
  const struct nk_osk_page *pages;
  const struct nk_osk_page *page;
  int                       count;
  int                       r;

  NK_ASSERT(ctx);

  if (!ctx || !ctx->nav.osk.open) {
    return nk_false;
  }

  nav = &ctx->nav;
  osk = &nav->osk;
  in  = &ctx->input;

  if ((osk->frames++ > 0 && !osk->seen) || osk->closing || nav->cancel) {
    nav->cancel = nk_false;
    nk_osk_close(ctx);
    return nk_false;
  }

  osk->seen = nk_false;

  /* pad shortcuts */
  if (nk_osk_held(ctx, NK_KEY_NAV_MENU)) {
    nk_osk_key(osk, NK_KEY_BACKSPACE);
  }

  if (nk_osk_held(ctx, NK_KEY_NAV_WINDOW_PREV)) {
    nk_osk_key(osk, NK_KEY_LEFT);
  }

  if (nk_osk_held(ctx, NK_KEY_NAV_WINDOW_NEXT)) {
    nk_osk_key(osk, NK_KEY_RIGHT);
  }

  if (nk_osk_held(ctx, NK_KEY_NAV_WORD_PREV)) {
    nk_osk_key(osk, NK_KEY_TEXT_WORD_LEFT);
  }

  if (nk_osk_held(ctx, NK_KEY_NAV_WORD_NEXT)) {
    nk_osk_key(osk, NK_KEY_TEXT_WORD_RIGHT);
  }

  pages = osk->pages ? osk->pages : nk_osk_default_pages;
  count = osk->pages ? osk->page_count : (int)NK_LEN(nk_osk_default_pages);
  page  = &pages[osk->page % count];

  if (osk->frames == 1) {
    nk_window_set_focus(ctx, "nk_osk");
  }

  if (nk_begin(ctx, "nk_osk", bounds, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR)) {
    const struct nk_style *style = &ctx->style;
    float                  row_h = (bounds.h - 2.0f * style->window.padding.y - (float)(NK_OSK_ROWS + 1) * style->window.spacing.y) / (float)(NK_OSK_ROWS + 2);

    osk->name = ctx->current->name;
    row_h     = NK_MAX(row_h, style->font_size + 4.0f);

    {
      struct nk_rect r;

      nk_layout_row_dynamic(ctx, row_h, 1);

      if (nk_widget(&r, ctx)) {
        struct nk_command_buffer *out  = nk_window_get_canvas(ctx);
        const struct nk_style_item *bg = &style->edit.active;
        float  before                  = nk_text_width(ctx, style->font, style->font_size, osk->preview, osk->preview_cursor);
        float  x                       = r.x + style->edit.padding.x;
        float  room                    = r.w - 2.0f * style->edit.padding.x;
        float  shift                   = before > room ? before - room : 0.0f;
        struct nk_rect old_clip        = out->clip, clip;

        nk_fill_rect(out, r, style->edit.rounding, bg->type == NK_STYLE_ITEM_COLOR ? bg->data.color : style->window.background);
        nk_unify(&clip, &old_clip, r.x, r.y, r.x + r.w, r.y + r.h);
        nk_push_scissor(out, clip);
        nk_draw_text(out, nk_rect(x - shift, r.y + (r.h - style->font_size) * 0.5f, room + shift, style->font_size), osk->preview, osk->preview_len, style->font,
                     nk_rgba(0, 0, 0, 0), style->edit.text_active);
        nk_fill_rect(out, nk_rect((float)(int)(x - shift + before), r.y + (r.h - style->font_size) * 0.5f, 1.0f, style->font_size), 0,
                     style->edit.cursor_normal);
        nk_push_scissor(out, old_clip);
      }
    }

    for (r = 0; r < NK_OSK_ROWS; ++r) {
      const char *row = (osk->shift && page->shifted[r]) ? page->shifted[r] : page->rows[r];
      int         len, n, at = 0;

      if (!row) {
        continue;
      }

      len = nk_strlen(row);
      n   = nk_utf_len(row, len);
      nk_layout_row_dynamic(ctx, row_h, NK_MAX(n, 1));

      while (at < len) {
        nk_rune rune;
        int     glyph = nk_utf_decode(row + at, &rune, len - at);

        if (!glyph) {
          break;
        }

        if (nk_button_text(ctx, row + at, glyph)) {
          nk_osk_type(osk, rune);

          if (osk->shift == 1) {
            osk->shift = 0;
          }
        }

        at += glyph;
      }
    }

    nk_layout_row(ctx, NK_DYNAMIC, row_h, (int)NK_LEN(bottom), bottom);

    {
      /* Shift: a tap for one letter, a double tap or a hold locks */
      struct nk_rect shift_bounds = nk_widget_bounds(ctx);

      if (nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, shift_bounds, nk_true)) {
        osk->shift_hold += ctx->delta_time_seconds;
        ctx->animating   = nk_true;

        if (osk->shift_hold >= 0.5f && !osk->shift_held) {
          osk->shift      = 2;
          osk->shift_held = nk_true;
        }
      } else {
        osk->shift_hold = 0;
      }

      if (nk_button_label(ctx, osk->shift == 2 ? "CAPS" : osk->shift ? "SHIFT" : "Shift")) {
        if (osk->shift_held) {
          osk->shift_held = nk_false;
        } else {
          osk->shift = (osk->shift + 1) % 3;
        }
      }
    }

    if (nk_button_label(ctx, pages[(osk->page + 1) % count].label ? pages[(osk->page + 1) % count].label : "...")) {
      osk->page  = (osk->page + 1) % count;
      osk->shift = 0;
    }

    if (nk_button_label(ctx, "Space")) {
      nk_osk_type(osk, ' ');
    }

    if (nk_button_label(ctx, "Bksp")) {
      nk_osk_key(osk, NK_KEY_BACKSPACE);
    }

    if (nk_button_label(ctx, "Enter")) {
      nk_osk_key(osk, NK_KEY_ENTER);
    }

    if (nk_button_label(ctx, "Done")) {
      osk->closing = nk_true;
    }
  }

  nk_end(ctx);
  return osk->open;
}

/* ===============================================================
 *
 *                              STYLE
 *
 * ===============================================================*/
NK_API void
nk_style_default(struct nk_context *ctx)
{
  nk_style_from_table(ctx, 0);
}

/* dark teal */
#define NK_COLOR_MAP(NK_COLOR)                                   \
  NK_COLOR(NK_COLOR_TEXT, 222, 226, 230, 255)                    \
  NK_COLOR(NK_COLOR_WINDOW, 22, 25, 29, 255)                     \
  NK_COLOR(NK_COLOR_HEADER, 32, 36, 42, 255)                     \
  NK_COLOR(NK_COLOR_BORDER, 50, 56, 64, 255)                     \
  NK_COLOR(NK_COLOR_BUTTON, 40, 46, 54, 255)                     \
  NK_COLOR(NK_COLOR_BUTTON_HOVER, 52, 61, 72, 255)               \
  NK_COLOR(NK_COLOR_BUTTON_ACTIVE, 20, 128, 124, 255)            \
  NK_COLOR(NK_COLOR_TOGGLE, 44, 50, 58, 255)                     \
  NK_COLOR(NK_COLOR_TOGGLE_HOVER, 58, 66, 76, 255)               \
  NK_COLOR(NK_COLOR_TOGGLE_CURSOR, 38, 198, 190, 255)            \
  NK_COLOR(NK_COLOR_SELECT, 34, 39, 46, 255)                     \
  NK_COLOR(NK_COLOR_SELECT_ACTIVE, 22, 112, 108, 255)            \
  NK_COLOR(NK_COLOR_SLIDER, 44, 50, 58, 255)                     \
  NK_COLOR(NK_COLOR_SLIDER_CURSOR, 38, 198, 190, 255)            \
  NK_COLOR(NK_COLOR_SLIDER_CURSOR_HOVER, 82, 218, 210, 255)      \
  NK_COLOR(NK_COLOR_SLIDER_CURSOR_ACTIVE, 140, 236, 230, 255)    \
  NK_COLOR(NK_COLOR_PROPERTY, 34, 39, 46, 255)                   \
  NK_COLOR(NK_COLOR_EDIT, 15, 17, 20, 255)                       \
  NK_COLOR(NK_COLOR_EDIT_CURSOR, 222, 226, 230, 255)             \
  NK_COLOR(NK_COLOR_COMBO, 34, 39, 46, 255)                      \
  NK_COLOR(NK_COLOR_CHART, 34, 39, 46, 255)                      \
  NK_COLOR(NK_COLOR_CHART_COLOR, 38, 198, 190, 255)              \
  NK_COLOR(NK_COLOR_CHART_COLOR_HIGHLIGHT, 255, 176, 64, 255)    \
  NK_COLOR(NK_COLOR_SCROLLBAR, 26, 29, 34, 255)                  \
  NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR, 58, 66, 76, 255)           \
  NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_HOVER, 76, 86, 98, 255)     \
  NK_COLOR(NK_COLOR_SCROLLBAR_CURSOR_ACTIVE, 38, 198, 190, 255)  \
  NK_COLOR(NK_COLOR_TAB_HEADER, 32, 36, 42, 255)                 \
  NK_COLOR(NK_COLOR_KNOB, 44, 50, 58, 255)                       \
  NK_COLOR(NK_COLOR_KNOB_CURSOR, 38, 198, 190, 255)              \
  NK_COLOR(NK_COLOR_KNOB_CURSOR_HOVER, 82, 218, 210, 255)        \
  NK_COLOR(NK_COLOR_KNOB_CURSOR_ACTIVE, 140, 236, 230, 255)

const struct nk_color nk_default_color_style[NK_COLOR_COUNT] = {
#define NK_COLOR(a, b, c, d, e) {b, c, d, e},
  NK_COLOR_MAP(NK_COLOR)
#undef NK_COLOR
};

static const struct nk_color nk_theme_dark_blue[NK_COLOR_COUNT] = {
  {230, 232, 236, 255}, /* TEXT */
  {20, 22, 28, 255},    /* WINDOW */
  {30, 34, 44, 255},    /* HEADER */
  {48, 54, 70, 255},    /* BORDER */
  {38, 52, 78, 255},    /* BUTTON */
  {48, 72, 112, 255},   /* BUTTON_HOVER */
  {36, 104, 196, 255},  /* BUTTON_ACTIVE */
  {40, 48, 66, 255},    /* TOGGLE */
  {52, 62, 86, 255},    /* TOGGLE_HOVER */
  {66, 150, 250, 255},  /* TOGGLE_CURSOR */
  {32, 38, 52, 255},    /* SELECT */
  {36, 90, 170, 255},   /* SELECT_ACTIVE */
  {40, 48, 66, 255},    /* SLIDER */
  {66, 150, 250, 255},  /* SLIDER_CURSOR */
  {100, 170, 255, 255}, /* SLIDER_CURSOR_HOVER */
  {150, 200, 255, 255}, /* SLIDER_CURSOR_ACTIVE */
  {32, 38, 52, 255},    /* PROPERTY */
  {14, 16, 22, 255},    /* EDIT */
  {230, 232, 236, 255}, /* EDIT_CURSOR */
  {32, 38, 52, 255},    /* COMBO */
  {32, 38, 52, 255},    /* CHART */
  {66, 150, 250, 255},  /* CHART_COLOR */
  {255, 160, 60, 255},  /* CHART_COLOR_HIGHLIGHT */
  {24, 27, 34, 255},    /* SCROLLBAR */
  {56, 64, 84, 255},    /* SCROLLBAR_CURSOR */
  {72, 82, 106, 255},   /* SCROLLBAR_CURSOR_HOVER */
  {66, 150, 250, 255},  /* SCROLLBAR_CURSOR_ACTIVE */
  {30, 34, 44, 255},    /* TAB_HEADER */
  {40, 48, 66, 255},    /* KNOB */
  {66, 150, 250, 255},  /* KNOB_CURSOR */
  {100, 170, 255, 255}, /* KNOB_CURSOR_HOVER */
  {150, 200, 255, 255}, /* KNOB_CURSOR_ACTIVE */
};

static const struct nk_color nk_theme_light[NK_COLOR_COUNT] = {
  {30, 34, 40, 255},    /* TEXT */
  {238, 240, 243, 255}, /* WINDOW */
  {222, 226, 232, 255}, /* HEADER */
  {196, 202, 210, 255}, /* BORDER */
  {226, 230, 236, 255}, /* BUTTON */
  {212, 218, 226, 255}, /* BUTTON_HOVER */
  {160, 190, 235, 255}, /* BUTTON_ACTIVE */
  {214, 219, 226, 255}, /* TOGGLE */
  {200, 206, 215, 255}, /* TOGGLE_HOVER */
  {40, 110, 210, 255},  /* TOGGLE_CURSOR */
  {226, 230, 236, 255}, /* SELECT */
  {180, 205, 240, 255}, /* SELECT_ACTIVE */
  {210, 215, 222, 255}, /* SLIDER */
  {40, 110, 210, 255},  /* SLIDER_CURSOR */
  {60, 130, 230, 255},  /* SLIDER_CURSOR_HOVER */
  {20, 90, 190, 255},   /* SLIDER_CURSOR_ACTIVE */
  {226, 230, 236, 255}, /* PROPERTY */
  {255, 255, 255, 255}, /* EDIT */
  {30, 34, 40, 255},    /* EDIT_CURSOR */
  {226, 230, 236, 255}, /* COMBO */
  {226, 230, 236, 255}, /* CHART */
  {40, 110, 210, 255},  /* CHART_COLOR */
  {220, 120, 10, 255},  /* CHART_COLOR_HIGHLIGHT */
  {228, 231, 235, 255}, /* SCROLLBAR */
  {190, 196, 205, 255}, /* SCROLLBAR_CURSOR */
  {170, 177, 188, 255}, /* SCROLLBAR_CURSOR_HOVER */
  {40, 110, 210, 255},  /* SCROLLBAR_CURSOR_ACTIVE */
  {222, 226, 232, 255}, /* TAB_HEADER */
  {210, 215, 222, 255}, /* KNOB */
  {40, 110, 210, 255},  /* KNOB_CURSOR */
  {60, 130, 230, 255},  /* KNOB_CURSOR_HOVER */
  {20, 90, 190, 255},   /* KNOB_CURSOR_ACTIVE */
};

NK_API const struct nk_color *
nk_style_theme_table(enum nk_style_theme theme)
{
  switch (theme) {
    case NK_THEME_DARK_BLUE:
      return nk_theme_dark_blue;

    case NK_THEME_LIGHT:
      return nk_theme_light;

    default:
      return nk_default_color_style;
  }
}

NK_API void
nk_style_from_theme(struct nk_context *ctx, enum nk_style_theme theme)
{
  nk_style_from_table(ctx, nk_style_theme_table(theme));
}

NK_INTERN struct nk_color
nk_style_blend(struct nk_color a, struct nk_color b, float t)
{
  return nk_rgba((int)((float)a.r + ((float)b.r - (float)a.r) * t), (int)((float)a.g + ((float)b.g - (float)a.g) * t),
                 (int)((float)a.b + ((float)b.b - (float)a.b) * t), (int)((float)a.a + ((float)b.a - (float)a.a) * t));
}

const char *nk_color_names[NK_COLOR_COUNT] = {
#define NK_COLOR(a, b, c, d, e) #a,
  NK_COLOR_MAP(NK_COLOR)
#undef NK_COLOR
};

NK_API const char *
nk_style_get_color_by_name(enum nk_style_colors c)
{
  return nk_color_names[c];
}

NK_API struct nk_style_item
nk_style_item_color(struct nk_color col)
{
  struct nk_style_item i;

  i.type       = NK_STYLE_ITEM_COLOR;
  i.data.color = col;
  return i;
}

NK_API struct nk_style_item
nk_style_item_image(struct nk_image img)
{
  struct nk_style_item i;

  i.type       = NK_STYLE_ITEM_IMAGE;
  i.data.image = img;
  return i;
}

NK_API struct nk_style_item
nk_style_item_nine_slice(struct nk_nine_slice slice)
{
  struct nk_style_item i;

  i.type       = NK_STYLE_ITEM_NINE_SLICE;
  i.data.slice = slice;
  return i;
}

NK_API struct nk_style_item
nk_style_item_hide(void)
{
  struct nk_style_item i;

  i.type       = NK_STYLE_ITEM_COLOR;
  i.data.color = nk_rgba(0, 0, 0, 0);
  return i;
}

NK_API void
nk_style_from_table(struct nk_context *ctx, const struct nk_color *table)
{
  struct nk_style            *style;
  struct nk_style_text       *text;
  struct nk_style_button     *button;
  struct nk_style_toggle     *toggle;
  struct nk_style_selectable *select;
  struct nk_style_slider     *slider;
  struct nk_style_knob       *knob;
  struct nk_style_progress   *prog;
  struct nk_style_scrollbar  *scroll;
  struct nk_style_edit       *edit;
  struct nk_style_property   *property;
  struct nk_style_combo      *combo;
  struct nk_style_chart      *chart;
  struct nk_style_tab        *tab;
  struct nk_style_window     *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  style                = &ctx->style;
  table                = (!table) ? nk_default_color_style : table;
  style->nav_highlight = table[NK_COLOR_TOGGLE_CURSOR];
  style->nav_active    = table[NK_COLOR_CHART_COLOR_HIGHLIGHT];

  /* default text */
  text                  = &style->text;
  text->color           = table[NK_COLOR_TEXT];
  text->padding         = nk_vec2(0, 0);
  text->color_factor    = 1.0f;
  text->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* default button */
  button = &style->button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_BUTTON]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_BUTTON_HOVER]);
  button->active                  = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
  button->border_color            = table[NK_COLOR_BORDER];
  button->text_background         = table[NK_COLOR_BUTTON];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->image_padding           = nk_vec2(0.0f, 0.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 1.0f;
  button->rounding                = 4.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* contextual button */
  button = &style->contextual_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_BUTTON_HOVER]);
  button->active                  = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
  button->border_color            = table[NK_COLOR_WINDOW];
  button->text_background         = table[NK_COLOR_WINDOW];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* menu button */
  button = &style->menu_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->active                  = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->border_color            = table[NK_COLOR_WINDOW];
  button->text_background         = table[NK_COLOR_WINDOW];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 1.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* checkbox toggle */
  toggle = &style->checkbox;
  nk_zero_struct(*toggle);
  toggle->normal          = nk_style_item_color(table[NK_COLOR_TOGGLE]);
  toggle->hover           = nk_style_item_color(table[NK_COLOR_TOGGLE_HOVER]);
  toggle->active          = nk_style_item_color(table[NK_COLOR_TOGGLE_HOVER]);
  toggle->cursor_normal   = nk_style_item_color(table[NK_COLOR_TOGGLE_CURSOR]);
  toggle->cursor_hover    = nk_style_item_color(table[NK_COLOR_TOGGLE_CURSOR]);
  toggle->userdata        = nk_handle_ptr(0);
  toggle->text_background = table[NK_COLOR_WINDOW];
  toggle->text_normal     = table[NK_COLOR_TEXT];
  toggle->text_hover      = table[NK_COLOR_TEXT];
  toggle->text_active     = table[NK_COLOR_TEXT];
  toggle->padding         = nk_vec2(2.0f, 2.0f);
  toggle->touch_padding   = nk_vec2(0, 0);
  toggle->border_color    = nk_rgba(0, 0, 0, 0);
  toggle->border          = 0.0f;
  toggle->spacing         = 4;
  toggle->color_factor    = 1.0f;
  toggle->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* option toggle */
  toggle = &style->option;
  nk_zero_struct(*toggle);
  toggle->normal          = nk_style_item_color(table[NK_COLOR_TOGGLE]);
  toggle->hover           = nk_style_item_color(table[NK_COLOR_TOGGLE_HOVER]);
  toggle->active          = nk_style_item_color(table[NK_COLOR_TOGGLE_HOVER]);
  toggle->cursor_normal   = nk_style_item_color(table[NK_COLOR_TOGGLE_CURSOR]);
  toggle->cursor_hover    = nk_style_item_color(table[NK_COLOR_TOGGLE_CURSOR]);
  toggle->userdata        = nk_handle_ptr(0);
  toggle->text_background = table[NK_COLOR_WINDOW];
  toggle->text_normal     = table[NK_COLOR_TEXT];
  toggle->text_hover      = table[NK_COLOR_TEXT];
  toggle->text_active     = table[NK_COLOR_TEXT];
  toggle->padding         = nk_vec2(3.0f, 3.0f);
  toggle->touch_padding   = nk_vec2(0, 0);
  toggle->border_color    = nk_rgba(0, 0, 0, 0);
  toggle->border          = 0.0f;
  toggle->spacing         = 4;
  toggle->color_factor    = 1.0f;
  toggle->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* selectable */
  select = &style->selectable;
  nk_zero_struct(*select);
  select->normal              = nk_style_item_color(table[NK_COLOR_SELECT]);
  select->hover               = nk_style_item_color(table[NK_COLOR_SELECT]);
  select->pressed             = nk_style_item_color(table[NK_COLOR_SELECT]);
  select->normal_active       = nk_style_item_color(table[NK_COLOR_SELECT_ACTIVE]);
  select->hover_active        = nk_style_item_color(table[NK_COLOR_SELECT_ACTIVE]);
  select->pressed_active      = nk_style_item_color(table[NK_COLOR_SELECT_ACTIVE]);
  select->text_normal         = table[NK_COLOR_TEXT];
  select->text_hover          = table[NK_COLOR_TEXT];
  select->text_pressed        = table[NK_COLOR_TEXT];
  select->text_normal_active  = table[NK_COLOR_TEXT];
  select->text_hover_active   = table[NK_COLOR_TEXT];
  select->text_pressed_active = table[NK_COLOR_TEXT];
  select->padding             = nk_vec2(2.0f, 2.0f);
  select->image_padding       = nk_vec2(2.0f, 2.0f);
  select->touch_padding       = nk_vec2(0, 0);
  select->userdata            = nk_handle_ptr(0);
  select->rounding            = 0.0f;
  select->color_factor        = 1.0f;
  select->disabled_factor     = NK_WIDGET_DISABLED_FACTOR;
  select->draw_begin          = 0;
  select->draw_end            = 0;

  /* slider */
  slider = &style->slider;
  nk_zero_struct(*slider);
  slider->normal          = nk_style_item_hide();
  slider->hover           = nk_style_item_hide();
  slider->active          = nk_style_item_hide();
  slider->bar_normal      = table[NK_COLOR_SLIDER];
  slider->bar_hover       = table[NK_COLOR_SLIDER];
  slider->bar_active      = table[NK_COLOR_SLIDER];
  slider->bar_filled      = table[NK_COLOR_SLIDER_CURSOR];
  slider->cursor_normal   = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR]);
  slider->cursor_hover    = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR_HOVER]);
  slider->cursor_active   = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR_ACTIVE]);
  slider->inc_symbol      = NK_SYMBOL_TRIANGLE_RIGHT;
  slider->dec_symbol      = NK_SYMBOL_TRIANGLE_LEFT;
  slider->cursor_size     = nk_vec2(16, 16);
  slider->padding         = nk_vec2(2, 2);
  slider->spacing         = nk_vec2(2, 2);
  slider->userdata        = nk_handle_ptr(0);
  slider->show_buttons    = nk_false;
  slider->bar_height      = 8;
  slider->rounding        = 0;
  slider->color_factor    = 1.0f;
  slider->disabled_factor = NK_WIDGET_DISABLED_FACTOR;
  slider->draw_begin      = 0;
  slider->draw_end        = 0;

  /* slider buttons */
  button                          = &style->slider.inc_button;
  button->normal                  = nk_style_item_color(nk_rgb(40, 40, 40));
  button->hover                   = nk_style_item_color(nk_rgb(42, 42, 42));
  button->active                  = nk_style_item_color(nk_rgb(44, 44, 44));
  button->border_color            = nk_rgb(65, 65, 65);
  button->text_background         = nk_rgb(40, 40, 40);
  button->text_normal             = nk_rgb(175, 175, 175);
  button->text_hover              = nk_rgb(175, 175, 175);
  button->text_active             = nk_rgb(175, 175, 175);
  button->padding                 = nk_vec2(8.0f, 8.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 1.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;
  style->slider.dec_button        = style->slider.inc_button;

  /* knob */
  knob = &style->knob;
  nk_zero_struct(*knob);
  knob->normal        = nk_style_item_hide();
  knob->hover         = nk_style_item_hide();
  knob->active        = nk_style_item_hide();
  knob->knob_normal   = table[NK_COLOR_KNOB];
  knob->knob_hover    = table[NK_COLOR_KNOB];
  knob->knob_active   = table[NK_COLOR_KNOB];
  knob->cursor_normal = table[NK_COLOR_KNOB_CURSOR];
  knob->cursor_hover  = table[NK_COLOR_KNOB_CURSOR_HOVER];
  knob->cursor_active = table[NK_COLOR_KNOB_CURSOR_ACTIVE];

  knob->knob_border_color = table[NK_COLOR_BORDER];
  knob->knob_border       = 1.0f;

  knob->padding         = nk_vec2(2, 2);
  knob->spacing         = nk_vec2(2, 2);
  knob->cursor_width    = 2;
  knob->color_factor    = 1.0f;
  knob->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  knob->userdata   = nk_handle_ptr(0);
  knob->draw_begin = 0;
  knob->draw_end   = 0;

  /* progressbar */
  prog = &style->progress;
  nk_zero_struct(*prog);
  prog->normal              = nk_style_item_color(table[NK_COLOR_SLIDER]);
  prog->hover               = nk_style_item_color(table[NK_COLOR_SLIDER]);
  prog->active              = nk_style_item_color(table[NK_COLOR_SLIDER]);
  prog->cursor_normal       = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR]);
  prog->cursor_hover        = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR_HOVER]);
  prog->cursor_active       = nk_style_item_color(table[NK_COLOR_SLIDER_CURSOR_ACTIVE]);
  prog->border_color        = nk_rgba(0, 0, 0, 0);
  prog->cursor_border_color = nk_rgba(0, 0, 0, 0);
  prog->userdata            = nk_handle_ptr(0);
  prog->padding             = nk_vec2(4, 4);
  prog->rounding            = 0;
  prog->border              = 0;
  prog->cursor_rounding     = 0;
  prog->cursor_border       = 0;
  prog->color_factor        = 1.0f;
  prog->disabled_factor     = NK_WIDGET_DISABLED_FACTOR;
  prog->draw_begin          = 0;
  prog->draw_end            = 0;

  /* scrollbars */
  scroll = &style->scrollh;
  nk_zero_struct(*scroll);
  scroll->normal              = nk_style_item_color(table[NK_COLOR_SCROLLBAR]);
  scroll->hover               = nk_style_item_color(table[NK_COLOR_SCROLLBAR]);
  scroll->active              = nk_style_item_color(table[NK_COLOR_SCROLLBAR]);
  scroll->cursor_normal       = nk_style_item_color(table[NK_COLOR_SCROLLBAR_CURSOR]);
  scroll->cursor_hover        = nk_style_item_color(table[NK_COLOR_SCROLLBAR_CURSOR_HOVER]);
  scroll->cursor_active       = nk_style_item_color(table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE]);
  scroll->dec_symbol          = NK_SYMBOL_CIRCLE_SOLID;
  scroll->inc_symbol          = NK_SYMBOL_CIRCLE_SOLID;
  scroll->userdata            = nk_handle_ptr(0);
  scroll->border_color        = table[NK_COLOR_SCROLLBAR];
  scroll->cursor_border_color = table[NK_COLOR_SCROLLBAR];
  scroll->padding             = nk_vec2(0, 0);
  scroll->show_buttons        = nk_false;
  scroll->border              = 0;
  scroll->rounding            = 0;
  scroll->border_cursor       = 0;
  scroll->rounding_cursor     = 0;
  scroll->cursor_min_size     = 10.0f;
  scroll->color_factor        = 1.0f;
  scroll->disabled_factor     = NK_WIDGET_DISABLED_FACTOR;
  scroll->draw_begin          = 0;
  scroll->draw_end            = 0;
  style->scrollv              = style->scrollh;

  /* scrollbars buttons */
  button                          = &style->scrollh.inc_button;
  button->normal                  = nk_style_item_color(nk_rgb(40, 40, 40));
  button->hover                   = nk_style_item_color(nk_rgb(42, 42, 42));
  button->active                  = nk_style_item_color(nk_rgb(44, 44, 44));
  button->border_color            = nk_rgb(65, 65, 65);
  button->text_background         = nk_rgb(40, 40, 40);
  button->text_normal             = nk_rgb(175, 175, 175);
  button->text_hover              = nk_rgb(175, 175, 175);
  button->text_active             = nk_rgb(175, 175, 175);
  button->padding                 = nk_vec2(4.0f, 4.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 1.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;
  style->scrollh.dec_button       = style->scrollh.inc_button;
  style->scrollv.inc_button       = style->scrollh.inc_button;
  style->scrollv.dec_button       = style->scrollh.inc_button;

  /* edit */
  edit = &style->edit;
  nk_zero_struct(*edit);
  /* ImGui-like */
  edit->normal               = nk_style_item_color(table[NK_COLOR_EDIT]);
  edit->hover                = nk_style_item_color(nk_style_blend(table[NK_COLOR_EDIT], table[NK_COLOR_TEXT], 0.06f));
  edit->active               = nk_style_item_color(nk_style_blend(table[NK_COLOR_EDIT], table[NK_COLOR_TEXT], 0.10f));
  edit->cursor_normal        = table[NK_COLOR_TEXT];
  edit->cursor_hover         = table[NK_COLOR_TEXT];
  edit->cursor_text_normal   = table[NK_COLOR_EDIT];
  edit->cursor_text_hover    = table[NK_COLOR_EDIT];
  edit->border_color         = table[NK_COLOR_BORDER];
  edit->text_normal          = table[NK_COLOR_TEXT];
  edit->text_hover           = table[NK_COLOR_TEXT];
  edit->text_active          = table[NK_COLOR_TEXT];
  edit->selected_normal      = nk_rgba(table[NK_COLOR_TOGGLE_CURSOR].r, table[NK_COLOR_TOGGLE_CURSOR].g, table[NK_COLOR_TOGGLE_CURSOR].b, 90);
  edit->selected_hover       = edit->selected_normal;
  edit->selected_text_normal = table[NK_COLOR_TEXT];
  edit->selected_text_hover  = table[NK_COLOR_TEXT];
  edit->scrollbar_size       = nk_vec2(10, 10);
  edit->scrollbar            = style->scrollv;
  edit->padding              = nk_vec2(4, 3);
  edit->row_padding          = 2;
  edit->cursor_size          = 1;
  edit->border               = 0;
  edit->rounding             = 0;
  edit->color_factor         = 1.0f;
  edit->disabled_factor      = NK_WIDGET_DISABLED_FACTOR;

  /* property */
  property = &style->property;
  nk_zero_struct(*property);
  property->normal          = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  property->hover           = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  property->active          = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  property->border_color    = table[NK_COLOR_BORDER];
  property->label_normal    = table[NK_COLOR_TEXT];
  property->label_hover     = table[NK_COLOR_TEXT];
  property->label_active    = table[NK_COLOR_TEXT];
  property->sym_left        = NK_SYMBOL_TRIANGLE_LEFT;
  property->sym_right       = NK_SYMBOL_TRIANGLE_RIGHT;
  property->userdata        = nk_handle_ptr(0);
  property->padding         = nk_vec2(4, 4);
  property->border          = 1;
  property->rounding        = 10;
  property->draw_begin      = 0;
  property->draw_end        = 0;
  property->color_factor    = 1.0f;
  property->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* property buttons */
  button = &style->property.dec_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  button->active                  = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = table[NK_COLOR_PROPERTY];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(0.0f, 0.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;
  style->property.inc_button      = style->property.dec_button;

  /* property edit */
  edit = &style->property.edit;
  nk_zero_struct(*edit);
  edit->normal               = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  edit->hover                = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  edit->active               = nk_style_item_color(table[NK_COLOR_PROPERTY]);
  edit->border_color         = nk_rgba(0, 0, 0, 0);
  edit->cursor_normal        = table[NK_COLOR_TEXT];
  edit->cursor_hover         = table[NK_COLOR_TEXT];
  edit->cursor_text_normal   = table[NK_COLOR_EDIT];
  edit->cursor_text_hover    = table[NK_COLOR_EDIT];
  edit->text_normal          = table[NK_COLOR_TEXT];
  edit->text_hover           = table[NK_COLOR_TEXT];
  edit->text_active          = table[NK_COLOR_TEXT];
  edit->selected_normal      = nk_rgba(table[NK_COLOR_TOGGLE_CURSOR].r, table[NK_COLOR_TOGGLE_CURSOR].g, table[NK_COLOR_TOGGLE_CURSOR].b, 90);
  edit->selected_hover       = edit->selected_normal;
  edit->selected_text_normal = table[NK_COLOR_TEXT];
  edit->selected_text_hover  = table[NK_COLOR_TEXT];
  edit->padding              = nk_vec2(0, 0);
  edit->cursor_size          = 1;
  edit->border               = 0;
  edit->rounding             = 0;
  edit->color_factor         = 1.0f;
  edit->disabled_factor      = NK_WIDGET_DISABLED_FACTOR;

  /* chart */
  chart = &style->chart;
  nk_zero_struct(*chart);
  chart->background      = nk_style_item_color(table[NK_COLOR_CHART]);
  chart->border_color    = table[NK_COLOR_BORDER];
  chart->selected_color  = table[NK_COLOR_CHART_COLOR_HIGHLIGHT];
  chart->color           = table[NK_COLOR_CHART_COLOR];
  chart->padding         = nk_vec2(4, 4);
  chart->border          = 0;
  chart->rounding        = 0;
  chart->color_factor    = 1.0f;
  chart->disabled_factor = NK_WIDGET_DISABLED_FACTOR;
  chart->show_markers    = nk_true;

  /* combo */
  combo                  = &style->combo;
  combo->normal          = nk_style_item_color(table[NK_COLOR_COMBO]);
  combo->hover           = nk_style_item_color(table[NK_COLOR_COMBO]);
  combo->active          = nk_style_item_color(table[NK_COLOR_COMBO]);
  combo->border_color    = table[NK_COLOR_BORDER];
  combo->label_normal    = table[NK_COLOR_TEXT];
  combo->label_hover     = table[NK_COLOR_TEXT];
  combo->label_active    = table[NK_COLOR_TEXT];
  combo->sym_normal      = NK_SYMBOL_TRIANGLE_DOWN;
  combo->sym_hover       = NK_SYMBOL_TRIANGLE_DOWN;
  combo->sym_active      = NK_SYMBOL_TRIANGLE_DOWN;
  combo->content_padding = nk_vec2(4, 4);
  combo->button_padding  = nk_vec2(0, 4);
  combo->spacing         = nk_vec2(4, 0);
  combo->border          = 1;
  combo->rounding        = 0;
  combo->color_factor    = 1.0f;
  combo->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* combo button */
  button = &style->combo.button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_COMBO]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_COMBO]);
  button->active                  = nk_style_item_color(table[NK_COLOR_COMBO]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = table[NK_COLOR_COMBO];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* tab */
  tab                  = &style->tab;
  tab->background      = nk_style_item_color(table[NK_COLOR_TAB_HEADER]);
  tab->border_color    = table[NK_COLOR_BORDER];
  tab->text            = table[NK_COLOR_TEXT];
  tab->sym_minimize    = NK_SYMBOL_TRIANGLE_RIGHT;
  tab->sym_maximize    = NK_SYMBOL_TRIANGLE_DOWN;
  tab->padding         = nk_vec2(4, 4);
  tab->spacing         = nk_vec2(4, 4);
  tab->indent          = 10.0f;
  tab->border          = 1;
  tab->rounding        = 0;
  tab->color_factor    = 1.0f;
  tab->disabled_factor = NK_WIDGET_DISABLED_FACTOR;

  /* tab button */
  button = &style->tab.tab_minimize_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_TAB_HEADER]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_TAB_HEADER]);
  button->active                  = nk_style_item_color(table[NK_COLOR_TAB_HEADER]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = table[NK_COLOR_TAB_HEADER];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;
  style->tab.tab_maximize_button  = *button;

  /* node button */
  button = &style->tab.node_minimize_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->hover                   = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->active                  = nk_style_item_color(table[NK_COLOR_WINDOW]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = table[NK_COLOR_TAB_HEADER];
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(2.0f, 2.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;
  style->tab.node_maximize_button = *button;

  /* window header */
  win                         = &style->window;
  win->header.align           = NK_HEADER_RIGHT;
  win->header.close_symbol    = NK_SYMBOL_X;
  win->header.minimize_symbol = NK_SYMBOL_MINUS;
  win->header.maximize_symbol = NK_SYMBOL_PLUS;
  win->header.normal          = nk_style_item_color(table[NK_COLOR_HEADER]);
  win->header.hover           = nk_style_item_color(table[NK_COLOR_HEADER]);
  win->header.active          = nk_style_item_color(nk_style_blend(table[NK_COLOR_HEADER], table[NK_COLOR_TOGGLE_CURSOR], 0.35f));
  win->header.label_normal    = table[NK_COLOR_TEXT];
  win->header.label_hover     = table[NK_COLOR_TEXT];
  win->header.label_active    = table[NK_COLOR_TEXT];
  win->header.label_padding   = nk_vec2(4, 4);
  win->header.padding         = nk_vec2(4, 4);
  win->header.spacing         = nk_vec2(0, 0);

  /* window header close button */
  button = &style->window.header.close_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  button->hover                   = nk_style_item_color(table[NK_COLOR_BUTTON_HOVER]);
  button->active                  = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = nk_rgba(0, 0, 0, 0);
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(0.0f, 0.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* window header minimize button */
  button = &style->window.header.minimize_button;
  nk_zero_struct(*button);
  button->normal                  = nk_style_item_color(nk_rgba(0, 0, 0, 0));
  button->hover                   = nk_style_item_color(table[NK_COLOR_BUTTON_HOVER]);
  button->active                  = nk_style_item_color(table[NK_COLOR_BUTTON_ACTIVE]);
  button->border_color            = nk_rgba(0, 0, 0, 0);
  button->text_background         = nk_rgba(0, 0, 0, 0);
  button->text_normal             = table[NK_COLOR_TEXT];
  button->text_hover              = table[NK_COLOR_TEXT];
  button->text_active             = table[NK_COLOR_TEXT];
  button->padding                 = nk_vec2(0.0f, 0.0f);
  button->touch_padding           = nk_vec2(0.0f, 0.0f);
  button->userdata                = nk_handle_ptr(0);
  button->text_alignment          = NK_TEXT_CENTERED;
  button->border                  = 0.0f;
  button->rounding                = 0.0f;
  button->color_factor_text       = 1.0f;
  button->color_factor_background = 1.0f;
  button->disabled_factor         = NK_WIDGET_DISABLED_FACTOR;
  button->draw_begin              = 0;
  button->draw_end                = 0;

  /* window */
  win->background              = table[NK_COLOR_WINDOW];
  win->fixed_background        = nk_style_item_color(table[NK_COLOR_WINDOW]);
  win->border_color            = table[NK_COLOR_BORDER];
  win->popup_border_color      = table[NK_COLOR_BORDER];
  win->combo_border_color      = table[NK_COLOR_BORDER];
  win->contextual_border_color = table[NK_COLOR_BORDER];
  win->menu_border_color       = table[NK_COLOR_BORDER];
  win->group_border_color      = table[NK_COLOR_BORDER];
  win->tooltip_border_color    = table[NK_COLOR_BORDER];
  win->scaler                  = nk_style_item_color(table[NK_COLOR_TEXT]);

  win->rounding       = 0.0f;
  win->spacing        = nk_vec2(4, 4);
  win->scrollbar_size = nk_vec2(10, 10);
  win->min_size       = nk_vec2(64, 64);

  win->combo_border           = 1.0f;
  win->contextual_border      = 1.0f;
  win->menu_border            = 1.0f;
  win->group_border           = 1.0f;
  win->tooltip_border         = 1.0f;
  win->popup_border           = 1.0f;
  win->border                 = 2.0f;
  win->min_row_height_padding = 8;

  win->padding            = nk_vec2(4, 4);
  win->group_padding      = nk_vec2(4, 4);
  win->popup_padding      = nk_vec2(4, 4);
  win->combo_padding      = nk_vec2(4, 4);
  win->contextual_padding = nk_vec2(4, 4);
  win->menu_padding       = nk_vec2(4, 4);
  win->tooltip_padding    = nk_vec2(4, 4);

  /* default tooltip just down and to the right of the cursor
   * so it doesn't cover the text
   *
   * TODO might be worth consolidating tooltip styling
   * into its own style structure, though it is a
   * type of window...*/
  win->tooltip_origin = NK_TOP_LEFT;
  win->tooltip_offset = nk_vec2(12, 12);
}

NK_API void
nk_style_set_font(struct nk_context *ctx, const struct nk_user_font *font)
{
  struct nk_style *style;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  style                  = &ctx->style;
  style->font            = font;
  style->font_size       = font ? font->height : 0.0f;
  ctx->stacks.fonts.head = 0;

  if (ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
}

NK_API void
nk_style_set_font_size(struct nk_context *ctx, float size)
{
  NK_ASSERT(ctx);
  NK_ASSERT(size > 0.0f);
  ctx->style.font_size = size;

  if (ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
}

NK_API nk_bool
nk_style_push_font(struct nk_context *ctx, const struct nk_user_font *font)
{
  struct nk_config_stack_user_font         *font_stack;
  struct nk_config_stack_user_font_element *element;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  font_stack = &ctx->stacks.fonts;
  NK_ASSERT(font_stack->head < (int)NK_LEN(font_stack->elements));

  if (font_stack->head >= (int)NK_LEN(font_stack->elements)) {
    return 0;
  }

  element            = &font_stack->elements[font_stack->head++];
  element->address   = &ctx->style.font;
  element->old_value = ctx->style.font;
  ctx->style.font    = font;

  if (ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
  return 1;
}

NK_API nk_bool
nk_style_pop_font(struct nk_context *ctx)
{
  struct nk_config_stack_user_font         *font_stack;
  struct nk_config_stack_user_font_element *element;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  font_stack = &ctx->stacks.fonts;
  NK_ASSERT(font_stack->head > 0);

  if (font_stack->head < 1) {
    return 0;
  }

  element           = &font_stack->elements[--font_stack->head];
  *element->address = element->old_value;

  if (ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
  return 1;
}

#define NK_STYLE_PUSH_IMPLEMENATION(prefix, type, stack)                                        \
  nk_style_push_##type(struct nk_context *ctx, prefix##_##type *address, prefix##_##type value) \
  {                                                                                             \
    struct nk_config_stack_##type           *type_stack;                                        \
    struct nk_config_stack_##type##_element *element;                                           \
    NK_ASSERT(ctx);                                                                             \
    if (!ctx)                                                                                   \
      return 0;                                                                                 \
    type_stack = &ctx->stacks.stack;                                                            \
    NK_ASSERT(type_stack->head < (int)NK_LEN(type_stack->elements));                            \
    if (type_stack->head >= (int)NK_LEN(type_stack->elements))                                  \
      return 0;                                                                                 \
    element            = &type_stack->elements[type_stack->head++];                             \
    element->address   = address;                                                               \
    element->old_value = *address;                                                              \
    *address           = value;                                                                 \
    return 1;                                                                                   \
  }
#define NK_STYLE_POP_IMPLEMENATION(type, stack)                    \
  nk_style_pop_##type(struct nk_context *ctx)                      \
  {                                                                \
    struct nk_config_stack_##type           *type_stack;           \
    struct nk_config_stack_##type##_element *element;              \
    NK_ASSERT(ctx);                                                \
    if (!ctx)                                                      \
      return 0;                                                    \
    type_stack = &ctx->stacks.stack;                               \
    NK_ASSERT(type_stack->head > 0);                               \
    if (type_stack->head < 1)                                      \
      return 0;                                                    \
    element           = &type_stack->elements[--type_stack->head]; \
    *element->address = element->old_value;                        \
    return 1;                                                      \
  }

NK_API nk_bool NK_STYLE_PUSH_IMPLEMENATION(struct nk, style_item, style_items)
NK_API nk_bool
NK_STYLE_PUSH_IMPLEMENATION(nk, float, floats)
NK_API nk_bool NK_STYLE_PUSH_IMPLEMENATION(struct nk, vec2, vectors)
NK_API nk_bool
NK_STYLE_PUSH_IMPLEMENATION(nk, flags, flags)
NK_API nk_bool NK_STYLE_PUSH_IMPLEMENATION(struct nk, color, colors)

NK_API nk_bool
NK_STYLE_POP_IMPLEMENATION(style_item, style_items)
NK_API nk_bool
NK_STYLE_POP_IMPLEMENATION(float, floats)
NK_API nk_bool
NK_STYLE_POP_IMPLEMENATION(vec2, vectors)
NK_API nk_bool
NK_STYLE_POP_IMPLEMENATION(flags, flags)
NK_API nk_bool
NK_STYLE_POP_IMPLEMENATION(color, colors)

NK_API nk_bool
nk_style_push_font_size(struct nk_context *ctx, float size)
{
  nk_bool result;

  NK_ASSERT(size > 0.0f);
  result = nk_style_push_float(ctx, &ctx->style.font_size, size);

  if (result && ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
  return result;
}

NK_API nk_bool
nk_style_pop_font_size(struct nk_context *ctx)
{
  nk_bool result = nk_style_pop_float(ctx);

  if (result && ctx->current) {
    nk_layout_reset_min_row_height(ctx);
  }
  return result;
}

NK_API nk_bool
nk_style_set_cursor(struct nk_context *ctx, enum nk_style_cursor c)
{
  struct nk_style *style;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  style = &ctx->style;

  if (style->cursors[c]) {
    style->cursor_active = style->cursors[c];
    return 1;
  }
  return 0;
}

NK_API void
nk_style_show_cursor(struct nk_context *ctx)
{
  ctx->style.cursor_visible = nk_true;
}

NK_API void
nk_style_hide_cursor(struct nk_context *ctx)
{
  ctx->style.cursor_visible = nk_false;
}

NK_API void
nk_style_load_cursor(struct nk_context *ctx, enum nk_style_cursor cursor, const struct nk_cursor *c)
{
  struct nk_style *style;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  style                  = &ctx->style;
  style->cursors[cursor] = c;
}

NK_API void
nk_style_load_all_cursors(struct nk_context *ctx, const struct nk_cursor *cursors)
{
  int              i = 0;
  struct nk_style *style;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  style = &ctx->style;

  for (i = 0; i < NK_CURSOR_COUNT; ++i) {
    style->cursors[i] = &cursors[i];
  }

  style->cursor_visible = nk_true;
}

/* ==============================================================
 *
 *                          CONTEXT
 *
 * ===============================================================*/
NK_INTERN void
nk_setup(struct nk_context *ctx, const struct nk_user_font *font)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  nk_zero_struct(*ctx);
  nk_style_default(ctx);
  ctx->seq = 1;
#ifdef NK_DEFAULT_NAVIGATION
  ctx->nav.enabled = nk_true;
#endif
  if (font) {
    ctx->style.font      = font;
    ctx->style.font_size = font->height;
  }
#ifdef NK_INCLUDE_VERTEX_BUFFER_OUTPUT
  nk_draw_list_init(&ctx->draw_list);
#endif
}

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
NK_API nk_bool
nk_init_default(struct nk_context *ctx, const struct nk_user_font *font)
{
  struct nk_allocator alloc;

  alloc.userdata.ptr = 0;
  alloc.alloc        = nk_malloc;
  alloc.free         = nk_mfree;
  return nk_init(ctx, &alloc, font);
}
#endif

NK_API nk_bool
nk_init_fixed(struct nk_context *ctx, void *memory, nk_size size, const struct nk_user_font *font)
{
  NK_ASSERT(memory);

  if (!memory) {
    return 0;
  }

  nk_setup(ctx, font);
  nk_buffer_init_fixed(&ctx->memory, memory, size);
  ctx->use_pool = nk_false;
  return 1;
}

NK_API nk_bool
nk_init_custom(struct nk_context *ctx, struct nk_buffer *cmds, struct nk_buffer *pool, const struct nk_user_font *font)
{
  NK_ASSERT(cmds);
  NK_ASSERT(pool);

  if (!cmds || !pool) {
    return 0;
  }

  nk_setup(ctx, font);
  ctx->memory = *cmds;

  if (pool->type == NK_BUFFER_FIXED) {
    /* take memory from buffer and alloc fixed pool */
    nk_pool_init_fixed(&ctx->pool, pool->memory.ptr, pool->memory.size);
  } else {
    /* create dynamic pool from buffer allocator */
    struct nk_allocator *alloc = &pool->pool;

    nk_pool_init(&ctx->pool, alloc, NK_POOL_DEFAULT_CAPACITY);
  }

  ctx->use_pool = nk_true;
  return 1;
}

NK_API nk_bool
nk_init(struct nk_context *ctx, const struct nk_allocator *alloc, const struct nk_user_font *font)
{
  NK_ASSERT(alloc);

  if (!alloc) {
    return 0;
  }

  nk_setup(ctx, font);
  nk_buffer_init(&ctx->memory, alloc, NK_DEFAULT_COMMAND_BUFFER_SIZE);
  nk_pool_init(&ctx->pool, alloc, NK_POOL_DEFAULT_CAPACITY);
  ctx->use_pool = nk_true;
  return 1;
}

#ifdef NK_INCLUDE_COMMAND_USERDATA
NK_API void
nk_set_user_data(struct nk_context *ctx, nk_handle handle)
{
  if (!ctx) {
    return;
  }

  ctx->userdata = handle;

  if (ctx->current) {
    ctx->current->buffer.userdata = handle;
  }
}
#endif

NK_API void
nk_free(struct nk_context *ctx)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  nk_buffer_free(&ctx->memory);

  if (ctx->use_pool) {
    nk_pool_free(&ctx->pool);
  }

  nk_zero(&ctx->input, sizeof(ctx->input));
  nk_zero(&ctx->style, sizeof(ctx->style));
  nk_zero(&ctx->memory, sizeof(ctx->memory));

  ctx->seq      = 0;
  ctx->build    = 0;
  ctx->begin    = 0;
  ctx->end      = 0;
  ctx->active   = 0;
  ctx->current  = 0;
  ctx->freelist = 0;
  ctx->count    = 0;
}

NK_API void
nk_clear(struct nk_context *ctx)
{
  struct nk_window *iter;
  struct nk_window *next;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  if (ctx->use_pool) {
    nk_buffer_clear(&ctx->memory);
  } else {
    nk_buffer_reset(&ctx->memory, NK_BUFFER_FRONT);
  }

  nk_nav_next_frame(ctx);
  ctx->build               = 0;
  ctx->memory.calls        = 0;
  ctx->last_widget_state   = 0;
  ctx->style.cursor_active = ctx->style.cursors[NK_CURSOR_ARROW];
  NK_MEMSET(&ctx->overlay, 0, sizeof(ctx->overlay));

  /* garbage collector */
  iter = ctx->begin;

  while (iter) {
    /* make sure valid minimized windows do not get removed */
    if ((iter->flags & NK_WINDOW_MINIMIZED) && !(iter->flags & NK_WINDOW_CLOSED) && iter->seq == ctx->seq) {
      iter = iter->next;
      continue;
    }

    /* remove hotness from hidden or closed windows*/
    if (((iter->flags & NK_WINDOW_HIDDEN) || (iter->flags & NK_WINDOW_CLOSED)) && iter == ctx->active) {
      ctx->active = iter->prev;
      ctx->end    = iter->prev;

      if (!ctx->end) {
        ctx->begin = 0;
      }

      if (ctx->active) {
        ctx->active->flags &= ~(unsigned)NK_WINDOW_ROM;
      }
    }

    /* free unused popup windows */
    if (iter->popup.win && iter->popup.win->seq != ctx->seq) {
      nk_free_window(ctx, iter->popup.win);
      iter->popup.win = 0;
    }

    /* remove unused window state tables */
    {
      struct nk_table *n, *it = iter->tables;

      while (it) {
        n = it->next;

        if (it->seq != ctx->seq) {
          nk_remove_table(iter, it);
          nk_zero(it, sizeof(union nk_page_data));
          nk_free_table(ctx, it);

          if (it == iter->tables) {
            iter->tables = n;
          }
        }

        it = n;
      }
    }

    /* window itself is not used anymore so free */
    if (iter->seq != ctx->seq || iter->flags & NK_WINDOW_CLOSED) {
      next = iter->next;
      nk_remove_window(ctx, iter);
      nk_free_window(ctx, iter);
      iter = next;
    } else {
      iter = iter->next;
    }
  }

  ctx->seq++;
}

NK_LIB void
nk_start_buffer(struct nk_context *ctx, struct nk_command_buffer *buffer)
{
  NK_ASSERT(ctx);
  NK_ASSERT(buffer);

  if (!ctx || !buffer) {
    return;
  }

  buffer->context = ctx;
  buffer->begin   = ctx->memory.allocated;
  buffer->end     = buffer->begin;
  buffer->last    = buffer->begin;
  buffer->clip    = nk_null_rect;
}

NK_LIB void
nk_start(struct nk_context *ctx, struct nk_window *win)
{
  NK_ASSERT(ctx);
  NK_ASSERT(win);
  nk_start_buffer(ctx, &win->buffer);
}

NK_LIB void
nk_start_popup(struct nk_context *ctx, struct nk_window *win)
{
  struct nk_popup_buffer *buf;

  NK_ASSERT(ctx);
  NK_ASSERT(win);

  if (!ctx || !win) {
    return;
  }

  /* save buffer fill state for popup */
  buf         = &win->popup.buf;
  buf->begin  = win->buffer.end;
  buf->end    = win->buffer.end;
  buf->parent = win->buffer.last;
  buf->last   = buf->begin;
  buf->active = nk_true;
}

NK_LIB void
nk_finish_popup(struct nk_context *ctx, struct nk_window *win)
{
  struct nk_popup_buffer *buf;

  NK_ASSERT(ctx);
  NK_ASSERT(win);

  if (!ctx || !win) {
    return;
  }

  buf       = &win->popup.buf;
  buf->last = win->buffer.last;
  buf->end  = win->buffer.end;
}

NK_LIB void
nk_finish_buffer(struct nk_context *ctx, struct nk_command_buffer *buffer)
{
  NK_ASSERT(ctx);
  NK_ASSERT(buffer);

  if (!ctx || !buffer) {
    return;
  }

  buffer->end = ctx->memory.allocated;
}

NK_LIB void
nk_finish(struct nk_context *ctx, struct nk_window *win)
{
  struct nk_popup_buffer *buf;
  struct nk_command      *parent_last;
  void                   *memory;

  NK_ASSERT(ctx);
  NK_ASSERT(win);

  if (!ctx || !win) {
    return;
  }

  nk_finish_buffer(ctx, &win->buffer);

  if (!win->popup.buf.active) {
    return;
  }

  buf               = &win->popup.buf;
  memory            = ctx->memory.memory.ptr;
  parent_last       = nk_ptr_add(struct nk_command, memory, buf->parent);
  parent_last->next = buf->end;
}

NK_LIB void
nk_build(struct nk_context *ctx)
{
  struct nk_window  *it     = 0;
  struct nk_command *cmd    = 0;
  nk_byte           *buffer = 0;

  /* draw cursor overlay */
  if (!ctx->style.cursor_active) {
    ctx->style.cursor_active = ctx->style.cursors[NK_CURSOR_ARROW];
  }

  if (ctx->style.cursor_active && !ctx->input.mouse.grabbed && ctx->style.cursor_visible) {
    struct nk_rect          mouse_bounds;
    const struct nk_cursor *cursor = ctx->style.cursor_active;

    nk_command_buffer_init(&ctx->overlay, &ctx->memory, NK_CLIPPING_OFF);
    nk_start_buffer(ctx, &ctx->overlay);

    mouse_bounds.x = ctx->input.mouse.pos.x - cursor->offset.x;
    mouse_bounds.y = ctx->input.mouse.pos.y - cursor->offset.y;
    mouse_bounds.w = cursor->size.x;
    mouse_bounds.h = cursor->size.y;

    nk_draw_image(&ctx->overlay, mouse_bounds, &cursor->img, nk_white);
    nk_finish_buffer(ctx, &ctx->overlay);
  }

  /* build one big draw command list out of all window buffers */
  it     = ctx->begin;
  buffer = (nk_byte *)ctx->memory.memory.ptr;

  while (it != 0) {
    struct nk_window *next = it->next;

    if (it->buffer.last == it->buffer.begin || (it->flags & NK_WINDOW_HIDDEN) || it->seq != ctx->seq) {
      goto cont;
    }

    cmd = nk_ptr_add(struct nk_command, buffer, it->buffer.last);

    while (next && ((next->buffer.last == next->buffer.begin) || (next->flags & NK_WINDOW_HIDDEN) || next->seq != ctx->seq)) {
      next = next->next; /* skip empty command buffers */
    }

    if (next) {
      cmd->next = next->buffer.begin;
    }

  cont:
    it = next;
  }

  /* append all popup draw commands into lists */
  it = ctx->begin;

  while (it != 0) {
    struct nk_window       *next = it->next;
    struct nk_popup_buffer *buf;

    if (!it->popup.buf.active) {
      goto skip;
    }

    buf         = &it->popup.buf;
    cmd->next   = buf->begin;
    cmd         = nk_ptr_add(struct nk_command, buffer, buf->last);
    buf->active = nk_false;

  skip:
    it = next;
  }

  if (cmd) {
    /* append overlay commands */
    if (ctx->overlay.end != ctx->overlay.begin) {
      cmd->next = ctx->overlay.begin;
    } else {
      cmd->next = ctx->memory.allocated;
    }
  }
}

NK_API const struct nk_command *
nk__begin(struct nk_context *ctx)
{
  struct nk_window *iter;
  nk_byte          *buffer;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  if (!ctx->count) {
    return 0;
  }

  buffer = (nk_byte *)ctx->memory.memory.ptr;

  if (!ctx->build) {
    nk_build(ctx);
    ctx->build = nk_true;
  }

  iter = ctx->begin;

  while (iter && ((iter->buffer.begin == iter->buffer.end) || (iter->flags & NK_WINDOW_HIDDEN) || iter->seq != ctx->seq)) {
    iter = iter->next;
  }

  if (!iter) {
    return 0;
  }
  return nk_ptr_add_const(struct nk_command, buffer, iter->buffer.begin);
}

NK_API const struct nk_command *
nk__next(struct nk_context *ctx, const struct nk_command *cmd)
{
  nk_byte                 *buffer;
  const struct nk_command *next;

  NK_ASSERT(ctx);

  if (!ctx || !cmd || !ctx->count) {
    return 0;
  }

  if (cmd->next >= ctx->memory.allocated) {
    return 0;
  }

  buffer = (nk_byte *)ctx->memory.memory.ptr;
  next   = nk_ptr_add_const(struct nk_command, buffer, cmd->next);
  return next;
}

/* ===============================================================
 *
 *                              POOL
 *
 * ===============================================================*/
NK_LIB void
nk_pool_init(struct nk_pool *pool, const struct nk_allocator *alloc, unsigned int capacity)
{
  NK_ASSERT(capacity >= 1);
  nk_zero(pool, sizeof(*pool));
  pool->alloc    = *alloc;
  pool->capacity = capacity;
  pool->type     = NK_BUFFER_DYNAMIC;
  pool->pages    = 0;
}

NK_LIB void
nk_pool_free(struct nk_pool *pool)
{
  struct nk_page *iter;

  if (!pool) {
    return;
  }

  iter = pool->pages;

  if (pool->type == NK_BUFFER_FIXED) {
    return;
  }

  while (iter) {
    struct nk_page *next = iter->next;

    pool->alloc.free(pool->alloc.userdata, iter);
    iter = next;
  }
}

NK_LIB void
nk_pool_init_fixed(struct nk_pool *pool, void *memory, nk_size size)
{
  nk_zero(pool, sizeof(*pool));
  NK_ASSERT(size >= sizeof(struct nk_page));

  if (size < sizeof(struct nk_page)) {
    return;
  }

  /* first nk_page_element is embedded in nk_page, additional elements follow in adjacent space */
  pool->capacity = (unsigned)(1 + (size - sizeof(struct nk_page)) / sizeof(struct nk_page_element));
  pool->pages    = (struct nk_page *)memory;
  pool->type     = NK_BUFFER_FIXED;
  pool->size     = size;
}

NK_LIB struct nk_page_element *
nk_pool_alloc(struct nk_pool *pool)
{
  if (!pool->pages || pool->pages->size >= pool->capacity) {
    /* allocate new page */
    struct nk_page *page;

    if (pool->type == NK_BUFFER_FIXED) {
      NK_ASSERT(pool->pages);

      if (!pool->pages) {
        return 0;
      }

      NK_ASSERT(pool->pages->size < pool->capacity);
      return 0;
    } else {
      nk_size size = sizeof(struct nk_page);

      size       += (pool->capacity - 1) * sizeof(struct nk_page_element);
      page        = (struct nk_page *)pool->alloc.alloc(pool->alloc.userdata, 0, size);
      page->next  = pool->pages;
      pool->pages = page;
      page->size  = 0;
    }
  }
  return &pool->pages->win[pool->pages->size++];
}

/* ===============================================================
 *
 *                          PAGE ELEMENT
 *
 * ===============================================================*/
NK_LIB struct nk_page_element *
nk_create_page_element(struct nk_context *ctx)
{
  struct nk_page_element *elem;

  if (ctx->freelist) {
    /* unlink page element from free list */
    elem          = ctx->freelist;
    ctx->freelist = elem->next;
  } else if (ctx->use_pool) {
    /* allocate page element from memory pool */
    elem = nk_pool_alloc(&ctx->pool);
    NK_ASSERT(elem);

    if (!elem) {
      return 0;
    }
  } else {
    /* allocate new page element from back of fixed size memory buffer */
    NK_STORAGE const nk_size size  = sizeof(struct nk_page_element);
    NK_STORAGE const nk_size align = NK_ALIGNOF(struct nk_page_element);

    elem = (struct nk_page_element *)nk_buffer_alloc(&ctx->memory, NK_BUFFER_BACK, size, align);
    NK_ASSERT(elem);

    if (!elem) {
      return 0;
    }
  }

  nk_zero_struct(*elem);
  elem->next = 0;
  elem->prev = 0;
  return elem;
}

NK_LIB void
nk_link_page_element_into_freelist(struct nk_context *ctx, struct nk_page_element *elem)
{
  /* link table into freelist */
  if (!ctx->freelist) {
    ctx->freelist = elem;
  } else {
    elem->next    = ctx->freelist;
    ctx->freelist = elem;
  }
}

NK_LIB void
nk_free_page_element(struct nk_context *ctx, struct nk_page_element *elem)
{
  /* we have a pool so just add to free list */
  if (ctx->use_pool) {
    nk_link_page_element_into_freelist(ctx, elem);
    return;
  }

  /* if possible remove last element from back of fixed memory buffer */
  {
    void *elem_end   = (void *)(elem + 1);
    void *buffer_end = (nk_byte *)ctx->memory.memory.ptr + ctx->memory.size;

    if (elem_end == buffer_end) {
      ctx->memory.size -= sizeof(struct nk_page_element);
    } else {
      nk_link_page_element_into_freelist(ctx, elem);
    }
  }
}

/* ===============================================================
 *
 *                              TABLE
 *
 * ===============================================================*/
NK_LIB struct nk_table *
nk_create_table(struct nk_context *ctx)
{
  struct nk_page_element *elem;

  elem = nk_create_page_element(ctx);

  if (!elem) {
    return 0;
  }
  return &elem->data.tbl;
}

NK_LIB void
nk_free_table(struct nk_context *ctx, struct nk_table *tbl)
{
  union nk_page_data     *pd = NK_CONTAINER_OF(tbl, union nk_page_data, tbl);
  struct nk_page_element *pe = NK_CONTAINER_OF(pd, struct nk_page_element, data);

  nk_free_page_element(ctx, pe);
}

NK_LIB void
nk_push_table(struct nk_window *win, struct nk_table *tbl)
{
  if (!win->tables) {
    win->tables      = tbl;
    tbl->next        = 0;
    tbl->prev        = 0;
    tbl->size        = 0;
    win->table_count = 1;
    return;
  }

  win->tables->prev = tbl;
  tbl->next         = win->tables;
  tbl->prev         = 0;
  tbl->size         = 0;
  win->tables       = tbl;
  win->table_count++;
}

NK_LIB void
nk_remove_table(struct nk_window *win, struct nk_table *tbl)
{
  if (win->tables == tbl) {
    win->tables = tbl->next;
  }

  if (tbl->next) {
    tbl->next->prev = tbl->prev;
  }

  if (tbl->prev) {
    tbl->prev->next = tbl->next;
  }

  tbl->next = 0;
  tbl->prev = 0;
}

NK_LIB nk_uint *
nk_add_value(struct nk_context *ctx, struct nk_window *win, nk_hash name, nk_uint value)
{
  NK_ASSERT(ctx);
  NK_ASSERT(win);

  if (!win || !ctx) {
    return 0;
  }

  if (!win->tables || win->tables->size >= NK_VALUE_PAGE_CAPACITY) {
    struct nk_table *tbl = nk_create_table(ctx);

    NK_ASSERT(tbl);

    if (!tbl) {
      return 0;
    }

    nk_push_table(win, tbl);
  }

  win->tables->seq                       = win->seq;
  win->tables->keys[win->tables->size]   = name;
  win->tables->values[win->tables->size] = value;
  return &win->tables->values[win->tables->size++];
}

NK_LIB nk_uint *
nk_find_value(const struct nk_window *win, nk_hash name)
{
  struct nk_table *iter = win->tables;

  while (iter) {
    unsigned int i    = 0;
    unsigned int size = iter->size;

    for (i = 0; i < size; ++i) {
      if (iter->keys[i] == name) {
        iter->seq = win->seq;
        return &iter->values[i];
      }
    }

    size = NK_VALUE_PAGE_CAPACITY;
    iter = iter->next;
  }
  return 0;
}

enum nk_text_selection_storage_field {
  NK_TEXT_SELECTION_STORAGE_ANCHOR,
  NK_TEXT_SELECTION_STORAGE_CURSOR,
  NK_TEXT_SELECTION_STORAGE_FLAGS,
  NK_TEXT_SELECTION_STORAGE_ANCHOR_WIDGET,
  NK_TEXT_SELECTION_STORAGE_CURSOR_WIDGET,
  NK_TEXT_SELECTION_STORAGE_INDEX,
  NK_TEXT_SELECTION_STORAGE_WIDGET_COUNT,
  NK_TEXT_SELECTION_STORAGE_FRAME,
  NK_TEXT_SELECTION_STORAGE_COUNT
};

NK_LIB struct nk_window *
nk_text_selection_storage_window(struct nk_window *win)
{
  while (win->parent) {
    win = win->parent;
  }
  return win;
}

NK_LIB nk_hash
nk_text_selection_storage_key(nk_hash scope, int field)
{
  return nk_murmur_hash(&scope, sizeof(scope), (nk_hash)(0x74584310u + field));
}

NK_LIB void
nk_text_selection_panel_load(struct nk_context *ctx, struct nk_panel *panel)
{
  struct nk_window *win = NULL;
  nk_uint          *value[NK_TEXT_SELECTION_STORAGE_COUNT];
  nk_uint           flags;
  int               i;

  win                          = nk_text_selection_storage_window(ctx->current);
  panel->text_selection_loaded = nk_true;

  for (i = 0; i < NK_TEXT_SELECTION_STORAGE_COUNT; ++i) {
    value[i] = nk_find_value(win, nk_text_selection_storage_key(panel->text_selection_scope, i));

    if (!value[i]) {
      return;
    }
  }

  flags                                           = *value[NK_TEXT_SELECTION_STORAGE_FLAGS];
  panel->text_selection.selection.anchor          = (int)*value[NK_TEXT_SELECTION_STORAGE_ANCHOR];
  panel->text_selection.selection.cursor          = (int)*value[NK_TEXT_SELECTION_STORAGE_CURSOR];
  panel->text_selection.selection.active          = (unsigned char)(flags & 0xffu);
  panel->text_selection.selection.dragging        = (unsigned char)((flags >> 8) & 0xffu);
  panel->text_selection.selection.affinity        = (unsigned char)((flags >> 16) & 0xffu);
  panel->text_selection.selection.anchor_affinity = (unsigned char)((flags >> 24) & 0xffu);
  panel->text_selection.anchor_widget             = *value[NK_TEXT_SELECTION_STORAGE_ANCHOR_WIDGET];
  panel->text_selection.cursor_widget             = *value[NK_TEXT_SELECTION_STORAGE_CURSOR_WIDGET];
  panel->text_selection.widget_index              = *value[NK_TEXT_SELECTION_STORAGE_INDEX];
  panel->text_selection.widget_count              = *value[NK_TEXT_SELECTION_STORAGE_WIDGET_COUNT];
  panel->text_selection.frame                     = *value[NK_TEXT_SELECTION_STORAGE_FRAME];
}

NK_LIB void
nk_text_selection_panel_save(struct nk_context *ctx, struct nk_panel *panel)
{
  struct nk_window *win = nk_text_selection_storage_window(ctx->current);
  nk_uint           value[NK_TEXT_SELECTION_STORAGE_COUNT];
  int               i;
  nk_uint           flags;

  flags = (nk_uint)(panel->text_selection.selection.active          <<  0) |
          (nk_uint)(panel->text_selection.selection.dragging        <<  8) |
          (nk_uint)(panel->text_selection.selection.affinity        << 16) |
          (nk_uint)(panel->text_selection.selection.anchor_affinity << 24);

  value[NK_TEXT_SELECTION_STORAGE_ANCHOR]        = (nk_uint)panel->text_selection.selection.anchor;
  value[NK_TEXT_SELECTION_STORAGE_CURSOR]        = (nk_uint)panel->text_selection.selection.cursor;
  value[NK_TEXT_SELECTION_STORAGE_FLAGS]         = flags;
  value[NK_TEXT_SELECTION_STORAGE_ANCHOR_WIDGET] = panel->text_selection.anchor_widget;
  value[NK_TEXT_SELECTION_STORAGE_CURSOR_WIDGET] = panel->text_selection.cursor_widget;
  value[NK_TEXT_SELECTION_STORAGE_INDEX]         = panel->text_selection.widget_index;
  value[NK_TEXT_SELECTION_STORAGE_WIDGET_COUNT]  = panel->text_selection.widget_count;
  value[NK_TEXT_SELECTION_STORAGE_FRAME]         = panel->text_selection.frame;

  for (i = 0; i < NK_TEXT_SELECTION_STORAGE_COUNT; ++i) {
    nk_hash  key   = nk_text_selection_storage_key(panel->text_selection_scope, i);
    nk_uint *entry = nk_find_value(win, key);

    if (!entry) {
      entry = nk_add_value(ctx, win, key, value[i]);
    }

    if (entry) {
      *entry = value[i];
    }
  }
}

/* ===============================================================
 *
 *                              PANEL
 *
 * ===============================================================*/
NK_LIB void *
nk_create_panel(struct nk_context *ctx)
{
  struct nk_page_element *elem;

  elem = nk_create_page_element(ctx);

  if (!elem) {
    return 0;
  }
  return &elem->data.pan;
}

NK_LIB void
nk_free_panel(struct nk_context *ctx, struct nk_panel *pan)
{
  union nk_page_data     *pd = NK_CONTAINER_OF(pan, union nk_page_data, pan);
  struct nk_page_element *pe = NK_CONTAINER_OF(pd, struct nk_page_element, data);

  nk_free_page_element(ctx, pe);
}

NK_LIB nk_bool
nk_panel_has_header(nk_flags flags, const char *title)
{
  nk_bool active = 0;

  active = (flags & (NK_WINDOW_CLOSABLE | NK_WINDOW_MINIMIZABLE));
  active = active || (flags & NK_WINDOW_TITLE);
  active = active && !(flags & NK_WINDOW_HIDDEN) && title;
  return active;
}

NK_LIB struct nk_vec2
nk_panel_get_padding(const struct nk_style *style, enum nk_panel_type type)
{
  switch (type) {
    default:
    case NK_PANEL_WINDOW:
      return style->window.padding;

    case NK_PANEL_GROUP:
      return style->window.group_padding;

    case NK_PANEL_POPUP:
      return style->window.popup_padding;

    case NK_PANEL_CONTEXTUAL:
      return style->window.contextual_padding;

    case NK_PANEL_COMBO:
      return style->window.combo_padding;

    case NK_PANEL_MENU:
      return style->window.menu_padding;

    case NK_PANEL_TOOLTIP:
      return style->window.menu_padding;
  }
}

NK_LIB float
nk_panel_get_border(const struct nk_style *style, nk_flags flags, enum nk_panel_type type)
{
  if (flags & NK_WINDOW_BORDER) {
    switch (type) {
      default:
      case NK_PANEL_WINDOW:
        return style->window.border;

      case NK_PANEL_GROUP:
        return style->window.group_border;

      case NK_PANEL_POPUP:
        return style->window.popup_border;

      case NK_PANEL_CONTEXTUAL:
        return style->window.contextual_border;

      case NK_PANEL_COMBO:
        return style->window.combo_border;

      case NK_PANEL_MENU:
        return style->window.menu_border;

      case NK_PANEL_TOOLTIP:
        return style->window.menu_border;
    }
  } else {
    return 0;
  }
}

NK_LIB struct nk_color
nk_panel_get_border_color(const struct nk_style *style, enum nk_panel_type type)
{
  switch (type) {
    default:
    case NK_PANEL_WINDOW:
      return style->window.border_color;

    case NK_PANEL_GROUP:
      return style->window.group_border_color;

    case NK_PANEL_POPUP:
      return style->window.popup_border_color;

    case NK_PANEL_CONTEXTUAL:
      return style->window.contextual_border_color;

    case NK_PANEL_COMBO:
      return style->window.combo_border_color;

    case NK_PANEL_MENU:
      return style->window.menu_border_color;

    case NK_PANEL_TOOLTIP:
      return style->window.menu_border_color;
  }
}

NK_LIB nk_bool
nk_panel_is_sub(enum nk_panel_type type)
{
  return ((int)type & (int)NK_PANEL_SET_SUB) ? 1 : 0;
}

NK_LIB nk_bool
nk_panel_is_nonblock(enum nk_panel_type type)
{
  return ((int)type & (int)NK_PANEL_SET_NONBLOCK) ? 1 : 0;
}

NK_LIB nk_bool
nk_panel_begin(struct nk_context *ctx, const char *title, enum nk_panel_type panel_type)
{
  struct nk_input           *in;
  struct nk_window          *win;
  struct nk_panel           *layout;
  struct nk_command_buffer  *out;
  const struct nk_style     *style;
  const struct nk_user_font *font;

  struct nk_vec2 scrollbar_size;
  struct nk_vec2 panel_padding;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  nk_zero(ctx->current->layout, sizeof(*ctx->current->layout));

  if ((ctx->current->flags & NK_WINDOW_HIDDEN) || (ctx->current->flags & NK_WINDOW_CLOSED)) {
    nk_zero(ctx->current->layout, sizeof(struct nk_panel));
    ctx->current->layout->type = panel_type;
    return 0;
  }

  /* pull state into local stack */
  style  = &ctx->style;
  font   = style->font;
  win    = ctx->current;
  layout = win->layout;
  out    = &win->buffer;
  in     = (win->flags & NK_WINDOW_NO_INPUT) ? 0 : &ctx->input;

  if (panel_type == NK_PANEL_WINDOW) {
    layout->text_selection_scope = nk_murmur_hash(&win->name, sizeof(win->name), (nk_hash)panel_type);
  } else {
    layout->text_selection_scope = nk_murmur_hash(title, title ? (int)nk_strlen(title) : 0, win->name ^ (nk_hash)panel_type);
  }
#ifdef NK_INCLUDE_COMMAND_USERDATA
  win->buffer.userdata = ctx->userdata;
#endif
  /* pull style configuration into local stack */
  scrollbar_size = style->window.scrollbar_size;
  panel_padding  = nk_panel_get_padding(style, panel_type);

  /* window movement */
  if ((win->flags & NK_WINDOW_MOVABLE) && !(win->flags & NK_WINDOW_ROM) && !(win->flags & NK_WINDOW_NO_INPUT)) {
    nk_bool      left_mouse_down;
    unsigned int left_mouse_clicked;
    int          left_mouse_click_in_cursor;

    /* calculate draggable window space */
    struct nk_rect header;

    header.x = win->bounds.x;
    header.y = win->bounds.y;
    header.w = win->bounds.w;

    if (nk_panel_has_header(win->flags, title)) {
      header.h  = style->font_size + 2.0f * style->window.header.padding.y;
      header.h += 2.0f * style->window.header.label_padding.y;
    } else {
      header.h = panel_padding.y;
    }

    /* window movement by dragging */
    left_mouse_down            = in->mouse.buttons[NK_BUTTON_LEFT].down;
    left_mouse_clicked         = in->mouse.buttons[NK_BUTTON_LEFT].clicked;
    left_mouse_click_in_cursor = nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, header, nk_true);

    if (!(win->flags & NK_WINDOW_MAXIMIZED) && left_mouse_down && left_mouse_click_in_cursor && !left_mouse_clicked) {
      win->bounds.x                                    = win->bounds.x + in->mouse.delta.x;
      win->bounds.y                                    = win->bounds.y + in->mouse.delta.y;
      in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x += in->mouse.delta.x;
      in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.y += in->mouse.delta.y;
      ctx->style.cursor_active                         = ctx->style.cursors[NK_CURSOR_MOVE];
    }
  }

  /* setup panel */
  layout->type      = panel_type;
  layout->flags     = win->flags;
  layout->bounds    = win->bounds;
  layout->bounds.x += panel_padding.x;
  layout->bounds.w -= 2 * panel_padding.x;

  if (win->flags & NK_WINDOW_BORDER) {
    layout->border = nk_panel_get_border(style, win->flags, panel_type);
    layout->bounds = nk_shrink_rect(layout->bounds, layout->border);
  } else {
    layout->border = 0;
  }

  layout->at_y          = layout->bounds.y;
  layout->at_x          = layout->bounds.x;
  layout->max_x         = 0;
  layout->header_height = 0;
  layout->footer_height = 0;
  nk_layout_reset_min_row_height(ctx);
  layout->row.index      = 0;
  layout->row.columns    = 0;
  layout->row.ratio      = 0;
  layout->row.item_width = 0;
  layout->row.tree_depth = 0;
  layout->row.height     = panel_padding.y;
  layout->has_scrolling  = nk_true;

  if (!(win->flags & NK_WINDOW_NO_SCROLLBAR_V)) {
    layout->bounds.w -= scrollbar_size.x;
  }

  if (!nk_panel_is_nonblock(panel_type)) {
    layout->footer_height = 0;

    if (!(win->flags & NK_WINDOW_NO_SCROLLBAR_H) || win->flags & NK_WINDOW_SCALABLE) {
      layout->footer_height = scrollbar_size.y;
    }

    layout->bounds.h -= layout->footer_height;
  }

  /* panel header */
  if (nk_panel_has_header(win->flags, title)) {
    struct nk_text              text;
    struct nk_rect              header;
    const struct nk_style_item *background = 0;

    /* calculate header bounds */
    header.x  = win->bounds.x;
    header.y  = win->bounds.y;
    header.w  = win->bounds.w;
    header.h  = style->font_size + 2.0f * style->window.header.padding.y;
    header.h += (2.0f * style->window.header.label_padding.y);

    /* shrink panel by header */
    layout->header_height = header.h;
    layout->bounds.y     += header.h;
    layout->bounds.h     -= header.h;
    layout->at_y         += header.h;

    /* select correct header background and text color */
    if (ctx->active == win) {
      background = &style->window.header.active;
      text.text  = style->window.header.label_active;
    } else if (nk_input_is_mouse_hovering_rect(&ctx->input, header)) {
      background = &style->window.header.hover;
      text.text  = style->window.header.label_hover;
    } else {
      background = &style->window.header.normal;
      text.text  = style->window.header.label_normal;
    }

    /* draw header background */
    header.h         += 1.0f;
    ctx->nav.suppress = nk_true;

    switch (background->type) {
      case NK_STYLE_ITEM_IMAGE:
        text.background = nk_rgba(0, 0, 0, 0);
        nk_draw_image(&win->buffer, header, &background->data.image, nk_white);
        break;

      case NK_STYLE_ITEM_NINE_SLICE:
        text.background = nk_rgba(0, 0, 0, 0);
        nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_white);
        break;

      case NK_STYLE_ITEM_COLOR:
        text.background = background->data.color;
        nk_fill_rect(out, header, 0, background->data.color);
        break;
    }

    /* window buttons */
    {
      struct nk_rect button;

      button.y = header.y + style->window.header.padding.y;
      button.h = header.h - 2 * style->window.header.padding.y;
      button.w = button.h;

      /* window close button */
      if (win->flags & NK_WINDOW_CLOSABLE) {
        nk_flags ws = 0;

        if (style->window.header.align == NK_HEADER_RIGHT) {
          button.x  = (header.w + header.x) - (button.w + style->window.header.padding.x);
          header.w -= button.w + style->window.header.spacing.x + style->window.header.padding.x;
        } else {
          button.x  = header.x + style->window.header.padding.x;
          header.x += button.w + style->window.header.spacing.x + style->window.header.padding.x;
        }

        if (nk_do_button_symbol(&ws, &win->buffer, button, style->window.header.close_symbol, NK_BUTTON_DEFAULT, &style->window.header.close_button, in, style->font) &&
            !(win->flags & NK_WINDOW_ROM)) {
          layout->flags |= NK_WINDOW_HIDDEN;

          if (!(layout->flags & NK_WINDOW_CLOSE_BUTTON_HIDES)) {
            layout->flags |= NK_WINDOW_CLOSED;
          }

          layout->flags &= (nk_flags)~NK_WINDOW_MINIMIZED;
        }
      }

      /* window maximize button */
      if (win->flags & NK_WINDOW_MAXIMIZABLE) {
        struct nk_style_button maximize_button;
        nk_flags               ws = 0;
        float                  icon_pad;

        if (style->window.header.align == NK_HEADER_RIGHT) {
          button.x = (header.w + header.x) - button.w;

          if (!(win->flags & NK_WINDOW_CLOSABLE)) {
            button.x -= style->window.header.padding.x;
            header.w -= style->window.header.padding.x;
          }

          header.w -= button.w + style->window.header.spacing.x;
        } else {
          button.x  = header.x;
          header.x += button.w + style->window.header.spacing.x + style->window.header.padding.x;
        }

        maximize_button           = style->window.header.minimize_button;
        icon_pad                  = NK_MAX(4.0f, button.h * 0.25f);
        maximize_button.padding.x = NK_MAX(maximize_button.padding.x, icon_pad);
        maximize_button.padding.y = NK_MAX(maximize_button.padding.y, icon_pad);

        if (nk_do_button_symbol(&ws,
                                &win->buffer,
                                button,
                                (win->flags & NK_WINDOW_MAXIMIZED) ? NK_SYMBOL_RECT_SOLID : NK_SYMBOL_RECT_OUTLINE,
                                NK_BUTTON_DEFAULT,
                                &maximize_button,
                                in,
                                style->font) &&
            !(win->flags & NK_WINDOW_ROM)) {
          if (win->flags & NK_WINDOW_MAXIMIZED) {
            win->bounds    = win->restore_bounds;
            win->flags    &= (nk_flags)~NK_WINDOW_MAXIMIZED;
            layout->flags &= (nk_flags)~NK_WINDOW_MAXIMIZED;
          } else {
            struct nk_rect target = ctx->window_maximize_bounds;

            if (target.w > 0.0f && target.h > 0.0f) {
              win->restore_bounds = win->bounds;
              win->bounds         = target;

              win->flags    |= NK_WINDOW_MAXIMIZED;
              layout->flags |= NK_WINDOW_MAXIMIZED;

              win->flags    &= (nk_flags)~NK_WINDOW_MINIMIZED;
              layout->flags &= (nk_flags)~NK_WINDOW_MINIMIZED;
            }
          }
        }
      }

      /* window minimize button */
      if (win->flags & NK_WINDOW_MINIMIZABLE) {
        nk_flags ws = 0;

        if (style->window.header.align == NK_HEADER_RIGHT) {
          button.x = (header.w + header.x) - button.w;

          if (!(win->flags & NK_WINDOW_CLOSABLE)) {
            button.x -= style->window.header.padding.x;
            header.w -= style->window.header.padding.x;
          }

          header.w -= button.w + style->window.header.spacing.x;
        } else {
          button.x  = header.x;
          header.x += button.w + style->window.header.spacing.x + style->window.header.padding.x;
        }

        if (nk_do_button_symbol(&ws,
                                &win->buffer,
                                button,
                                (layout->flags & NK_WINDOW_MINIMIZED) ? style->window.header.maximize_symbol : style->window.header.minimize_symbol,
                                NK_BUTTON_DEFAULT,
                                &style->window.header.minimize_button,
                                in,
                                style->font) &&
            !(win->flags & NK_WINDOW_ROM)) {
          layout->flags = (layout->flags & NK_WINDOW_MINIMIZED) ? layout->flags & (nk_flags)~NK_WINDOW_MINIMIZED : layout->flags | NK_WINDOW_MINIMIZED;
        }
      }
    }

    ctx->nav.suppress = nk_false;

    { /* window header title */
      int            text_len = nk_strlen(title);
      struct nk_rect label    = {0, 0, 0, 0};
      float          t        = nk_text_width(ctx, font, style->font_size, title, text_len);

      text.padding = nk_vec2(0, 0);

      label.x  = header.x + style->window.header.padding.x;
      label.x += style->window.header.label_padding.x;
      label.y  = header.y + style->window.header.label_padding.y;
      label.h  = style->font_size + 2 * style->window.header.label_padding.y;
      label.w  = t + 2 * style->window.header.spacing.x;
      label.w  = NK_CLAMP(0, label.w, header.x + header.w - label.x);
      nk_widget_text(out, label, (const char *)title, text_len, &text, NK_TEXT_LEFT, font);
    }
  }

  /* draw window background */
  if (!(layout->flags & NK_WINDOW_MINIMIZED) && !(layout->flags & NK_WINDOW_DYNAMIC)) {
    struct nk_rect body;

    body.x = win->bounds.x;
    body.w = win->bounds.w;
    body.y = (win->bounds.y + layout->header_height);
    body.h = (win->bounds.h - layout->header_height);

    switch (style->window.fixed_background.type) {
      case NK_STYLE_ITEM_IMAGE:
        nk_draw_image(out, body, &style->window.fixed_background.data.image, nk_white);
        break;

      case NK_STYLE_ITEM_NINE_SLICE:
        nk_draw_nine_slice(out, body, &style->window.fixed_background.data.slice, nk_white);
        break;

      case NK_STYLE_ITEM_COLOR:
        nk_fill_rect(out, body, style->window.rounding, style->window.fixed_background.data.color);
        break;
    }
  }

  /* set clipping rectangle */
  {
    struct nk_rect clip;

    layout->clip = layout->bounds;
    nk_unify(&clip, &win->buffer.clip, layout->clip.x, layout->clip.y, layout->clip.x + layout->clip.w, layout->clip.y + layout->clip.h);
    nk_push_scissor(out, clip);
    layout->clip = clip;
  }
  return !(layout->flags & NK_WINDOW_HIDDEN) && !(layout->flags & NK_WINDOW_MINIMIZED);
}

NK_INTERN void
nk_panel_disable_parent_scrolling(struct nk_panel *layout)
{
  struct nk_panel *parent;

  for (parent = layout ? layout->parent : 0; parent; parent = parent->parent) {
    parent->has_scrolling = nk_false;
  }
}

NK_LIB void
nk_panel_end(struct nk_context *ctx)
{
  struct nk_input          *in;
  struct nk_window         *window;
  struct nk_panel          *layout;
  const struct nk_style    *style;
  struct nk_command_buffer *out;

  struct nk_vec2 scrollbar_size;
  struct nk_vec2 panel_padding;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  window = ctx->current;
  layout = window->layout;
  style  = &ctx->style;
  out    = &window->buffer;
  in     = (layout->flags & NK_WINDOW_ROM || layout->flags & NK_WINDOW_NO_INPUT) ? 0 : &ctx->input;

  if (layout->text_selection_loaded) {
    nk_text_selection_panel_save(ctx, layout);
  }

  if (!nk_panel_is_sub(layout->type)) {
    nk_push_scissor(out, nk_null_rect);
  }

  /* cache configuration data */
  scrollbar_size = style->window.scrollbar_size;
  panel_padding  = nk_panel_get_padding(style, layout->type);

  /* update the current cursor Y-position to point over the last added widget */
  layout->at_y += layout->row.height;

  /* dynamic panels */
  if (layout->flags & NK_WINDOW_DYNAMIC && !(layout->flags & NK_WINDOW_MINIMIZED)) {
    /* update panel height to fit dynamic growth */
    struct nk_rect empty_space;

    if (layout->at_y < (layout->bounds.y + layout->bounds.h)) {
      layout->bounds.h = layout->at_y - layout->bounds.y;
    }

    /* fill top empty space */
    empty_space.x = window->bounds.x;
    empty_space.y = layout->bounds.y;
    empty_space.h = panel_padding.y;
    empty_space.w = window->bounds.w;
    nk_fill_rect(out, empty_space, 0, style->window.background);

    /* fill left empty space */
    empty_space.x = window->bounds.x;
    empty_space.y = layout->bounds.y;
    empty_space.w = panel_padding.x + layout->border;
    empty_space.h = layout->bounds.h;
    nk_fill_rect(out, empty_space, 0, style->window.background);

    /* fill right empty space */
    empty_space.x = layout->bounds.x + layout->bounds.w;
    empty_space.y = layout->bounds.y;
    empty_space.w = panel_padding.x + layout->border;
    empty_space.h = layout->bounds.h;

    if (*layout->offset_y == 0 && !(layout->flags & NK_WINDOW_NO_SCROLLBAR_V)) {
      empty_space.w += scrollbar_size.x;
    }

    nk_fill_rect(out, empty_space, 0, style->window.background);

    /* fill bottom empty space */
    if (layout->footer_height > 0) {
      empty_space.x = window->bounds.x;
      empty_space.y = layout->bounds.y + layout->bounds.h;
      empty_space.w = window->bounds.w;
      empty_space.h = layout->footer_height;
      nk_fill_rect(out, empty_space, 0, style->window.background);
    }
  }

  /* scrollbars */
  if ((!(layout->flags & NK_WINDOW_NO_SCROLLBAR_V) || !(layout->flags & NK_WINDOW_NO_SCROLLBAR_H)) && !(layout->flags & NK_WINDOW_MINIMIZED) &&
      window->scrollbar_hiding_timer < NK_SCROLLBAR_HIDING_TIMEOUT) {
    struct nk_rect scroll;
    int            scroll_has_scrolling;
    float          scroll_target;
    float          scroll_offset;
    float          scroll_step;
    float          scroll_inc;
    nk_bool        scroll_consumed = nk_false;
    nk_bool        shift_down;
    nk_bool        can_shift_scroll_h;
    float          h_scroll_target;
    float          h_max_offset;

    /* mouse wheel scrolling */
    if (nk_panel_is_sub(layout->type)) {
      /* sub-window mouse wheel scrolling */
      struct nk_window *root_window = window;
      struct nk_panel  *root_panel  = window->layout;

      while (root_panel->parent) {
        root_panel = root_panel->parent;
      }

      while (root_window->parent) {
        root_window = root_window->parent;
      }

      /* only allow scrolling if parent window is active */
      scroll_has_scrolling = nk_false;

      if ((root_window == ctx->active) && layout->has_scrolling) {
        /* and panel is being hovered and inside clip rect*/
        if (nk_input_is_mouse_hovering_rect(in, layout->bounds) &&
            NK_INTERSECT(layout->bounds.x, layout->bounds.y, layout->bounds.w, layout->bounds.h, root_panel->clip.x, root_panel->clip.y, root_panel->clip.w, root_panel->clip.h)) {
          scroll_has_scrolling = nk_true;
        }
      }
    } else {
      /* window mouse wheel scrolling */
      scroll_has_scrolling = (window == ctx->active) && layout->has_scrolling;

      if (in && (in->mouse.scroll_delta.y != 0.0f || in->mouse.scroll_delta.x != 0.0f) && scroll_has_scrolling) {
        window->scrolled = nk_true;
      } else {
        window->scrolled = nk_false;
      }
    }

    shift_down         = in && nk_input_is_key_down(in, NK_KEY_SHIFT);
    h_scroll_target    = (float)(int)(layout->max_x - layout->bounds.x);
    h_max_offset       = NK_MAX(h_scroll_target - layout->bounds.w, 0.0f);
    can_shift_scroll_h = shift_down && !(layout->flags & NK_WINDOW_NO_SCROLLBAR_H) && h_max_offset > 0.0f;

    if (!(layout->flags & NK_WINDOW_NO_SCROLLBAR_V)) {
      /* vertical scrollbar */
      nk_flags state          = 0;
      float    old_max_offset = 0.0f;
      float    new_max_offset = 0.0f;
      float    input_scroll_y;
      nk_bool  has_y_wheel_input;
      nk_uint  before_offset_y;
      nk_uint  after_offset_y;
      nk_bool  y_consumed;

      scroll.x = layout->bounds.x + layout->bounds.w + panel_padding.x;
      scroll.y = layout->bounds.y;
      scroll.w = scrollbar_size.x;
      scroll.h = layout->bounds.h;

      scroll_offset = (float)*layout->offset_y;
      scroll_step   = scroll.h * 0.10f;
      scroll_inc    = scroll.h * 0.01f;
      scroll_target = (float)(int)(layout->at_y - scroll.y);

      old_max_offset = layout->prev_max_y ? (float)*layout->prev_max_y : 0.0f;
      new_max_offset = NK_MAX(scroll_target - scroll.h, 0.0f);

      if (layout->prev_max_y && layout->scroll_flags && (*layout->scroll_flags & NK_SCROLL_HAS_PREV_MAX_Y)) {
        nk_bool had_scrollbar = old_max_offset > 0.0f;
        nk_bool was_at_end    = had_scrollbar && (scroll_offset >= NK_MAX(old_max_offset - 1.0f, 0.0f));

        if (was_at_end && new_max_offset > old_max_offset) {
          scroll_offset = new_max_offset;
        }
      }

      input_scroll_y    = in ? in->mouse.scroll_delta.y : 0.0f;
      has_y_wheel_input = input_scroll_y != 0.0f && !can_shift_scroll_h;
      before_offset_y   = *layout->offset_y;

      scroll_offset = nk_do_scrollbarv(
        &state, out, scroll, scroll_has_scrolling && !can_shift_scroll_h, scroll_offset, scroll_target, scroll_step, scroll_inc, &ctx->style.scrollv, in, style->font);

      after_offset_y = (nk_uint)scroll_offset;
      y_consumed     = has_y_wheel_input && after_offset_y != before_offset_y;

      *layout->offset_y = after_offset_y;

      if (y_consumed) {
        scroll_consumed          = nk_true;
        in->mouse.scroll_delta.y = 0.0f;
      }

      if (layout->prev_max_y && layout->scroll_flags) {
        *layout->prev_max_y    = (nk_uint)new_max_offset;
        *layout->scroll_flags |= NK_SCROLL_HAS_PREV_MAX_Y;
      }
    } else if (layout->offset_y) {
      *layout->offset_y = 0;

      if (layout->prev_max_y && layout->scroll_flags) {
        *layout->prev_max_y    = 0;
        *layout->scroll_flags &= ~(nk_uint)NK_SCROLL_HAS_PREV_MAX_Y;
      }
    }

    if (!(layout->flags & NK_WINDOW_NO_SCROLLBAR_H)) {
      /* horizontal scrollbar */
      nk_flags state          = 0;
      float    old_max_offset = 0.0f;
      float    new_max_offset = 0.0f;
      float    input_scroll_x;
      nk_bool  has_x_wheel_input;
      nk_uint  before_offset_x;
      nk_uint  after_offset_x;
      nk_bool  x_consumed;

      scroll.x = layout->bounds.x;
      scroll.y = layout->bounds.y + layout->bounds.h;
      scroll.w = layout->bounds.w;
      scroll.h = scrollbar_size.y;

      scroll_offset = (float)*layout->offset_x;
      scroll_target = (float)(int)(layout->max_x - scroll.x);
      scroll_step   = layout->max_x * 0.05f;
      scroll_inc    = layout->max_x * 0.005f;

      old_max_offset = layout->prev_max_x ? (float)*layout->prev_max_x : 0.0f;
      new_max_offset = NK_MAX(scroll_target - scroll.w, 0.0f);

      if (layout->prev_max_x && layout->scroll_flags && (*layout->scroll_flags & NK_SCROLL_HAS_PREV_MAX_X)) {
        nk_bool had_scrollbar = old_max_offset > 0.0f;
        nk_bool was_at_end    = had_scrollbar && (scroll_offset >= NK_MAX(old_max_offset - 1.0f, 0.0f));

        if (was_at_end && new_max_offset > old_max_offset) {
          scroll_offset = new_max_offset;
        }
      }

      input_scroll_x = in ? in->mouse.scroll_delta.x : 0.0f;

      if (input_scroll_x == 0.0f && shift_down && in) {
        input_scroll_x = in->mouse.scroll_delta.y;
      }

      has_x_wheel_input = input_scroll_x != 0.0f;
      before_offset_x   = *layout->offset_x;

      scroll_offset = nk_do_scrollbarh(&state, out, scroll, scroll_has_scrolling, scroll_offset, scroll_target, scroll_step, scroll_inc, &ctx->style.scrollh, in, style->font);

      after_offset_x = (nk_uint)scroll_offset;
      x_consumed     = has_x_wheel_input && after_offset_x != before_offset_x;

      *layout->offset_x = after_offset_x;

      if (x_consumed) {
        scroll_consumed = nk_true;

        if (in->mouse.scroll_delta.x != 0.0f) {
          in->mouse.scroll_delta.x = 0.0f;
        }

        if (shift_down) {
          in->mouse.scroll_delta.y = 0.0f;
        }
      }

      if (layout->prev_max_x && layout->scroll_flags) {
        *layout->prev_max_x    = (nk_uint)new_max_offset;
        *layout->scroll_flags |= NK_SCROLL_HAS_PREV_MAX_X;
      }
    } else if (layout->offset_x) {
      *layout->offset_x = 0;

      if (layout->prev_max_x && layout->scroll_flags) {
        *layout->prev_max_x    = 0;
        *layout->scroll_flags &= ~(nk_uint)NK_SCROLL_HAS_PREV_MAX_X;
      }
    }

    if (scroll_consumed && nk_panel_is_sub(layout->type)) {
      nk_panel_disable_parent_scrolling(layout);
    }
  }

  /* hide scroll if no user input */
  if (window->flags & NK_WINDOW_SCROLL_AUTO_HIDE) {
    int has_input         = nk_input_is_mouse_moved(&ctx->input) || ctx->input.mouse.scroll_delta.y != 0;
    int is_window_hovered = nk_window_is_hovered(ctx);
    int any_item_active   = (ctx->last_widget_state & NK_WIDGET_STATE_MODIFIED);

    if ((!has_input && is_window_hovered) || (!is_window_hovered && !any_item_active)) {
      window->scrollbar_hiding_timer += ctx->delta_time_seconds;
    } else {
      window->scrollbar_hiding_timer = 0;
    }
  } else {
    window->scrollbar_hiding_timer = 0;
  }

  {
    nk_bool navigated = layout->type == NK_PANEL_WINDOW && ctx->nav.enabled && ctx->nav.active && ctx->nav.focus &&
                        window->name == ctx->nav.focus_window;

    if ((layout->flags & NK_WINDOW_BORDER) || navigated) {
      nk_bool         moving = ctx->nav.window_mode && ctx->nav.mode_window == window->name;
      struct nk_color border_color = moving      ? ctx->style.nav_active
                                     : navigated ? ctx->style.nav_highlight
                                                 : nk_panel_get_border_color(style, layout->type);
      const float     padding_y = (layout->flags & NK_WINDOW_MINIMIZED)
                                    ? (style->window.border + window->bounds.y + layout->header_height)
                                    : ((layout->flags & NK_WINDOW_DYNAMIC) ? (layout->bounds.y + layout->bounds.h + layout->footer_height) : (window->bounds.y + window->bounds.h));
      struct nk_rect  b = window->bounds;

      b.h = padding_y - window->bounds.y;
      nk_stroke_rect(out, b, style->window.rounding, navigated ? NK_MAX(layout->border, 2.0f) : layout->border, border_color);
    }
  }

  /* scaler */
  if ((layout->flags & NK_WINDOW_SCALABLE) && !(layout->flags & NK_WINDOW_MAXIMIZED) && in && !(layout->flags & NK_WINDOW_MINIMIZED)) {
    /* calculate scaler bounds */
    struct nk_rect scaler;

    scaler.w = scrollbar_size.x;
    scaler.h = scrollbar_size.y;
    scaler.y = layout->bounds.y + layout->bounds.h;

    if (layout->flags & NK_WINDOW_SCALE_LEFT) {
      scaler.x = layout->bounds.x - panel_padding.x * 0.5f;
    } else {
      scaler.x = layout->bounds.x + layout->bounds.w + panel_padding.x;

      if (layout->flags & NK_WINDOW_NO_SCROLLBAR_V) {
        scaler.x -= scaler.w;
      }
    }

    /* draw scaler */
    {
      const struct nk_style_item *item = &style->window.scaler;

      if (item->type == NK_STYLE_ITEM_IMAGE) {
        nk_draw_image(out, scaler, &item->data.image, nk_white);
      } else {
        if (layout->flags & NK_WINDOW_SCALE_LEFT) {
          nk_fill_triangle(out, scaler.x, scaler.y, scaler.x, scaler.y + scaler.h, scaler.x + scaler.w, scaler.y + scaler.h, item->data.color);
        } else {
          nk_fill_triangle(out, scaler.x + scaler.w, scaler.y, scaler.x + scaler.w, scaler.y + scaler.h, scaler.x, scaler.y + scaler.h, item->data.color);
        }
      }
    }

    /* do window scaling */
    if (!(window->flags & NK_WINDOW_ROM)) {
      struct nk_vec2 window_size                = style->window.min_size;
      int            left_mouse_down            = in->mouse.buttons[NK_BUTTON_LEFT].down;
      int            left_mouse_click_in_scaler = nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, scaler, nk_true);

      if (left_mouse_down && left_mouse_click_in_scaler) {
        float delta_x = in->mouse.delta.x;

        if (layout->flags & NK_WINDOW_SCALE_LEFT) {
          delta_x           = -delta_x;
          window->bounds.x += in->mouse.delta.x;
        }

        /* dragging in x-direction  */
        if (window->bounds.w + delta_x >= window_size.x) {
          if ((delta_x < 0) || (delta_x > 0 && in->mouse.pos.x >= scaler.x)) {
            window->bounds.w = window->bounds.w + delta_x;
            scaler.x        += in->mouse.delta.x;
          }
        }

        /* dragging in y-direction (only possible if static window) */
        if (!(layout->flags & NK_WINDOW_DYNAMIC)) {
          if (window_size.y < window->bounds.h + in->mouse.delta.y) {
            if ((in->mouse.delta.y < 0) || (in->mouse.delta.y > 0 && in->mouse.pos.y >= scaler.y)) {
              window->bounds.h = window->bounds.h + in->mouse.delta.y;
              scaler.y        += in->mouse.delta.y;
            }
          }
        }

        ctx->style.cursor_active                        = ctx->style.cursors[NK_CURSOR_RESIZE_TOP_RIGHT_DOWN_LEFT];
        in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x = scaler.x + scaler.w / 2.0f;
        in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.y = scaler.y + scaler.h / 2.0f;
      }
    }
  }

  if (!nk_panel_is_sub(layout->type)) {
    /* window is hidden so clear command buffer  */
    if (layout->flags & NK_WINDOW_HIDDEN) {
      nk_command_buffer_reset(&window->buffer);
    }
    /* window is visible and not tab */
    else {
      nk_finish(ctx, window);
    }
  }

  /* NK_WINDOW_REMOVE_ROM flag was set so remove NK_WINDOW_ROM */
  if (layout->flags & NK_WINDOW_REMOVE_ROM) {
    layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
    layout->flags &= ~(nk_flags)NK_WINDOW_REMOVE_ROM;
  }

  window->flags = layout->flags;

  /* property garbage collector */
  if (window->property.active && window->property.old != window->property.seq && window->property.active == window->property.prev) {
    nk_zero(&window->property, sizeof(window->property));
  } else {
    window->property.old  = window->property.seq;
    window->property.prev = window->property.active;
    window->property.seq  = 0;
  }

  window->tab_frame_last = window->tab_last;
  window->tab_last       = 0;

  if (!window->tab_target_seen && !window->tab_target_new) {
    window->tab_target = 0;
  }

  window->tab_target_seen = nk_false;
  window->tab_target_new  = nk_false;

  /* edit garbage collector */
  if (window->edit.active && window->edit.old != window->edit.seq && window->edit.active == window->edit.prev) {
    nk_zero(&window->edit, sizeof(window->edit));
  } else {
    window->edit.old  = window->edit.seq;
    window->edit.prev = window->edit.active;
    window->edit.seq  = 0;
  }

  /* contextual garbage collector */
  if (window->popup.active_con && window->popup.con_old != window->popup.con_count) {
    window->popup.con_count  = 0;
    window->popup.con_old    = 0;
    window->popup.active_con = 0;
  } else {
    window->popup.con_old   = window->popup.con_count;
    window->popup.con_count = 0;
  }

  window->popup.combo_count = 0;
  /* helper to make sure you have a 'nk_tree_push' for every 'nk_tree_pop' */
  NK_ASSERT(!layout->row.tree_depth);
}

/* ===============================================================
 *
 *                              WINDOW
 *
 * ===============================================================*/
NK_LIB void *
nk_create_window(struct nk_context *ctx)
{
  struct nk_page_element *elem;

  elem = nk_create_page_element(ctx);

  if (!elem) {
    return 0;
  }

  elem->data.win.seq = ctx->seq;
  return &elem->data.win;
}

NK_LIB void
nk_free_window(struct nk_context *ctx, struct nk_window *win)
{
  /* unlink windows from list */
  struct nk_table *it = win->tables;

  if (win->popup.win) {
    nk_free_window(ctx, win->popup.win);
    win->popup.win = 0;
  }

  win->next = 0;
  win->prev = 0;

  while (it) {
    /*free window state tables */
    struct nk_table *n = it->next;

    nk_remove_table(win, it);
    nk_free_table(ctx, it);

    if (it == win->tables) {
      win->tables = n;
    }

    it = n;
  }

  /* link windows into freelist */
  {
    union nk_page_data     *pd = NK_CONTAINER_OF(win, union nk_page_data, win);
    struct nk_page_element *pe = NK_CONTAINER_OF(pd, struct nk_page_element, data);

    nk_free_page_element(ctx, pe);
  }
}

NK_LIB struct nk_window *
nk_find_window(const struct nk_context *ctx, nk_hash hash, const char *name)
{
  struct nk_window *iter;

  iter = ctx->begin;

  while (iter) {
    NK_ASSERT(iter != iter->next);

    if (iter->name == hash) {
      int max_len = nk_strlen(iter->name_string);

      if (!nk_stricmpn(iter->name_string, name, max_len)) {
        return iter;
      }
    }

    iter = iter->next;
  }
  return 0;
}

NK_LIB void
nk_insert_window(struct nk_context *ctx, struct nk_window *win, enum nk_window_insert_location loc)
{
  const struct nk_window *iter;

  NK_ASSERT(ctx);
  NK_ASSERT(win);

  if (!win || !ctx) {
    return;
  }

  iter = ctx->begin;

  while (iter) {
    NK_ASSERT(iter != iter->next);
    NK_ASSERT(iter != win);

    if (iter == win) {
      return;
    }

    iter = iter->next;
  }

  if (!ctx->begin) {
    win->next  = 0;
    win->prev  = 0;
    ctx->begin = win;
    ctx->end   = win;
    ctx->count = 1;
    return;
  }

  if (loc == NK_INSERT_BACK) {
    struct nk_window *end;

    end              = ctx->end;
    end->flags      |= NK_WINDOW_ROM;
    end->next        = win;
    win->prev        = ctx->end;
    win->next        = 0;
    ctx->end         = win;
    ctx->active      = ctx->end;
    ctx->end->flags &= ~(nk_flags)NK_WINDOW_ROM;
  } else {
    /*ctx->end->flags |= NK_WINDOW_ROM;*/
    ctx->begin->prev   = win;
    win->next          = ctx->begin;
    win->prev          = 0;
    ctx->begin         = win;
    ctx->begin->flags &= ~(nk_flags)NK_WINDOW_ROM;
  }

  ctx->count++;
}

NK_LIB void
nk_remove_window(struct nk_context *ctx, struct nk_window *win)
{
  if (win == ctx->begin || win == ctx->end) {
    if (win == ctx->begin) {
      ctx->begin = win->next;

      if (win->next) {
        win->next->prev = 0;
      }
    }

    if (win == ctx->end) {
      ctx->end = win->prev;

      if (win->prev) {
        win->prev->next = 0;
      }
    }
  } else {
    if (win->next) {
      win->next->prev = win->prev;
    }

    if (win->prev) {
      win->prev->next = win->next;
    }
  }

  if (win == ctx->active || !ctx->active) {
    ctx->active = ctx->end;

    if (ctx->end) {
      ctx->end->flags &= ~(nk_flags)NK_WINDOW_ROM;
    }
  }

  win->next = 0;
  win->prev = 0;
  ctx->count--;
}

NK_API nk_bool
nk_begin(struct nk_context *ctx, const char *title, struct nk_rect bounds, nk_flags flags)
{
  return nk_begin_titled(ctx, title, title, bounds, flags);
}

NK_API nk_bool
nk_begin_titled(struct nk_context *ctx, const char *name, const char *title, struct nk_rect bounds, nk_flags flags)
{
  struct nk_window *win;
  struct nk_style  *style;
  nk_hash           name_hash;
  int               name_len;
  int               ret = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(name);
  NK_ASSERT(title);
  NK_ASSERT(ctx->style.font && "if this triggers you forgot to add a font");
  NK_ASSERT(!ctx->current && "if this triggers you missed a `nk_end` call");

  if (!ctx || ctx->current || !title || !name) {
    return 0;
  }

  /* find or create window */
  style     = &ctx->style;
  name_len  = (int)nk_strlen(name);
  name_hash = nk_murmur_hash(name, (int)name_len, NK_WINDOW_TITLE);
  win       = nk_find_window(ctx, name_hash, name);

  if (!win) {
    /* create new window */
    nk_size name_length = (nk_size)name_len;

    win = (struct nk_window *)nk_create_window(ctx);
    NK_ASSERT(win);

    if (!win) {
      return 0;
    }

    if (flags & NK_WINDOW_BACKGROUND) {
      nk_insert_window(ctx, win, NK_INSERT_FRONT);
    } else {
      nk_insert_window(ctx, win, NK_INSERT_BACK);
    }

    nk_command_buffer_init(&win->buffer, &ctx->memory, NK_CLIPPING_ON);

    win->flags  = flags;
    win->bounds = bounds;
    win->name   = name_hash;
    name_length = NK_MIN(name_length, NK_WINDOW_MAX_NAME - 1);
    NK_MEMCPY(win->name_string, name, name_length);
    win->name_string[name_length] = 0;
    win->popup.win                = 0;
    win->widgets_disabled         = nk_false;

    if (!ctx->active) {
      ctx->active = win;
    }
  } else {
    /* update window */
    win->flags &= ~(nk_flags)(NK_WINDOW_PRIVATE - 1);
    win->flags |= flags;

    if (!(win->flags & (NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE))) {
      win->bounds = bounds;
    }

    /* If this assert triggers you either:
     *
     * I.) Have more than one window with the same name or
     * II.) You forgot to actually draw the window.
     *      More specific you did not call `nk_clear` (nk_clear will be
     *      automatically called for you if you are using one of the
     *      provided demo backends). */
    NK_ASSERT(win->seq != ctx->seq);
    win->seq = ctx->seq;

    if (!ctx->active && !(win->flags & NK_WINDOW_HIDDEN)) {
      ctx->active = win;
      ctx->end    = win;
    }
  }

  if (win->flags & NK_WINDOW_HIDDEN) {
    ctx->current = win;
    win->layout  = 0;
    return 0;
  } else {
    nk_start(ctx, win);
  }

  if ((win->flags & NK_WINDOW_MAXIMIZED) && ctx->window_maximize_bounds.w > 0.0f && ctx->window_maximize_bounds.h > 0.0f) {
    win->bounds = ctx->window_maximize_bounds;
  }

  /* window overlapping */
  if (!(win->flags & NK_WINDOW_HIDDEN) && !(win->flags & NK_WINDOW_NO_INPUT)) {
    int               inpanel, ishovered, hover_free;
    struct nk_window *iter       = win;
    float             h          = style->font_size + 2.0f * style->window.header.padding.y + (2.0f * style->window.header.label_padding.y);
    struct nk_rect    win_bounds = (!(win->flags & NK_WINDOW_MINIMIZED)) ? win->bounds : nk_rect(win->bounds.x, win->bounds.y, win->bounds.w, h);

    /* activate window if hovered and no other window is overlapping this window */
    inpanel   = nk_input_has_mouse_click_down_in_rect(&ctx->input, NK_BUTTON_LEFT, win_bounds, nk_true);
    inpanel   = inpanel && ctx->input.mouse.buttons[NK_BUTTON_LEFT].clicked;
    ishovered = nk_input_is_mouse_hovering_rect(&ctx->input, win_bounds);

    if ((win != ctx->active) && ishovered && !ctx->input.mouse.buttons[NK_BUTTON_LEFT].down) {
      iter = win->next;

      while (iter) {
        struct nk_rect iter_bounds = (!(iter->flags & NK_WINDOW_MINIMIZED)) ? iter->bounds : nk_rect(iter->bounds.x, iter->bounds.y, iter->bounds.w, h);

        if (NK_INTERSECT(win_bounds.x, win_bounds.y, win_bounds.w, win_bounds.h, iter_bounds.x, iter_bounds.y, iter_bounds.w, iter_bounds.h) &&
            (!(iter->flags & NK_WINDOW_HIDDEN))) {
          break;
        }

        if (iter->popup.win && iter->popup.active && !(iter->flags & NK_WINDOW_HIDDEN) &&
            NK_INTERSECT(win->bounds.x,
                         win_bounds.y,
                         win_bounds.w,
                         win_bounds.h,
                         iter->popup.win->bounds.x,
                         iter->popup.win->bounds.y,
                         iter->popup.win->bounds.w,
                         iter->popup.win->bounds.h)) {
          break;
        }

        iter = iter->next;
      }
    }

    /* only a click focuses a window (like ImGui) */
    hover_free = ishovered && !iter;

    if (!inpanel) {
      iter = win;
    }

    /* activate window if clicked */
    if (iter && inpanel && (win != ctx->end)) {
      iter = win->next;

      while (iter) {
        /* try to find a panel with higher priority in the same position */
        struct nk_rect iter_bounds = (!(iter->flags & NK_WINDOW_MINIMIZED)) ? iter->bounds : nk_rect(iter->bounds.x, iter->bounds.y, iter->bounds.w, h);

        if (NK_INBOX(ctx->input.mouse.pos.x, ctx->input.mouse.pos.y, iter_bounds.x, iter_bounds.y, iter_bounds.w, iter_bounds.h) && !(iter->flags & NK_WINDOW_HIDDEN)) {
          break;
        }

        if (iter->popup.win && iter->popup.active && !(iter->flags & NK_WINDOW_HIDDEN) &&
            NK_INTERSECT(
              win_bounds.x, win_bounds.y, win_bounds.w, win_bounds.h, iter->popup.win->bounds.x, iter->popup.win->bounds.y, iter->popup.win->bounds.w, iter->popup.win->bounds.h)) {
          break;
        }

        iter = iter->next;
      }
    }

    if (iter && !(win->flags & NK_WINDOW_ROM) && (win->flags & NK_WINDOW_BACKGROUND)) {
      win->flags  |= (nk_flags)NK_WINDOW_ROM;
      iter->flags &= ~(nk_flags)NK_WINDOW_ROM;
      ctx->active  = iter;

      if (!(iter->flags & NK_WINDOW_BACKGROUND)) {
        /* current window is active in that position so transfer to top
         * at the highest priority in stack */
        nk_remove_window(ctx, iter);
        nk_insert_window(ctx, iter, NK_INSERT_BACK);
      }
    } else {
      if (!iter && ctx->end != win) {
        if (!(win->flags & NK_WINDOW_BACKGROUND)) {
          /* current window is active in that position so transfer to top
           * at the highest priority in stack */
          nk_remove_window(ctx, win);
          nk_insert_window(ctx, win, NK_INSERT_BACK);
        }

        win->flags &= ~(nk_flags)NK_WINDOW_ROM;
        ctx->active = win;
      }

      if (ctx->end != win && !(win->flags & NK_WINDOW_BACKGROUND)) {
        win->flags |= NK_WINDOW_ROM;
      }
    }

    if (hover_free && !ctx->nav.active) {
      win->flags &= ~(nk_flags)NK_WINDOW_ROM;
    }
  }

  win->layout               = (struct nk_panel *)nk_create_panel(ctx);
  ctx->current              = win;
  ret                       = nk_panel_begin(ctx, title, NK_PANEL_WINDOW);
  win->layout->offset_x     = &win->scrollbar.x;
  win->layout->offset_y     = &win->scrollbar.y;
  win->layout->prev_max_x   = &win->scrollbar.prev_max_x;
  win->layout->prev_max_y   = &win->scrollbar.prev_max_y;
  win->layout->scroll_flags = &win->scrollbar.flags;
  return ret;
}

NK_API void
nk_end(struct nk_context *ctx)
{
  struct nk_panel *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current && "if this triggers you forgot to call `nk_begin`");

  if (!ctx || !ctx->current) {
    return;
  }

  layout = ctx->current->layout;

  if (!layout || (layout->type == NK_PANEL_WINDOW && (ctx->current->flags & NK_WINDOW_HIDDEN))) {
    ctx->current = 0;
    return;
  }

  if (ctx->nav.window_menu && ctx->nav.window_menu == ctx->current->name && layout->type == NK_PANEL_WINDOW &&
      !(layout->flags & NK_WINDOW_MINIMIZED)) {
    nk_nav_window_menu(ctx, ctx->current);
  }

  if (ctx->nav.enabled && layout->type == NK_PANEL_WINDOW && !(layout->flags & NK_WINDOW_MINIMIZED)) {
    const struct nk_nav_stop *stops = ctx->nav.stops[ctx->nav.current];
    int                       i, n  = ctx->nav.count[ctx->nav.current];

    for (i = 0; i < n && stops[i].window != ctx->current->name; ++i) {
    }

    if (i == n && layout->clip.w > 0 && layout->clip.h > 0) {
      nk_nav_stop(ctx, layout->clip, NK_NAV_STOP_PANEL);
    }
  }

  nk_panel_end(ctx);
  nk_free_panel(ctx, ctx->current->layout);
  ctx->current = 0;
}

NK_API struct nk_rect
nk_window_get_bounds(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_rect(0, 0, 0, 0);
  }
  return ctx->current->bounds;
}

NK_API struct nk_vec2
nk_window_get_position(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }
  return nk_vec2(ctx->current->bounds.x, ctx->current->bounds.y);
}

NK_API struct nk_vec2
nk_window_get_size(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }
  return nk_vec2(ctx->current->bounds.w, ctx->current->bounds.h);
}

NK_API float
nk_window_get_width(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return 0;
  }
  return ctx->current->bounds.w;
}

NK_API float
nk_window_get_height(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return 0;
  }
  return ctx->current->bounds.h;
}

NK_API struct nk_rect
nk_window_get_content_region(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_rect(0, 0, 0, 0);
  }
  return ctx->current->layout->clip;
}

NK_API struct nk_vec2
nk_window_get_content_region_min(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }
  return nk_vec2(ctx->current->layout->clip.x, ctx->current->layout->clip.y);
}

NK_API struct nk_vec2
nk_window_get_content_region_max(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }
  return nk_vec2(ctx->current->layout->clip.x + ctx->current->layout->clip.w, ctx->current->layout->clip.y + ctx->current->layout->clip.h);
}

NK_API struct nk_vec2
nk_window_get_content_region_size(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }
  return nk_vec2(ctx->current->layout->clip.w, ctx->current->layout->clip.h);
}

NK_API struct nk_command_buffer *
nk_window_get_canvas(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current) {
    return 0;
  }
  return &ctx->current->buffer;
}

NK_API struct nk_panel *
nk_window_get_panel(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return 0;
  }
  return ctx->current->layout;
}

NK_API void
nk_window_get_scroll(const struct nk_context *ctx, nk_uint *offset_x, nk_uint *offset_y)
{
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win = ctx->current;

  if (offset_x) {
    *offset_x = win->scrollbar.x;
  }

  if (offset_y) {
    *offset_y = win->scrollbar.y;
  }
}

NK_API nk_bool
nk_window_has_focus(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current) {
    return 0;
  }
  return ctx->current == ctx->active;
}

NK_API nk_bool
nk_window_is_hovered(const struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current || (ctx->current->flags & NK_WINDOW_HIDDEN)) {
    return 0;
  } else {
    struct nk_rect actual_bounds = ctx->current->bounds;

    if (ctx->current->flags & NK_WINDOW_MINIMIZED) {
      actual_bounds.h = ctx->current->layout->header_height;
    }
    return nk_input_is_mouse_hovering_rect(&ctx->input, actual_bounds);
  }
}

NK_API nk_bool
nk_window_is_any_hovered(const struct nk_context *ctx)
{
  struct nk_window *iter;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  iter = ctx->begin;

  while (iter) {
    /* check if window is being hovered */
    if (!(iter->flags & NK_WINDOW_HIDDEN)) {
      /* check if window popup is being hovered */
      if (iter->popup.active && iter->popup.win && nk_input_is_mouse_hovering_rect(&ctx->input, iter->popup.win->bounds)) {
        return 1;
      }

      if (iter->flags & NK_WINDOW_MINIMIZED) {
        struct nk_rect header = iter->bounds;

        header.h = ctx->style.font_size + 2 * ctx->style.window.header.padding.y;

        if (nk_input_is_mouse_hovering_rect(&ctx->input, header)) {
          return 1;
        }
      } else if (nk_input_is_mouse_hovering_rect(&ctx->input, iter->bounds)) {
        return 1;
      }
    }

    iter = iter->next;
  }
  return 0;
}

NK_API nk_bool
nk_item_is_any_active(const struct nk_context *ctx)
{
  int any_hovered = nk_window_is_any_hovered(ctx);
  int any_active  = (ctx->last_widget_state & NK_WIDGET_STATE_MODIFIED);
  return any_hovered || any_active;
}

NK_API nk_bool
nk_window_is_collapsed(const struct nk_context *ctx, const char *name)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return 0;
  }
  return win->flags & NK_WINDOW_MINIMIZED;
}

NK_API nk_bool
nk_window_is_closed(const struct nk_context *ctx, const char *name)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 1;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return 1;
  }
  return (win->flags & NK_WINDOW_CLOSED);
}

NK_API nk_bool
nk_window_is_hidden(const struct nk_context *ctx, const char *name)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 1;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return 1;
  }
  return (win->flags & NK_WINDOW_HIDDEN);
}

NK_API nk_bool
nk_window_is_active(const struct nk_context *ctx, const char *name)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return 0;
  }
  return win == ctx->active;
}

NK_API struct nk_window *
nk_window_find(const struct nk_context *ctx, const char *name)
{
  int     title_len;
  nk_hash title_hash;

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  return nk_find_window(ctx, title_hash, name);
}

NK_API void
nk_window_set_maximize_bounds(struct nk_context *ctx, struct nk_rect bounds)
{
  if (ctx) {
    ctx->window_maximize_bounds = bounds;
  }
}

NK_API void
nk_window_close(struct nk_context *ctx, const char *name)
{
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  win = nk_window_find(ctx, name);

  if (!win) {
    return;
  }

  NK_ASSERT(ctx->current != win && "You cannot close a currently active window");

  if (ctx->current == win) {
    return;
  }

  win->flags |= NK_WINDOW_HIDDEN;
  win->flags |= NK_WINDOW_CLOSED;
}

NK_API void
nk_window_set_bounds(struct nk_context *ctx, const char *name, struct nk_rect bounds)
{
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  win = nk_window_find(ctx, name);

  if (!win) {
    return;
  }

  win->bounds = bounds;
}

NK_API void
nk_window_set_position(struct nk_context *ctx, const char *name, struct nk_vec2 pos)
{
  struct nk_window *win = nk_window_find(ctx, name);

  if (!win) {
    return;
  }

  win->bounds.x = pos.x;
  win->bounds.y = pos.y;
}

NK_API void
nk_window_set_size(struct nk_context *ctx, const char *name, struct nk_vec2 size)
{
  struct nk_window *win = nk_window_find(ctx, name);

  if (!win) {
    return;
  }

  win->bounds.w = size.x;
  win->bounds.h = size.y;
}

NK_API void
nk_window_set_scroll(struct nk_context *ctx, nk_uint offset_x, nk_uint offset_y)
{
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win              = ctx->current;
  win->scrollbar.x = offset_x;
  win->scrollbar.y = offset_y;
}

NK_API void
nk_window_collapse(struct nk_context *ctx, const char *name, enum nk_collapse_states c)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return;
  }

  if (c == NK_MINIMIZED) {
    win->flags |= NK_WINDOW_MINIMIZED;
  } else {
    win->flags &= ~(nk_flags)NK_WINDOW_MINIMIZED;
  }
}

NK_API void
nk_window_collapse_if(struct nk_context *ctx, const char *name, enum nk_collapse_states c, int cond)
{
  NK_ASSERT(ctx);

  if (!ctx || !cond) {
    return;
  }

  nk_window_collapse(ctx, name, c);
}

NK_API void
nk_window_show(struct nk_context *ctx, const char *name, enum nk_show_states s)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (!win) {
    return;
  }

  if (s == NK_HIDDEN) {
    win->flags |= NK_WINDOW_HIDDEN;
  } else {
    win->flags &= ~(nk_flags)NK_WINDOW_HIDDEN;
  }
}

NK_API void
nk_window_show_if(struct nk_context *ctx, const char *name, enum nk_show_states s, int cond)
{
  NK_ASSERT(ctx);

  if (!ctx || !cond) {
    return;
  }

  nk_window_show(ctx, name, s);
}

NK_API void
nk_window_set_focus(struct nk_context *ctx, const char *name)
{
  int               title_len;
  nk_hash           title_hash;
  struct nk_window *win;

  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  title_len  = (int)nk_strlen(name);
  title_hash = nk_murmur_hash(name, (int)title_len, NK_WINDOW_TITLE);
  win        = nk_find_window(ctx, title_hash, name);

  if (win && ctx->end != win) {
    nk_remove_window(ctx, win);
    nk_insert_window(ctx, win, NK_INSERT_BACK);
  }

  if (win) {
    ctx->nav.focus_request = win->name;
  }

  ctx->active = win;
}

NK_API void
nk_rule_horizontal(struct nk_context *ctx, struct nk_color color, nk_bool rounding)
{
  struct nk_rect               space;
  enum nk_widget_layout_states state  = nk_widget(&space, ctx);
  struct nk_command_buffer    *canvas = nk_window_get_canvas(ctx);

  if (!state) {
    return;
  }

  nk_fill_rect(canvas, space, rounding && space.h > 1.5f ? space.h / 2.0f : 0, color);
}

/* ===============================================================
 *
 *                              POPUP
 *
 * ===============================================================*/
NK_API nk_bool
nk_popup_begin(struct nk_context *ctx, enum nk_popup_type type, const char *title, nk_flags flags, struct nk_rect rect)
{
  struct nk_window *popup;
  struct nk_window *win;
  struct nk_panel  *panel;

  int     title_len;
  nk_hash title_hash;
  nk_size allocated;
  nk_bool fresh;

  NK_ASSERT(ctx);
  NK_ASSERT(title);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  panel = win->layout;
  NK_ASSERT(!((int)panel->type & (int)NK_PANEL_SET_POPUP) && "popups are not allowed to have popups");
  (void)panel;
  title_len  = (int)nk_strlen(title);
  title_hash = nk_murmur_hash(title, (int)title_len, NK_PANEL_POPUP);

  popup = win->popup.win;

  if (!popup) {
    popup = (struct nk_window *)nk_create_window(ctx);

    if (!popup) {
      return 0;
    }

    popup->parent     = win;
    win->popup.win    = popup;
    win->popup.active = 0;
    win->popup.type   = NK_PANEL_POPUP;
  }

  /* make sure we have correct popup */
  fresh = win->popup.name != title_hash || !win->popup.active;

  if (win->popup.name != title_hash) {
    if (!win->popup.active) {
      nk_zero(popup, sizeof(*popup));
      win->popup.name   = title_hash;
      win->popup.active = 1;
      win->popup.type   = NK_PANEL_POPUP;
    } else {
      return 0;
    }
  }

  /* popup position is local to window */
  ctx->current = popup;
  rect.x      += win->layout->clip.x;
  rect.y      += win->layout->clip.y;

  /* setup popup data */
  popup->parent = win;
  popup->bounds = rect;
  popup->seq    = ctx->seq;
  popup->layout = (struct nk_panel *)nk_create_panel(ctx);
  popup->flags  = flags;
  popup->flags |= NK_WINDOW_BORDER;

  if (type == NK_POPUP_DYNAMIC) {
    popup->flags |= NK_WINDOW_DYNAMIC;
  }

  if (!fresh && ctx->nav.cancel && !ctx->nav.editing_last) {
    popup->flags   |= NK_WINDOW_HIDDEN;
    ctx->nav.cancel = nk_false;
  }

  popup->buffer = win->buffer;
  nk_start_popup(ctx, win);
  allocated = ctx->memory.allocated;
  nk_push_scissor(&popup->buffer, nk_null_rect);

  if (nk_panel_begin(ctx, title, NK_PANEL_POPUP)) {
    /* popup is running therefore invalidate parent panels */
    struct nk_panel *root;

    root = win->layout;

    while (root) {
      root->flags |= NK_WINDOW_ROM;
      root->flags &= ~(nk_flags)NK_WINDOW_REMOVE_ROM;
      root         = root->parent;
    }

    win->popup.active           = 1;
    popup->layout->offset_x     = &popup->scrollbar.x;
    popup->layout->offset_y     = &popup->scrollbar.y;
    popup->layout->prev_max_x   = &popup->scrollbar.prev_max_x;
    popup->layout->prev_max_y   = &popup->scrollbar.prev_max_y;
    popup->layout->scroll_flags = &popup->scrollbar.flags;
    popup->layout->parent       = win->layout;
    return 1;
  } else {
    /* popup was closed/is invalid so cleanup */
    struct nk_panel *root;

    root = win->layout;

    while (root) {
      root->flags |= NK_WINDOW_REMOVE_ROM;
      root         = root->parent;
    }

    win->popup.buf.active = 0;
    win->popup.active     = 0;
    ctx->memory.allocated = allocated;
    ctx->current          = win;
    nk_free_panel(ctx, popup->layout);
    popup->layout = 0;
    return 0;
  }
}

NK_LIB nk_bool
nk_nonblock_begin(struct nk_context *ctx, nk_flags flags, struct nk_rect body, struct nk_rect header, enum nk_panel_type panel_type)
{
  struct nk_window *popup;
  struct nk_window *win;
  struct nk_panel  *panel;
  int               is_active = nk_true;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  /* popups cannot have popups */
  win   = ctx->current;
  panel = win->layout;
  NK_ASSERT(!((int)panel->type & (int)NK_PANEL_SET_POPUP));
  (void)panel;
  popup = win->popup.win;

  if (!popup) {
    /* create window for nonblocking popup */
    popup = (struct nk_window *)nk_create_window(ctx);

    if (!popup) {
      return 0;
    }

    popup->parent   = win;
    win->popup.win  = popup;
    win->popup.type = panel_type;
    nk_command_buffer_init(&popup->buffer, &ctx->memory, NK_CLIPPING_ON);
  } else {
    /* close the popup if user pressed outside or in the header */
    int pressed, in_body, in_header;
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
    pressed = nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT);
#else
    pressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
#endif
    in_body   = nk_input_is_mouse_hovering_rect(&ctx->input, body);
    in_header = nk_input_is_mouse_hovering_rect(&ctx->input, header);

    if ((pressed && (!in_body || in_header)) || (ctx->nav.cancel && !ctx->nav.editing_last)) {
      is_active       = nk_false;
      ctx->nav.cancel = nk_false;
    }
  }

  win->popup.header = header;

  if (!is_active) {
    /* remove read only mode from all parent panels */
    struct nk_panel *root = win->layout;

    while (root) {
      root->flags |= NK_WINDOW_REMOVE_ROM;
      root         = root->parent;
    }
    return is_active;
  }

  popup->bounds     = body;
  popup->parent     = win;
  popup->layout     = (struct nk_panel *)nk_create_panel(ctx);
  popup->flags      = flags;
  popup->flags     |= NK_WINDOW_BORDER;
  popup->flags     |= NK_WINDOW_DYNAMIC;
  popup->seq        = ctx->seq;
  win->popup.active = 1;
  NK_ASSERT(popup->layout);

  nk_start_popup(ctx, win);
  popup->buffer = win->buffer;
  nk_push_scissor(&popup->buffer, nk_null_rect);
  ctx->current = popup;

  nk_panel_begin(ctx, 0, panel_type);
  win->buffer                 = popup->buffer;
  popup->layout->parent       = win->layout;
  popup->layout->offset_x     = &popup->scrollbar.x;
  popup->layout->offset_y     = &popup->scrollbar.y;
  popup->layout->prev_max_x   = &popup->scrollbar.prev_max_x;
  popup->layout->prev_max_y   = &popup->scrollbar.prev_max_y;
  popup->layout->scroll_flags = &popup->scrollbar.flags;

  /* set read only mode to all parent panels */
  {
    struct nk_panel *root;

    root = win->layout;

    while (root) {
      root->flags |= NK_WINDOW_ROM;
      root         = root->parent;
    }
  }
  return is_active;
}

NK_API void
nk_popup_close(struct nk_context *ctx)
{
  struct nk_window *popup;

  NK_ASSERT(ctx);

  if (!ctx || !ctx->current) {
    return;
  }

  popup = ctx->current;
  NK_ASSERT(popup->parent);
  NK_ASSERT((int)popup->layout->type & (int)NK_PANEL_SET_POPUP);
  popup->flags |= NK_WINDOW_HIDDEN;
}

NK_API void
nk_popup_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_window *popup;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  popup = ctx->current;

  if (!popup->parent) {
    return;
  }

  win = popup->parent;

  if (popup->flags & NK_WINDOW_HIDDEN) {
    struct nk_panel *root;

    root = win->layout;

    while (root) {
      root->flags |= NK_WINDOW_REMOVE_ROM;
      root         = root->parent;
    }

    win->popup.active = 0;
  }

  nk_push_scissor(&popup->buffer, nk_null_rect);
  nk_end(ctx);

  win->buffer = popup->buffer;
  nk_finish_popup(ctx, win);
  ctx->current = win;
  nk_push_scissor(&win->buffer, win->layout->clip);
}

NK_API void
nk_popup_get_scroll(const struct nk_context *ctx, nk_uint *offset_x, nk_uint *offset_y)
{
  struct nk_window *popup;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  popup = ctx->current;

  if (offset_x) {
    *offset_x = popup->scrollbar.x;
  }

  if (offset_y) {
    *offset_y = popup->scrollbar.y;
  }
}

NK_API void
nk_popup_set_scroll(struct nk_context *ctx, nk_uint offset_x, nk_uint offset_y)
{
  struct nk_window *popup;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  popup              = ctx->current;
  popup->scrollbar.x = offset_x;
  popup->scrollbar.y = offset_y;
}

/* ==============================================================
 *
 *                          CONTEXTUAL
 *
 * ===============================================================*/
NK_API nk_bool
nk_contextual_begin(struct nk_context *ctx, nk_flags flags, struct nk_vec2 size, struct nk_rect trigger_bounds)
{
  struct nk_window *win;
  struct nk_window *popup;
  struct nk_rect    body;
  struct nk_input  *in;

  NK_STORAGE const struct nk_rect null_rect  = {-1, -1, 0, 0};
  int                             is_clicked = 0;
  int                             is_open    = 0;
  int                             ret        = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win = ctx->current;
  ++win->popup.con_count;

  if (ctx->current != ctx->active) {
    return 0;
  }

  /* check if currently active contextual is active */
  popup   = win->popup.win;
  is_open = (popup && win->popup.type == NK_PANEL_CONTEXTUAL);
  in      = win->widgets_disabled ? 0 : &ctx->input;

  if (in) {
    int right_pressed = nk_input_is_mouse_pressed(in, NK_BUTTON_RIGHT);

    is_clicked = nk_input_mouse_clicked(in, NK_BUTTON_RIGHT, trigger_bounds);

    /*
     * The currently active contextual was reached before the newly right-clicked target
     * Close it now so the later target can claim the popup during the same frame
     */
    if (is_open && win->popup.active_con == win->popup.con_count && right_pressed && !is_clicked) {
      struct nk_panel *root = win->layout;

      while (root) {
        root->flags &= ~(nk_flags)NK_WINDOW_ROM;
        root         = root->parent;
      }

      win->popup.active     = 0;
      win->popup.active_con = 0;
      win->popup.type       = NK_PANEL_NONE;

      popup->seq    = 0;
      popup->flags |= NK_WINDOW_HIDDEN;

      is_open = 0;
    }

    /*
     * Ignore other contextual targets unless this target was just right-clicked
     * A clicked target takes ownership of the popup
     */
    if (win->popup.active_con && win->popup.con_count != win->popup.active_con && !is_clicked) {
      return 0;
    }

    if (!is_open && !is_clicked) {
      return 0;
    }

    win->popup.active_con = win->popup.con_count;

    if (is_clicked) {
      body.x = in->mouse.pos.x;
      body.y = in->mouse.pos.y;
    } else {
      body.x = popup->bounds.x;
      body.y = popup->bounds.y;
    }

    body.w = size.x;
    body.h = size.y;

    {
      struct nk_rect client = ctx->window_maximize_bounds;

      if (client.w > 0.0f && client.h > 0.0f) {
        body.w = NK_MIN(body.w, client.w);
        body.h = NK_MIN(body.h, client.h);
        body.x = NK_CLAMP(client.x, body.x, client.x + client.w - body.w);
        body.y = NK_CLAMP(client.y, body.y, client.y + client.h - body.h);
      }
    }

    ret = nk_nonblock_begin(ctx, flags | NK_WINDOW_NO_SCROLLBAR, body, null_rect, NK_PANEL_CONTEXTUAL);

    if (ret) {
      win->popup.type = NK_PANEL_CONTEXTUAL;
    } else {
      win->popup.active_con = 0;
      win->popup.type       = NK_PANEL_NONE;

      if (win->popup.win) {
        win->popup.win->flags = 0;
      }
    }
  }
  return ret;
}

NK_API nk_bool
nk_contextual_item_text(struct nk_context *ctx, const char *text, int len, nk_flags alignment)
{
  struct nk_window      *win;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  state = nk_widget_fitting(&bounds, ctx, style->contextual_button.padding);

  if (!state) {
    return nk_false;
  }

  in = (state == NK_WIDGET_ROM || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text(&ctx->last_widget_state, &win->buffer, bounds, text, len, alignment, NK_BUTTON_DEFAULT, &style->contextual_button, in, style->font)) {
    nk_contextual_close(ctx);
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_contextual_item_label(struct nk_context *ctx, const char *label, nk_flags align)
{
  return nk_contextual_item_text(ctx, label, nk_strlen(label), align);
}

NK_API nk_bool
nk_contextual_item_image_text(struct nk_context *ctx, struct nk_image img, const char *text, int len, nk_flags align)
{
  struct nk_window      *win;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  state = nk_widget_fitting(&bounds, ctx, style->contextual_button.padding);

  if (!state) {
    return nk_false;
  }

  in = (state == NK_WIDGET_ROM || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text_image(&ctx->last_widget_state, &win->buffer, bounds, img, text, len, align, NK_BUTTON_DEFAULT, &style->contextual_button, style->font, in)) {
    nk_contextual_close(ctx);
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_contextual_item_image_label(struct nk_context *ctx, struct nk_image img, const char *label, nk_flags align)
{
  return nk_contextual_item_image_text(ctx, img, label, nk_strlen(label), align);
}

NK_API nk_bool
nk_contextual_item_symbol_text(struct nk_context *ctx, enum nk_symbol_type symbol, const char *text, int len, nk_flags align)
{
  struct nk_window      *win;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  state = nk_widget_fitting(&bounds, ctx, style->contextual_button.padding);

  if (!state) {
    return nk_false;
  }

  in = (state == NK_WIDGET_ROM || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text_symbol(&ctx->last_widget_state, &win->buffer, bounds, symbol, text, len, align, NK_BUTTON_DEFAULT, &style->contextual_button, style->font, in)) {
    nk_contextual_close(ctx);
    return nk_true;
  }
  return nk_false;
}

NK_API nk_bool
nk_contextual_item_symbol_label(struct nk_context *ctx, enum nk_symbol_type symbol, const char *text, nk_flags align)
{
  return nk_contextual_item_symbol_text(ctx, symbol, text, nk_strlen(text), align);
}

NK_API void
nk_contextual_close(struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  nk_popup_close(ctx);
}

NK_API void
nk_contextual_end(struct nk_context *ctx)
{
  struct nk_window *popup;
  struct nk_panel  *panel;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  popup = ctx->current;
  panel = popup->layout;
  NK_ASSERT(popup->parent);
  NK_ASSERT((int)panel->type & (int)NK_PANEL_SET_POPUP);

  if (panel->flags & NK_WINDOW_DYNAMIC) {
    /* Close behavior
    This is a bit of a hack solution since we do not know before we end our popup
    how big it will be. We therefore do not directly know when a
    click outside the non-blocking popup must close it at that direct frame.
    Instead it will be closed in the next frame.*/
    struct nk_rect body = {0, 0, 0, 0};

    if (panel->at_y < (panel->bounds.y + panel->bounds.h)) {
      struct nk_vec2 padding = nk_panel_get_padding(&ctx->style, panel->type);

      body   = panel->bounds;
      body.y = (panel->at_y + panel->footer_height + panel->border + padding.y + panel->row.height);
      body.h = (panel->bounds.y + panel->bounds.h) - body.y;
    }

    {
      int pressed = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT);
      int in_body = nk_input_is_mouse_hovering_rect(&ctx->input, body);

      if (pressed && in_body) {
        popup->flags |= NK_WINDOW_HIDDEN;
      }
    }
  }

  if (popup->flags & NK_WINDOW_HIDDEN) {
    popup->seq = 0;
  }

  nk_popup_end(ctx);
  return;
}

/* ===============================================================
 *
 *                              MENU
 *
 * ===============================================================*/
NK_API void
nk_menubar_begin(struct nk_context *ctx)
{
  struct nk_panel *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  layout = ctx->current->layout;
  NK_ASSERT(layout->at_y == layout->bounds.y);

  /* if this assert triggers you allocated space between nk_begin and nk_menubar_begin.
  If you want a menubar the first nuklear function after `nk_begin` has to be a
  `nk_menubar_begin` call. Inside the menubar you then have to allocate space for
  widgets (also supports multiple rows).
  Example:
      if (nk_begin(...)) {
          nk_menubar_begin(...);
              nk_layout_xxxx(...);
              nk_button(...);
              nk_layout_xxxx(...);
              nk_button(...);
          nk_menubar_end(...);
      }
      nk_end(...);
  */
  if (layout->flags & NK_WINDOW_HIDDEN || layout->flags & NK_WINDOW_MINIMIZED) {
    return;
  }

  layout->menu.x        = layout->at_x;
  layout->menu.y        = layout->at_y + layout->row.height;
  layout->menu.w        = layout->bounds.w;
  layout->menu.offset.x = *layout->offset_x;
  layout->menu.offset.y = *layout->offset_y;
  *layout->offset_y     = 0;
}

NK_API void
nk_menubar_end(struct nk_context *ctx)
{
  struct nk_window         *win;
  struct nk_panel          *layout;
  struct nk_command_buffer *out;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  out    = &win->buffer;
  layout = win->layout;

  if (layout->flags & NK_WINDOW_HIDDEN || layout->flags & NK_WINDOW_MINIMIZED) {
    return;
  }

  layout->menu.h  = layout->at_y - layout->menu.y;
  layout->menu.h += layout->row.height + ctx->style.window.spacing.y;

  layout->bounds.y += layout->menu.h;
  layout->bounds.h -= layout->menu.h;

  *layout->offset_x = layout->menu.offset.x;
  *layout->offset_y = layout->menu.offset.y;
  layout->at_y      = layout->bounds.y - layout->row.height;

  layout->clip.y = layout->bounds.y;
  layout->clip.h = layout->bounds.h;
  nk_push_scissor(out, layout->clip);
}

NK_INTERN int
nk_menu_begin(struct nk_context *ctx, struct nk_window *win, const char *id, int is_clicked, struct nk_rect header, struct nk_vec2 size)
{
  int               is_open   = 0;
  int               is_active = 0;
  struct nk_rect    body;
  struct nk_window *popup;
  nk_hash           hash = nk_murmur_hash(id, (int)nk_strlen(id), NK_PANEL_MENU);

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  body.x = header.x;
  body.w = size.x;
  body.y = header.y + header.h;
  body.h = size.y;

  popup     = win->popup.win;
  is_open   = popup ? nk_true : nk_false;
  is_active = (popup && (win->popup.name == hash) && win->popup.type == NK_PANEL_MENU);

  if ((is_clicked && is_open && !is_active) || (is_open && !is_active) || (!is_open && !is_active && !is_clicked)) {
    return 0;
  }

  if (!nk_nonblock_begin(ctx, NK_WINDOW_NO_SCROLLBAR, body, header, NK_PANEL_MENU)) {
    return 0;
  }

  win->popup.type = NK_PANEL_MENU;
  win->popup.name = hash;
  return 1;
}

NK_API nk_bool
nk_menu_begin_text(struct nk_context *ctx, const char *title, int len, nk_flags align, struct nk_vec2 size)
{
  struct nk_window      *win;
  const struct nk_input *in;
  struct nk_rect         header;
  int                    is_clicked = nk_false;
  nk_flags               state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  state = nk_widget_nav(&header, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || win->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text(&ctx->last_widget_state, &win->buffer, header, title, len, align, NK_BUTTON_DEFAULT, &ctx->style.menu_button, in, ctx->style.font)) {
    is_clicked = nk_true;
  }
  return nk_menu_begin(ctx, win, title, is_clicked, header, size);
}

NK_API nk_bool
nk_menu_begin_label(struct nk_context *ctx, const char *text, nk_flags align, struct nk_vec2 size)
{
  return nk_menu_begin_text(ctx, text, nk_strlen(text), align, size);
}

NK_API nk_bool
nk_menu_begin_image(struct nk_context *ctx, const char *id, struct nk_image img, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_rect         header;
  const struct nk_input *in;
  int                    is_clicked = nk_false;
  nk_flags               state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  state = nk_widget_nav(&header, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_image(&ctx->last_widget_state, &win->buffer, header, img, NK_BUTTON_DEFAULT, &ctx->style.menu_button, in)) {
    is_clicked = nk_true;
  }
  return nk_menu_begin(ctx, win, id, is_clicked, header, size);
}

NK_API nk_bool
nk_menu_begin_symbol(struct nk_context *ctx, const char *id, enum nk_symbol_type sym, struct nk_vec2 size)
{
  struct nk_window      *win;
  const struct nk_input *in;
  struct nk_rect         header;
  int                    is_clicked = nk_false;
  nk_flags               state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  state = nk_widget_nav(&header, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_symbol(&ctx->last_widget_state, &win->buffer, header, sym, NK_BUTTON_DEFAULT, &ctx->style.menu_button, in, ctx->style.font)) {
    is_clicked = nk_true;
  }
  return nk_menu_begin(ctx, win, id, is_clicked, header, size);
}

NK_API nk_bool
nk_menu_begin_image_text(struct nk_context *ctx, const char *title, int len, nk_flags align, struct nk_image img, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_rect         header;
  const struct nk_input *in;
  int                    is_clicked = nk_false;
  nk_flags               state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  state = nk_widget_nav(&header, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text_image(&ctx->last_widget_state, &win->buffer, header, img, title, len, align, NK_BUTTON_DEFAULT, &ctx->style.menu_button, ctx->style.font, in)) {
    is_clicked = nk_true;
  }
  return nk_menu_begin(ctx, win, title, is_clicked, header, size);
}

NK_API nk_bool
nk_menu_begin_image_label(struct nk_context *ctx, const char *title, nk_flags align, struct nk_image img, struct nk_vec2 size)
{
  return nk_menu_begin_image_text(ctx, title, nk_strlen(title), align, img, size);
}

NK_API nk_bool
nk_menu_begin_symbol_text(struct nk_context *ctx, const char *title, int len, nk_flags align, enum nk_symbol_type sym, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_rect         header;
  const struct nk_input *in;
  int                    is_clicked = nk_false;
  nk_flags               state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  state = nk_widget_nav(&header, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || win->layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  if (nk_do_button_text_symbol(&ctx->last_widget_state, &win->buffer, header, sym, title, len, align, NK_BUTTON_DEFAULT, &ctx->style.menu_button, ctx->style.font, in)) {
    is_clicked = nk_true;
  }
  return nk_menu_begin(ctx, win, title, is_clicked, header, size);
}

NK_API nk_bool
nk_menu_begin_symbol_label(struct nk_context *ctx, const char *title, nk_flags align, enum nk_symbol_type sym, struct nk_vec2 size)
{
  return nk_menu_begin_symbol_text(ctx, title, nk_strlen(title), align, sym, size);
}

NK_API nk_bool
nk_menu_item_text(struct nk_context *ctx, const char *title, int len, nk_flags align)
{
  return nk_contextual_item_text(ctx, title, len, align);
}

NK_API nk_bool
nk_menu_item_label(struct nk_context *ctx, const char *label, nk_flags align)
{
  return nk_contextual_item_label(ctx, label, align);
}

NK_API nk_bool
nk_menu_item_image_label(struct nk_context *ctx, struct nk_image img, const char *label, nk_flags align)
{
  return nk_contextual_item_image_label(ctx, img, label, align);
}

NK_API nk_bool
nk_menu_item_image_text(struct nk_context *ctx, struct nk_image img, const char *text, int len, nk_flags align)
{
  return nk_contextual_item_image_text(ctx, img, text, len, align);
}

NK_API nk_bool
nk_menu_item_symbol_text(struct nk_context *ctx, enum nk_symbol_type sym, const char *text, int len, nk_flags align)
{
  return nk_contextual_item_symbol_text(ctx, sym, text, len, align);
}

NK_API nk_bool
nk_menu_item_symbol_label(struct nk_context *ctx, enum nk_symbol_type sym, const char *label, nk_flags align)
{
  return nk_contextual_item_symbol_label(ctx, sym, label, align);
}

NK_API void
nk_menu_close(struct nk_context *ctx)
{
  nk_contextual_close(ctx);
}

NK_API void
nk_menu_end(struct nk_context *ctx)
{
  nk_contextual_end(ctx);
}

/* ===============================================================
 *
 *                          LAYOUT
 *
 * ===============================================================*/
NK_API void
nk_layout_set_min_row_height(struct nk_context *ctx, float height)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win                    = ctx->current;
  layout                 = win->layout;
  layout->row.min_height = height;
}

NK_API void
nk_layout_reset_min_row_height(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win                     = ctx->current;
  layout                  = win->layout;
  layout->row.min_height  = ctx->style.font_size;
  layout->row.min_height += ctx->style.text.padding.y * 2;
  layout->row.min_height += ctx->style.window.min_row_height_padding * 2;
}

NK_LIB float
nk_layout_row_calculate_usable_space(const struct nk_style *style, enum nk_panel_type type, float total_space, int columns)
{
  float panel_spacing;
  float panel_space;

  struct nk_vec2 spacing;

  NK_UNUSED(type);

  spacing = style->window.spacing;

  /* calculate the usable panel space */
  panel_spacing = (float)NK_MAX(columns - 1, 0) * spacing.x;
  panel_space   = total_space - panel_spacing;
  return panel_space;
}

NK_LIB void
nk_panel_layout(const struct nk_context *ctx, struct nk_window *win, float height, int cols)
{
  struct nk_panel          *layout;
  const struct nk_style    *style;
  struct nk_command_buffer *out;

  struct nk_vec2  item_spacing;
  struct nk_color color;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  /* prefetch some configuration data */
  layout       = win->layout;
  style        = &ctx->style;
  out          = &win->buffer;
  color        = style->window.background;
  item_spacing = style->window.spacing;

  /*  if one of these triggers you forgot to add an `if` condition around either
      a window, group, popup, combobox or contextual menu `begin` and `end` block.
      Example:
          if (nk_begin(...) {...} nk_end(...); or
          if (nk_group_begin(...) { nk_group_end(...);} */
  NK_ASSERT(!(layout->flags & NK_WINDOW_MINIMIZED));
  NK_ASSERT(!(layout->flags & NK_WINDOW_HIDDEN));
  NK_ASSERT(!(layout->flags & NK_WINDOW_CLOSED));

  /* update the current row and set the current row layout */
  layout->row.index   = 0;
  layout->at_y       += layout->row.height;
  layout->row.columns = cols;

  if (height == 0.0f) {
    layout->row.height = NK_MAX(height, layout->row.min_height) + item_spacing.y;
  } else {
    layout->row.height = height + item_spacing.y;
  }

  layout->row.item_offset = 0;

  if (layout->flags & NK_WINDOW_DYNAMIC) {
    /* draw background for dynamic panels */
    struct nk_rect background;

    background.x = win->bounds.x;
    background.w = win->bounds.w;
    background.y = layout->at_y - 1.0f;
    background.h = layout->row.height + 1.0f;
    nk_fill_rect(out, background, 0, color);
  }
}

NK_LIB void
nk_row_layout(struct nk_context *ctx, enum nk_layout_format fmt, float height, int cols, int width)
{
  /* update the current row and set the current row layout */
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win = ctx->current;
  nk_panel_layout(ctx, win, height, cols);

  if (fmt == NK_DYNAMIC) {
    win->layout->row.type = NK_LAYOUT_DYNAMIC_FIXED;
  } else {
    win->layout->row.type = NK_LAYOUT_STATIC_FIXED;
  }

  win->layout->row.ratio       = 0;
  win->layout->row.filled      = 0;
  win->layout->row.item_offset = 0;
  win->layout->row.item_width  = (float)width;
}

NK_API float
nk_layout_ratio_from_pixel(const struct nk_context *ctx, float pixel_width)
{
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(pixel_width);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win = ctx->current;
  return NK_CLAMP(0.0f, pixel_width / win->bounds.x, 1.0f);
}

NK_API void
nk_layout_row_dynamic(struct nk_context *ctx, float height, int cols)
{
  nk_row_layout(ctx, NK_DYNAMIC, height, cols, 0);
}

NK_API void
nk_layout_row_static(struct nk_context *ctx, float height, int item_width, int cols)
{
  nk_row_layout(ctx, NK_STATIC, height, cols, item_width);
}

NK_API void
nk_layout_row_begin(struct nk_context *ctx, enum nk_layout_format fmt, float row_height, int cols)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  nk_panel_layout(ctx, win, row_height, cols);

  if (fmt == NK_DYNAMIC) {
    layout->row.type = NK_LAYOUT_DYNAMIC_ROW;
  } else {
    layout->row.type = NK_LAYOUT_STATIC_ROW;
  }

  layout->row.ratio       = 0;
  layout->row.filled      = 0;
  layout->row.item_width  = 0;
  layout->row.item_offset = 0;
  layout->row.columns     = cols;
}

NK_API void
nk_layout_row_push(struct nk_context *ctx, float ratio_or_width)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_STATIC_ROW || layout->row.type == NK_LAYOUT_DYNAMIC_ROW);

  if (layout->row.type != NK_LAYOUT_STATIC_ROW && layout->row.type != NK_LAYOUT_DYNAMIC_ROW) {
    return;
  }

  if (layout->row.type == NK_LAYOUT_DYNAMIC_ROW) {
    float ratio = ratio_or_width;

    if ((ratio + layout->row.filled) > 1.0f) {
      return;
    }

    if (ratio > 0.0f) {
      layout->row.item_width = NK_SATURATE(ratio);
    } else {
      layout->row.item_width = 1.0f - layout->row.filled;
    }
  } else {
    layout->row.item_width = ratio_or_width;
  }
}

NK_API void
nk_layout_row_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_STATIC_ROW || layout->row.type == NK_LAYOUT_DYNAMIC_ROW);

  if (layout->row.type != NK_LAYOUT_STATIC_ROW && layout->row.type != NK_LAYOUT_DYNAMIC_ROW) {
    return;
  }

  layout->row.item_width  = 0;
  layout->row.item_offset = 0;
}

NK_API void
nk_layout_row(struct nk_context *ctx, enum nk_layout_format fmt, float height, int cols, const float *ratio)
{
  int               i;
  int               n_undef = 0;
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  nk_panel_layout(ctx, win, height, cols);

  if (fmt == NK_DYNAMIC) {
    /* calculate width of undefined widget ratios */
    float r = 0;

    layout->row.ratio = ratio;

    for (i = 0; i < cols; ++i) {
      if (ratio[i] < 0.0f) {
        n_undef++;
      } else {
        r += ratio[i];
      }
    }

    r                      = NK_SATURATE(1.0f - r);
    layout->row.type       = NK_LAYOUT_DYNAMIC;
    layout->row.item_width = (r > 0 && n_undef > 0) ? (r / (float)n_undef) : 0;
  } else {
    layout->row.ratio       = ratio;
    layout->row.type        = NK_LAYOUT_STATIC;
    layout->row.item_width  = 0;
    layout->row.item_offset = 0;
  }

  layout->row.item_offset = 0;
  layout->row.filled      = 0;
}

NK_API void
nk_layout_row_template_begin(struct nk_context *ctx, float height)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  nk_panel_layout(ctx, win, height, 1);
  layout->row.type        = NK_LAYOUT_TEMPLATE;
  layout->row.columns     = 0;
  layout->row.ratio       = 0;
  layout->row.item_width  = 0;
  layout->row.item_height = 0;
  layout->row.item_offset = 0;
  layout->row.filled      = 0;
  layout->row.item.x      = 0;
  layout->row.item.y      = 0;
  layout->row.item.w      = 0;
  layout->row.item.h      = 0;
}

NK_API void
nk_layout_row_template_push_dynamic(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_TEMPLATE);
  NK_ASSERT(layout->row.columns < NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS);

  if (layout->row.type != NK_LAYOUT_TEMPLATE) {
    return;
  }

  if (layout->row.columns >= NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS) {
    return;
  }

  layout->row.templates[layout->row.columns++] = -1.0f;
}

NK_API void
nk_layout_row_template_push_variable(struct nk_context *ctx, float min_width)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_TEMPLATE);
  NK_ASSERT(layout->row.columns < NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS);

  if (layout->row.type != NK_LAYOUT_TEMPLATE) {
    return;
  }

  if (layout->row.columns >= NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS) {
    return;
  }

  layout->row.templates[layout->row.columns++] = -min_width;
}

NK_API void
nk_layout_row_template_push_static(struct nk_context *ctx, float width)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_TEMPLATE);
  NK_ASSERT(layout->row.columns < NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS);

  if (layout->row.type != NK_LAYOUT_TEMPLATE) {
    return;
  }

  if (layout->row.columns >= NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS) {
    return;
  }

  layout->row.templates[layout->row.columns++] = width;
}

NK_API void
nk_layout_row_template_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  int   i                  = 0;
  int   variable_count     = 0;
  int   min_variable_count = 0;
  float min_fixed_width    = 0.0f;
  float total_fixed_width  = 0.0f;
  float max_variable_width = 0.0f;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  NK_ASSERT(layout->row.type == NK_LAYOUT_TEMPLATE);

  if (layout->row.type != NK_LAYOUT_TEMPLATE) {
    return;
  }

  for (i = 0; i < layout->row.columns; ++i) {
    float width = layout->row.templates[i];

    if (width >= 0.0f) {
      total_fixed_width += width;
      min_fixed_width   += width;
    } else if (width < -1.0f) {
      width              = -width;
      total_fixed_width += width;
      max_variable_width = NK_MAX(max_variable_width, width);
      variable_count++;
    } else {
      min_variable_count++;
      variable_count++;
    }
  }

  if (variable_count) {
    float space        = nk_layout_row_calculate_usable_space(&ctx->style, layout->type, layout->bounds.w, layout->row.columns);
    float var_width    = (NK_MAX(space - min_fixed_width, 0.0f)) / (float)variable_count;
    int   enough_space = var_width >= max_variable_width;

    if (!enough_space) {
      var_width = (NK_MAX(space - total_fixed_width, 0)) / (float)min_variable_count;
    }

    for (i = 0; i < layout->row.columns; ++i) {
      float *width = &layout->row.templates[i];

      *width = (*width >= 0.0f) ? *width : (*width < -1.0f && !enough_space) ? -(*width) : var_width;
    }
  }
}

NK_API void
nk_layout_space_begin(struct nk_context *ctx, enum nk_layout_format fmt, float height, int widget_count)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  nk_panel_layout(ctx, win, height, widget_count);

  if (fmt == NK_STATIC) {
    layout->row.type = NK_LAYOUT_STATIC_FREE;
  } else {
    layout->row.type = NK_LAYOUT_DYNAMIC_FREE;
  }

  layout->row.ratio       = 0;
  layout->row.filled      = 0;
  layout->row.item_width  = 0;
  layout->row.item_offset = 0;
}

NK_API void
nk_layout_space_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win                     = ctx->current;
  layout                  = win->layout;
  layout->row.item_width  = 0;
  layout->row.item_height = 0;
  layout->row.item_offset = 0;
  nk_zero(&layout->row.item, sizeof(layout->row.item));
}

NK_API void
nk_layout_space_push(struct nk_context *ctx, struct nk_rect rect)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win              = ctx->current;
  layout           = win->layout;
  layout->row.item = rect;
}

NK_API struct nk_rect
nk_layout_space_bounds(const struct nk_context *ctx)
{
  struct nk_rect    ret;
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x = layout->clip.x;
  ret.y = layout->clip.y;
  ret.w = layout->clip.w;
  ret.h = layout->row.height;
  return ret;
}

NK_API struct nk_rect
nk_layout_widget_bounds(const struct nk_context *ctx)
{
  struct nk_rect    ret;
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x = layout->at_x;
  ret.y = layout->at_y;
  ret.w = layout->bounds.w - NK_MAX(layout->at_x - layout->bounds.x, 0);
  ret.h = layout->row.height;
  return ret;
}

NK_API struct nk_vec2
nk_layout_space_to_screen(const struct nk_context *ctx, struct nk_vec2 ret)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x += layout->at_x - (float)*layout->offset_x;
  ret.y += layout->at_y - (float)*layout->offset_y;
  return ret;
}

NK_API struct nk_vec2
nk_layout_space_to_local(const struct nk_context *ctx, struct nk_vec2 ret)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x += -layout->at_x + (float)*layout->offset_x;
  ret.y += -layout->at_y + (float)*layout->offset_y;
  return ret;
}

NK_API struct nk_rect
nk_layout_space_rect_to_screen(const struct nk_context *ctx, struct nk_rect ret)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x += layout->at_x - (float)*layout->offset_x;
  ret.y += layout->at_y - (float)*layout->offset_y;
  return ret;
}

NK_API struct nk_rect
nk_layout_space_rect_to_local(const struct nk_context *ctx, struct nk_rect ret)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  win    = ctx->current;
  layout = win->layout;

  ret.x += -layout->at_x + (float)*layout->offset_x;
  ret.y += -layout->at_y + (float)*layout->offset_y;
  return ret;
}

NK_API void
nk_grid_state_reset(struct nk_grid_state *state)
{
  NK_ASSERT(state);
  NK_MEMSET(state, 0, sizeof(*state));
}

NK_INTERN float
nk_grid_column_width(const struct nk_grid_column *column, float width)
{
  width = NK_MAX(width, column->min_width);

  if (column->max_width > 0.0f) {
    width = NK_MIN(width, column->max_width);
  }
  return width;
}

NK_INTERN void
nk_grid_solve(struct nk_grid *grid)
{
  float used_width;
  float remaining;
  int   column;
  int   pass;

  used_width = grid->padding_x * 2.0f + grid->column_gap * (float)NK_MAX(0, grid->column_count - 1);

  for (column = 0; column < grid->column_count; ++column) {
    const struct nk_grid_column *config = &grid->columns[column];
    float                        width;

    if (config->sizing == NK_GRID_COLUMN_FIXED) {
      width = config->width;
    } else if (config->sizing == NK_GRID_COLUMN_CONTENT) {
      width = grid->state->content_widths[column];
    } else {
      width = config->min_width;
    }

    grid->widths[column] = nk_grid_column_width(config, width);
    used_width          += grid->widths[column];
  }

  remaining = NK_MAX(0.0f, grid->available_width - used_width);

  for (pass = 0; pass < grid->column_count && remaining > 0.0f; ++pass) {
    float weight_sum  = 0.0f;
    float distributed = 0.0f;

    for (column = 0; column < grid->column_count; ++column) {
      const struct nk_grid_column *config = &grid->columns[column];

      if (config->sizing == NK_GRID_COLUMN_FLEX && (config->max_width <= 0.0f || grid->widths[column] < config->max_width)) {
        weight_sum += config->width > 0.0f ? config->width : 1.0f;
      }
    }

    if (weight_sum == 0.0f) {
      break;
    }

    for (column = 0; column < grid->column_count; ++column) {
      const struct nk_grid_column *config = &grid->columns[column];
      float                        weight;
      float                        add;

      if (config->sizing != NK_GRID_COLUMN_FLEX || (config->max_width > 0.0f && grid->widths[column] >= config->max_width)) {
        continue;
      }

      weight = config->width > 0.0f ? config->width : 1.0f;
      add    = remaining * weight / weight_sum;

      if (config->max_width > 0.0f) {
        add = NK_MIN(add, config->max_width - grid->widths[column]);
      }

      grid->widths[column] += add;
      distributed          += add;
    }

    if (distributed == 0.0f) {
      break;
    }

    remaining -= distributed;
  }

  used_width = grid->padding_x * 2.0f;

  for (column = 0; column < grid->column_count; ++column) {
    grid->offsets[column] = used_width;
    used_width           += grid->widths[column] + grid->column_gap;
  }

  if (grid->column_count) {
    used_width -= grid->column_gap;
  }

  grid->row_width = NK_MAX(grid->available_width, used_width);
}

NK_API void
nk_grid_begin(struct nk_context *ctx, struct nk_grid *grid, struct nk_grid_state *state, const struct nk_grid_column *columns, int column_count, const struct nk_grid_options *options)
{
  NK_ASSERT(ctx);
  NK_ASSERT(grid);
  NK_ASSERT(state);
  NK_ASSERT(columns);
  NK_ASSERT(options);
  NK_ASSERT(column_count > 0 && column_count <= NK_GRID_MAX_COLUMNS);

  if (state->column_count != column_count || state->generation != options->generation || state->font != ctx->style.font || state->font_size != ctx->style.font_size) {
    nk_grid_state_reset(state);
    state->column_count = column_count;
    state->generation   = options->generation;
    state->font         = ctx->style.font;
    state->font_size    = ctx->style.font_size;
  }

  NK_MEMSET(grid, 0, sizeof(*grid));
  grid->ctx             = ctx;
  grid->state           = state;
  grid->columns         = columns;
  grid->column_count    = column_count;
  grid->row_height      = options->row_height;
  grid->column_gap      = options->column_gap;
  grid->padding_x       = options->padding_x;
  grid->available_width = NK_MAX(0.0f, nk_layout_widget_bounds(ctx).w);
  nk_grid_solve(grid);
}

NK_API void
nk_grid_end(struct nk_grid *grid)
{
  NK_ASSERT(grid);
  NK_ASSERT(!grid->row_active);
  grid->ctx = NULL;
}

NK_API nk_bool
nk_grid_row_begin(struct nk_grid *grid)
{
  struct nk_rect row;
  struct nk_rect clip;

  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx);
  NK_ASSERT(!grid->row_active);

  nk_layout_space_begin(grid->ctx, NK_STATIC, grid->row_height, NK_SINT_MAX);
  grid->row_active = nk_true;
  nk_layout_space_push(grid->ctx, nk_rect(0.0f, 0.0f, grid->row_width, grid->row_height));
  nk_spacer(grid->ctx);

  row               = nk_layout_space_rect_to_screen(grid->ctx, nk_rect(0.0f, 0.0f, grid->row_width, grid->row_height));
  clip              = grid->ctx->current->layout->clip;
  grid->row_visible = NK_INTERSECT(row.x, row.y, row.w, row.h, clip.x, clip.y, clip.w, clip.h);
  return grid->row_visible;
}

NK_API void
nk_grid_row_end(struct nk_grid *grid)
{
  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx);
  NK_ASSERT(grid->row_active);
  nk_layout_space_end(grid->ctx);
  grid->row_active  = nk_false;
  grid->row_visible = nk_false;
}

NK_API void
nk_grid_push_row(struct nk_grid *grid)
{
  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx && grid->row_active);
  nk_layout_space_push(grid->ctx, nk_rect(0.0f, 0.0f, grid->row_width, grid->row_height));
}

NK_API struct nk_rect
nk_grid_cell_bounds(struct nk_grid *grid, int column, struct nk_vec2 intrinsic_size)
{
  struct nk_rect bounds;

  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx && grid->row_active);
  NK_ASSERT(column >= 0 && column < grid->column_count);

  if (grid->row_visible && grid->columns[column].sizing == NK_GRID_COLUMN_CONTENT) {
    float width = nk_grid_column_width(&grid->columns[column], intrinsic_size.x);

    grid->state->content_widths[column] = NK_MAX(grid->state->content_widths[column], width);
  }

  bounds = nk_rect(grid->offsets[column], 0.0f, grid->widths[column], grid->row_height);
  return nk_layout_space_rect_to_screen(grid->ctx, bounds);
}

NK_API void
nk_grid_push(struct nk_grid *grid, int column, struct nk_vec2 intrinsic_size)
{
  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx && grid->row_active);
  NK_ASSERT(column >= 0 && column < grid->column_count);

  nk_grid_cell_bounds(grid, column, intrinsic_size);
  nk_layout_space_push(grid->ctx, nk_rect(grid->offsets[column], 0.0f, grid->widths[column], grid->row_height));
}

NK_INTERN nk_bool
nk_text_selection_widget_clipped(struct nk_context *ctx, const char *text, int len,
                                 const struct nk_text_options *options, struct nk_rect bounds);

NK_API nk_bool
nk_grid_text(struct nk_grid *grid, int column, const char *text, int len, const struct nk_text_options *options)
{
  struct nk_rect bounds;
  float width;

  NK_ASSERT(grid);
  NK_ASSERT(grid->ctx);
  NK_ASSERT(text || len == 0);
  NK_ASSERT(options);

  if (!text) {
    text = "";
  }

  if (!grid->row_visible) {
    bounds = nk_grid_cell_bounds(grid, column, nk_vec2(0.0f, grid->ctx->style.font_size));
    return nk_text_selection_widget_clipped(grid->ctx, text, len, options, bounds);
  }

  width = len > 0 ? nk_text_width(grid->ctx, grid->ctx->style.font, grid->ctx->style.font_size, text, len) : 0.0f;
  nk_grid_push(grid, column, nk_vec2(width, grid->ctx->style.font_size));
  return nk_text_ex(grid->ctx, text, len, options);
}

NK_LIB void
nk_panel_alloc_row(const struct nk_context *ctx, struct nk_window *win)
{
  struct nk_panel *layout     = win->layout;
  struct nk_vec2   spacing    = ctx->style.window.spacing;
  const float      row_height = layout->row.height - spacing.y;

  nk_panel_layout(ctx, win, row_height, layout->row.columns);
}

NK_LIB void
nk_layout_widget_space(struct nk_rect *bounds, const struct nk_context *ctx, struct nk_window *win, int modify)
{
  struct nk_panel       *layout;
  const struct nk_style *style;

  struct nk_vec2 spacing;

  float item_offset  = 0;
  float item_width   = 0;
  float item_spacing = 0;
  float panel_space  = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  style  = &ctx->style;
  NK_ASSERT(bounds);

  spacing     = style->window.spacing;
  panel_space = nk_layout_row_calculate_usable_space(&ctx->style, layout->type, layout->bounds.w, layout->row.columns);

#define NK_FRAC(x) (x - (float)(int)nk_roundf(x)) /* will be used to remove fookin gaps */
  /* calculate the width of one item inside the current layout space */
  switch (layout->row.type) {
    case NK_LAYOUT_DYNAMIC_FIXED: {
      /* scaling fixed size widgets item width */
      float w = NK_MAX(1.0f, panel_space) / (float)layout->row.columns;

      item_offset  = (float)layout->row.index * w;
      item_width   = w + NK_FRAC(item_offset);
      item_spacing = (float)layout->row.index * spacing.x;
    } break;
    case NK_LAYOUT_DYNAMIC_ROW: {
      /* scaling single ratio widget width */
      float w = layout->row.item_width * panel_space;

      item_offset  = layout->row.item_offset;
      item_width   = w + NK_FRAC(item_offset);
      item_spacing = 0;

      if (modify) {
        layout->row.item_offset += w + spacing.x;
        layout->row.filled      += layout->row.item_width;
        layout->row.index        = 0;
      }
    } break;
    case NK_LAYOUT_DYNAMIC_FREE: {
      /* panel width depended free widget placing */
      bounds->x  = layout->at_x + (layout->bounds.w * layout->row.item.x);
      bounds->x -= (float)*layout->offset_x;
      bounds->y  = layout->at_y + (layout->row.height * layout->row.item.y);
      bounds->y -= (float)*layout->offset_y;
      bounds->w  = layout->bounds.w * layout->row.item.w + NK_FRAC(bounds->x);
      bounds->h  = layout->row.height * layout->row.item.h + NK_FRAC(bounds->y);
      return;
    }
    case NK_LAYOUT_DYNAMIC: {
      /* scaling arrays of panel width ratios for every widget */
      float ratio, w;

      NK_ASSERT(layout->row.ratio);
      ratio = (layout->row.ratio[layout->row.index] < 0) ? layout->row.item_width : layout->row.ratio[layout->row.index];

      w            = (ratio * panel_space);
      item_spacing = (float)layout->row.index * spacing.x;
      item_offset  = layout->row.item_offset;
      item_width   = w + NK_FRAC(item_offset);

      if (modify) {
        layout->row.item_offset += w;
        layout->row.filled      += ratio;
      }
    } break;
    case NK_LAYOUT_STATIC_FIXED: {
      /* non-scaling fixed widgets item width */
      item_width   = layout->row.item_width;
      item_offset  = (float)layout->row.index * item_width;
      item_spacing = (float)layout->row.index * spacing.x;
    } break;
    case NK_LAYOUT_STATIC_ROW: {
      /* scaling single ratio widget width */
      item_width   = layout->row.item_width;
      item_offset  = layout->row.item_offset;
      item_spacing = (float)layout->row.index * spacing.x;

      if (modify) {
        layout->row.item_offset += item_width;
      }
    } break;
    case NK_LAYOUT_STATIC_FREE: {
      /* free widget placing */
      bounds->x = layout->at_x + layout->row.item.x;
      bounds->w = layout->row.item.w;

      if (((bounds->x + bounds->w) > layout->max_x) && modify) {
        layout->max_x = (bounds->x + bounds->w);
      }

      bounds->x -= (float)*layout->offset_x;
      bounds->y  = layout->at_y + layout->row.item.y;
      bounds->y -= (float)*layout->offset_y;
      bounds->h  = layout->row.item.h;
      return;
    }
    case NK_LAYOUT_STATIC: {
      /* non-scaling array of panel pixel width for every widget */
      item_spacing = (float)layout->row.index * spacing.x;
      item_width   = layout->row.ratio[layout->row.index];
      item_offset  = layout->row.item_offset;

      if (modify) {
        layout->row.item_offset += item_width;
      }
    } break;
    case NK_LAYOUT_TEMPLATE: {
      /* stretchy row layout with combined dynamic/static widget width*/
      float w;

      NK_ASSERT(layout->row.index < layout->row.columns);
      NK_ASSERT(layout->row.index < NK_MAX_LAYOUT_ROW_TEMPLATE_COLUMNS);
      w            = layout->row.templates[layout->row.index];
      item_offset  = layout->row.item_offset;
      item_width   = w + NK_FRAC(item_offset);
      item_spacing = (float)layout->row.index * spacing.x;

      if (modify) {
        layout->row.item_offset += w;
      }
    } break;
#undef NK_FRAC
    default:
      NK_ASSERT(0);
      break;
  };

  /* set the bounds of the newly allocated widget */
  bounds->w = item_width;
  bounds->h = layout->row.height - spacing.y;
  bounds->y = layout->at_y - (float)*layout->offset_y;
  bounds->x = layout->at_x + item_offset + item_spacing;

  if (((bounds->x + bounds->w) > layout->max_x) && modify) {
    layout->max_x = bounds->x + bounds->w;
  }

  bounds->x -= (float)*layout->offset_x;
}

NK_LIB void
nk_panel_alloc_space(struct nk_rect *bounds, const struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  /* check if the end of the row has been hit and begin new row if so */
  win    = ctx->current;
  layout = win->layout;

  if (layout->row.index >= layout->row.columns) {
    nk_panel_alloc_row(ctx, win);
  }

  /* calculate widget position and size */
  nk_layout_widget_space(bounds, ctx, win, nk_true);
  layout->row.index++;
}

NK_LIB void
nk_layout_peek(struct nk_rect *bounds, const struct nk_context *ctx)
{
  float             y;
  int               index;
  struct nk_window *win;
  struct nk_panel  *layout;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    *bounds = nk_rect(0, 0, 0, 0);
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  y      = layout->at_y;
  index  = layout->row.index;

  if (layout->row.index >= layout->row.columns) {
    layout->at_y     += layout->row.height;
    layout->row.index = 0;
  }

  nk_layout_widget_space(bounds, ctx, win, nk_false);

  if (!layout->row.index) {
    bounds->x -= layout->row.item_offset;
  }

  layout->at_y      = y;
  layout->row.index = index;
}

NK_API void
nk_spacer(struct nk_context *ctx)
{
  struct nk_rect dummy_rect = {0, 0, 0, 0};

  nk_panel_alloc_space(&dummy_rect, ctx);
}

NK_API float
nk_layout_get_remaining_height(const struct nk_context *ctx)
{
  struct nk_panel *p     = ctx->current->layout;
  float            pad_y = nk_panel_get_padding(&ctx->style, p->type).y;
  float            gap_y = ctx->style.window.spacing.y;

  float used     = (p->at_y - p->bounds.y) + p->row.height; // includes top pad + previous rows + previous gaps
  float remain_h = p->bounds.h - used - pad_y - gap_y;

  if (remain_h < 0) {
    remain_h = 0;
  }

  return remain_h;
}

/* ===============================================================
 *
 *                              TREE
 *
 * ===============================================================*/
NK_INTERN int
nk_tree_state_base(struct nk_context *ctx, enum nk_tree_type type, struct nk_image *img, const char *title, int title_len, enum nk_collapse_states *state)
{
  struct nk_window             *win;
  struct nk_panel              *layout;
  const struct nk_style        *style;
  struct nk_command_buffer     *out;
  const struct nk_input        *in;
  const struct nk_style_button *button;
  enum nk_symbol_type           symbol;
  float                         row_height;

  struct nk_vec2 item_spacing;
  struct nk_rect header = {0, 0, 0, 0};
  struct nk_rect sym    = {0, 0, 0, 0};
  struct nk_text text;

  nk_flags                     ws = 0;
  enum nk_widget_layout_states widget_state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  /* cache some data */
  win          = ctx->current;
  layout       = win->layout;
  out          = &win->buffer;
  style        = &ctx->style;
  item_spacing = style->window.spacing;

  /* calculate header bounds and draw background */
  row_height = style->font_size + 2 * style->tab.padding.y;
  nk_layout_set_min_row_height(ctx, row_height);
  nk_layout_row_dynamic(ctx, row_height, 1);
  nk_layout_reset_min_row_height(ctx);

  widget_state = nk_widget_nav(&header, ctx);

  if (type == NK_TREE_TAB) {
    const struct nk_style_item *background = &style->tab.background;

    switch (background->type) {
      case NK_STYLE_ITEM_IMAGE:
        nk_draw_image(out, header, &background->data.image, nk_rgb_factor(nk_white, style->tab.color_factor));
        break;

      case NK_STYLE_ITEM_NINE_SLICE:
        nk_draw_nine_slice(out, header, &background->data.slice, nk_rgb_factor(nk_white, style->tab.color_factor));
        break;

      case NK_STYLE_ITEM_COLOR:
        nk_fill_rect(out, header, 0, nk_rgb_factor(style->tab.border_color, style->tab.color_factor));
        nk_fill_rect(out, nk_shrink_rect(header, style->tab.border), style->tab.rounding, nk_rgb_factor(background->data.color, style->tab.color_factor));
        break;
    }
  } else {
    text.background = style->window.background;
  }

  /* update node state */
  in = (!(layout->flags & NK_WINDOW_ROM)) ? &ctx->input : 0;
  in = (in && widget_state == NK_WIDGET_VALID) ? &ctx->input : 0;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ws, header, in, NK_BUTTON_DEFAULT)) {
    *state = (*state == NK_MAXIMIZED) ? NK_MINIMIZED : NK_MAXIMIZED;
  }

  /* select correct button style */
  if (*state == NK_MAXIMIZED) {
    symbol = style->tab.sym_maximize;

    if (type == NK_TREE_TAB) {
      button = &style->tab.tab_maximize_button;
    } else {
      button = &style->tab.node_maximize_button;
    }
  } else {
    symbol = style->tab.sym_minimize;

    if (type == NK_TREE_TAB) {
      button = &style->tab.tab_minimize_button;
    } else {
      button = &style->tab.node_minimize_button;
    }
  }

  { /* draw triangle button */
    sym.w = sym.h = style->font_size;
    sym.y = header.y + style->tab.padding.y;
    sym.x = header.x + style->tab.padding.x;
    nk_do_button_symbol(&ws, &win->buffer, sym, symbol, NK_BUTTON_DEFAULT, button, 0, style->font);

    if (img) {
      /* draw optional image icon */
      sym.x = sym.x + sym.w + 4 * item_spacing.x;
      nk_draw_image(&win->buffer, sym, img, nk_white);
      sym.w = style->font_size + style->tab.spacing.x;
    }
  }

  { /* draw label */
    struct nk_rect label;

    header.w     = NK_MAX(header.w, sym.w + item_spacing.x);
    label.x      = sym.x + sym.w + item_spacing.x;
    label.y      = sym.y;
    label.w      = header.w - (sym.w + item_spacing.y + style->tab.indent);
    label.h      = style->font_size;
    text.text    = nk_rgb_factor(style->tab.text, style->tab.color_factor);
    text.padding = nk_vec2(0, 0);
    nk_widget_text(out, label, title, title_len, &text, NK_TEXT_LEFT, style->font);
  }

  /* increase x-axis cursor widget position pointer */
  if (*state == NK_MAXIMIZED) {
    layout->at_x      = header.x + (float)*layout->offset_x + style->tab.indent;
    layout->bounds.w  = NK_MAX(layout->bounds.w, style->tab.indent);
    layout->bounds.w -= (style->tab.indent + style->window.padding.x);
    layout->row.tree_depth++;
    return nk_true;
  } else {
    return nk_false;
  }
}

NK_INTERN int
nk_tree_base(struct nk_context      *ctx,
             enum nk_tree_type       type,
             struct nk_image        *img,
             const char             *title,
             int                     title_len,
             enum nk_collapse_states initial_state,
             const char             *hash,
             int                     len,
             int                     line)
{
  struct nk_window *win       = ctx->current;
  nk_hash           tree_hash = 0;
  nk_uint          *state     = 0;

  /* retrieve tree state from internal widget state tables */
  if (!hash) {
    tree_hash = nk_murmur_hash(title, title_len, (nk_hash)line);
  } else {
    tree_hash = nk_murmur_hash(hash, len, (nk_hash)line);
  }

  state = nk_find_value(win, tree_hash);

  if (!state) {
    state  = nk_add_value(ctx, win, tree_hash, 0);
    *state = initial_state;
  }
  return nk_tree_state_base(ctx, type, img, title, title_len, (enum nk_collapse_states *)state);
}

NK_API nk_bool
nk_tree_state_push(struct nk_context *ctx, enum nk_tree_type type, const char *title, enum nk_collapse_states *state)
{
  return nk_tree_state_base(ctx, type, 0, title, nk_strlen(title), state);
}

NK_API nk_bool
nk_tree_state_push_text(struct nk_context *ctx, enum nk_tree_type type, const char *title, int title_len, enum nk_collapse_states *state)
{
  return nk_tree_state_base(ctx, type, 0, title, title_len, state);
}

NK_API nk_bool
nk_tree_state_image_push(struct nk_context *ctx, enum nk_tree_type type, struct nk_image img, const char *title, enum nk_collapse_states *state)
{
  return nk_tree_state_base(ctx, type, &img, title, nk_strlen(title), state);
}

NK_API void
nk_tree_state_pop(struct nk_context *ctx)
{
  struct nk_window *win    = 0;
  struct nk_panel  *layout = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win               = ctx->current;
  layout            = win->layout;
  layout->at_x     -= ctx->style.tab.indent + (float)*layout->offset_x;
  layout->bounds.w += ctx->style.tab.indent + ctx->style.window.padding.x;
  NK_ASSERT(layout->row.tree_depth);
  layout->row.tree_depth--;
}

NK_API nk_bool
nk_tree_push_hashed(struct nk_context *ctx, enum nk_tree_type type, const char *title, enum nk_collapse_states initial_state, const char *hash, int len, int line)
{
  return nk_tree_base(ctx, type, 0, title, nk_strlen(title), initial_state, hash, len, line);
}

NK_API nk_bool
nk_tree_push_hashed_text(
  struct nk_context *ctx, enum nk_tree_type type, const char *title, int title_len, enum nk_collapse_states initial_state, const char *hash, int len, int line)
{
  return nk_tree_base(ctx, type, 0, title, title_len, initial_state, hash, len, line);
}

NK_API nk_bool
nk_tree_image_push_hashed(
  struct nk_context *ctx, enum nk_tree_type type, struct nk_image img, const char *title, enum nk_collapse_states initial_state, const char *hash, int len, int seed)
{
  return nk_tree_base(ctx, type, &img, title, nk_strlen(title), initial_state, hash, len, seed);
}

NK_API void
nk_tree_pop(struct nk_context *ctx)
{
  nk_tree_state_pop(ctx);
}

NK_INTERN int
nk_tree_element_image_push_hashed_base(
  struct nk_context *ctx, enum nk_tree_type type, struct nk_image *img, const char *title, int title_len, enum nk_collapse_states *state, nk_bool *selected)
{
  struct nk_window             *win;
  struct nk_panel              *layout;
  const struct nk_style        *style;
  struct nk_command_buffer     *out;
  const struct nk_input        *in;
  const struct nk_style_button *button;
  enum nk_symbol_type           symbol;
  float                         row_height;
  struct nk_vec2                padding;

  int   text_len;
  float text_width;

  struct nk_vec2 item_spacing;
  struct nk_rect header = {0, 0, 0, 0};
  struct nk_rect sym    = {0, 0, 0, 0};

  nk_flags                     ws = 0;
  enum nk_widget_layout_states widget_state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  /* cache some data */
  win          = ctx->current;
  layout       = win->layout;
  out          = &win->buffer;
  style        = &ctx->style;
  item_spacing = style->window.spacing;
  padding      = style->selectable.padding;

  /* calculate header bounds and draw background */
  row_height = style->font_size + 2 * style->tab.padding.y;
  nk_layout_set_min_row_height(ctx, row_height);
  nk_layout_row_dynamic(ctx, row_height, 1);
  nk_layout_reset_min_row_height(ctx);

  widget_state = nk_widget_nav(&header, ctx);

  if (type == NK_TREE_TAB) {
    const struct nk_style_item *background = &style->tab.background;

    switch (background->type) {
      case NK_STYLE_ITEM_IMAGE:
        nk_draw_image(out, header, &background->data.image, nk_rgb_factor(nk_white, style->tab.color_factor));
        break;

      case NK_STYLE_ITEM_NINE_SLICE:
        nk_draw_nine_slice(out, header, &background->data.slice, nk_rgb_factor(nk_white, style->tab.color_factor));
        break;

      case NK_STYLE_ITEM_COLOR:
        nk_fill_rect(out, header, 0, nk_rgb_factor(style->tab.border_color, style->tab.color_factor));
        nk_fill_rect(out, nk_shrink_rect(header, style->tab.border), style->tab.rounding, nk_rgb_factor(background->data.color, style->tab.color_factor));

        break;
    }
  }

  in = (!(layout->flags & NK_WINDOW_ROM)) ? &ctx->input : 0;
  in = (in && widget_state == NK_WIDGET_VALID) ? &ctx->input : 0;

  /* select correct button style */
  if (*state == NK_MAXIMIZED) {
    symbol = style->tab.sym_maximize;

    if (type == NK_TREE_TAB) {
      button = &style->tab.tab_maximize_button;
    } else {
      button = &style->tab.node_maximize_button;
    }
  } else {
    symbol = style->tab.sym_minimize;

    if (type == NK_TREE_TAB) {
      button = &style->tab.tab_minimize_button;
    } else {
      button = &style->tab.node_minimize_button;
    }
  }

  { /* draw triangle button */
    sym.w = sym.h = style->font_size;
    sym.y = header.y + style->tab.padding.y;
    sym.x = header.x + style->tab.padding.x;

    if (nk_do_button_symbol(&ws, &win->buffer, sym, symbol, NK_BUTTON_DEFAULT, button, in, style->font)) {
      *state = (*state == NK_MAXIMIZED) ? NK_MINIMIZED : NK_MAXIMIZED;
    }
  }

  /* draw label */
  {
    nk_flags       dummy = 0;
    struct nk_rect label;

    /* calculate size of the text and tooltip */
    text_len    = nk_strlen(title);
    text_width  = nk_text_width(ctx, style->font, style->font_size, title, text_len);
    text_width += (4 * padding.x);

    header.w = NK_MAX(header.w, sym.w + item_spacing.x);
    label.x  = sym.x + sym.w + item_spacing.x;
    label.y  = sym.y;
    label.w  = NK_MIN(header.w - (sym.w + item_spacing.y + style->tab.indent), text_width);
    label.h  = style->font_size;

    if (img) {
      nk_do_selectable_image(&dummy, &win->buffer, label, title, title_len, NK_TEXT_LEFT, selected, img, &style->selectable, in, style->font);
    } else {
      nk_do_selectable(&dummy, &win->buffer, label, title, title_len, NK_TEXT_LEFT, selected, &style->selectable, in, style->font);
    }
  }

  /* increase x-axis cursor widget position pointer */
  if (*state == NK_MAXIMIZED) {
    layout->at_x      = header.x + (float)*layout->offset_x + style->tab.indent;
    layout->bounds.w  = NK_MAX(layout->bounds.w, style->tab.indent);
    layout->bounds.w -= (style->tab.indent + style->window.padding.x);
    layout->row.tree_depth++;
    return nk_true;
  } else {
    return nk_false;
  }
}

NK_INTERN int
nk_tree_element_base(struct nk_context      *ctx,
                     enum nk_tree_type       type,
                     struct nk_image        *img,
                     const char             *title,
                     enum nk_collapse_states initial_state,
                     nk_bool                *selected,
                     const char             *hash,
                     int                     len,
                     int                     line)
{
  struct nk_window *win       = ctx->current;
  int               title_len = 0;
  nk_hash           tree_hash = 0;
  nk_uint          *state     = 0;

  /* retrieve tree state from internal widget state tables */
  if (!hash) {
    title_len = (int)nk_strlen(title);
    tree_hash = nk_murmur_hash(title, (int)title_len, (nk_hash)line);
  } else {
    tree_hash = nk_murmur_hash(hash, len, (nk_hash)line);
  }

  state = nk_find_value(win, tree_hash);

  if (!state) {
    state  = nk_add_value(ctx, win, tree_hash, 0);
    *state = initial_state;
  }
  return nk_tree_element_image_push_hashed_base(ctx, type, img, title, nk_strlen(title), (enum nk_collapse_states *)state, selected);
}

NK_API nk_bool
nk_tree_element_push_hashed(
  struct nk_context *ctx, enum nk_tree_type type, const char *title, enum nk_collapse_states initial_state, nk_bool *selected, const char *hash, int len, int seed)
{
  return nk_tree_element_base(ctx, type, 0, title, initial_state, selected, hash, len, seed);
}

NK_API nk_bool
nk_tree_element_image_push_hashed(struct nk_context      *ctx,
                                  enum nk_tree_type       type,
                                  struct nk_image         img,
                                  const char             *title,
                                  enum nk_collapse_states initial_state,
                                  nk_bool                *selected,
                                  const char             *hash,
                                  int                     len,
                                  int                     seed)
{
  return nk_tree_element_base(ctx, type, &img, title, initial_state, selected, hash, len, seed);
}

NK_API void
nk_tree_element_pop(struct nk_context *ctx)
{
  nk_tree_state_pop(ctx);
}

/* ===============================================================
 *
 *                          GROUP
 *
 * ===============================================================*/
NK_API nk_bool
nk_group_scrolled_offset_begin(
  struct nk_context *ctx, nk_uint *x_offset, nk_uint *y_offset, nk_uint *prev_max_x, nk_uint *prev_max_y, nk_uint *scroll_flags, const char *title, nk_flags flags)
{
  struct nk_rect    bounds;
  struct nk_window  panel;
  struct nk_window *win;

  win = ctx->current;
  nk_panel_alloc_space(&bounds, ctx);

  {
    const struct nk_rect *c = &win->layout->clip;

    if (!NK_INTERSECT(c->x, c->y, c->w, c->h, bounds.x, bounds.y, bounds.w, bounds.h) && !(flags & NK_WINDOW_MOVABLE)) {
      return 0;
    }
  }

  if (win->flags & NK_WINDOW_ROM) {
    flags |= NK_WINDOW_ROM;
  }

  /* initialize a fake window to create the panel from */
  nk_zero(&panel, sizeof(panel));
  panel.bounds      = bounds;
  panel.flags       = flags;
  panel.scrollbar.x = *x_offset;
  panel.scrollbar.y = *y_offset;
  panel.buffer      = win->buffer;
  panel.layout      = (struct nk_panel *)nk_create_panel(ctx);
  ctx->current      = &panel;
  nk_panel_begin(ctx, (flags & NK_WINDOW_TITLE) ? title : 0, NK_PANEL_GROUP);

  win->buffer                        = panel.buffer;
  win->buffer.clip                   = panel.layout->clip;
  panel.layout->offset_x             = x_offset;
  panel.layout->offset_y             = y_offset;
  panel.layout->prev_max_x           = prev_max_x;
  panel.layout->prev_max_y           = prev_max_y;
  panel.layout->scroll_flags         = scroll_flags;
  panel.layout->parent               = win->layout;
  panel.layout->text_selection_scope = nk_murmur_hash(title, title ? (int)nk_strlen(title) : 0, win->layout->text_selection_scope);
  win->layout                        = panel.layout;

  ctx->current = win;

  if ((panel.layout->flags & NK_WINDOW_CLOSED) || (panel.layout->flags & NK_WINDOW_MINIMIZED)) {
    nk_flags f = panel.layout->flags;

    nk_group_scrolled_end(ctx);

    if (f & NK_WINDOW_CLOSED) {
      return NK_WINDOW_CLOSED;
    }

    if (f & NK_WINDOW_MINIMIZED) {
      return NK_WINDOW_MINIMIZED;
    }
  }
  return 1;
}

NK_API void
nk_group_scrolled_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_panel  *parent;
  struct nk_panel  *g;

  struct nk_rect   clip;
  struct nk_window pan;
  struct nk_vec2   panel_padding;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  /* make sure nk_group_begin was called correctly */
  NK_ASSERT(ctx->current);
  win = ctx->current;
  NK_ASSERT(win->layout);
  g = win->layout;
  NK_ASSERT(g->parent);
  parent = g->parent;

  /* dummy window */
  nk_zero_struct(pan);
  panel_padding = nk_panel_get_padding(&ctx->style, NK_PANEL_GROUP);
  pan.bounds.y  = g->bounds.y - (g->header_height + g->menu.h);
  pan.bounds.x  = g->bounds.x - panel_padding.x;
  pan.bounds.w  = g->bounds.w + 2 * panel_padding.x;
  pan.bounds.h  = g->bounds.h + g->header_height + g->menu.h;

  if (g->flags & NK_WINDOW_BORDER) {
    pan.bounds.x -= g->border;
    pan.bounds.y -= g->border;
    pan.bounds.w += 2 * g->border;
    pan.bounds.h += 2 * g->border;
  }

  if (!(g->flags & NK_WINDOW_NO_SCROLLBAR_V)) {
    pan.bounds.w += ctx->style.window.scrollbar_size.x;
  }

  if (!(g->flags & NK_WINDOW_NO_SCROLLBAR_H)) {
    pan.bounds.h += ctx->style.window.scrollbar_size.y;
  }

  pan.scrollbar.x = *g->offset_x;
  pan.scrollbar.y = *g->offset_y;
  pan.flags       = g->flags;
  pan.buffer      = win->buffer;
  pan.layout      = g;
  pan.parent      = win;
  ctx->current    = &pan;

  /* make sure group has correct clipping rectangle */
  nk_unify(&clip, &parent->clip, pan.bounds.x, pan.bounds.y, pan.bounds.x + pan.bounds.w, pan.bounds.y + pan.bounds.h + panel_padding.x);
  nk_push_scissor(&pan.buffer, clip);
  nk_end(ctx);

  win->buffer = pan.buffer;
  nk_push_scissor(&win->buffer, parent->clip);
  ctx->current = win;
  win->layout  = parent;
  g->bounds    = pan.bounds;
  return;
}

NK_API nk_bool
nk_group_scrolled_begin(struct nk_context *ctx, struct nk_scroll *scroll, const char *title, nk_flags flags)
{
  return nk_group_scrolled_offset_begin(ctx, &scroll->x, &scroll->y, &scroll->prev_max_x, &scroll->prev_max_y, &scroll->flags, title, flags);
}

NK_API nk_bool
nk_group_begin_titled(struct nk_context *ctx, const char *id, const char *title, nk_flags flags)
{
  int               id_len;
  nk_hash           id_hash;
  struct nk_window *win;
  nk_uint          *x_offset;
  nk_uint          *y_offset;
  nk_uint          *prev_max_x;
  nk_uint          *prev_max_y;
  nk_uint          *scroll_flags;

  NK_ASSERT(ctx);
  NK_ASSERT(id);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !id) {
    return 0;
  }

  /* find persistent group scrollbar value */
  win          = ctx->current;
  id_len       = (int)nk_strlen(id);
  id_hash      = nk_murmur_hash(id, (int)id_len, NK_PANEL_GROUP);
  x_offset     = nk_find_value(win, id_hash + 0);
  y_offset     = nk_find_value(win, id_hash + 1);
  prev_max_x   = nk_find_value(win, id_hash + 2);
  prev_max_y   = nk_find_value(win, id_hash + 3);
  scroll_flags = nk_find_value(win, id_hash + 4);

  if (!x_offset) {
    x_offset = nk_add_value(ctx, win, id_hash + 0, 0);
  }

  if (!y_offset) {
    y_offset = nk_add_value(ctx, win, id_hash + 1, 0);
  }

  if (!prev_max_x) {
    prev_max_x = nk_add_value(ctx, win, id_hash + 2, 0);
  }

  if (!prev_max_y) {
    prev_max_y = nk_add_value(ctx, win, id_hash + 3, 0);
  }

  if (!scroll_flags) {
    scroll_flags = nk_add_value(ctx, win, id_hash + 4, 0);
  }

  if (!x_offset || !y_offset || !prev_max_x || !prev_max_y || !scroll_flags) {
    return 0;
  }

  return nk_group_scrolled_offset_begin(ctx, x_offset, y_offset, prev_max_x, prev_max_y, scroll_flags, title, flags);
}

NK_API nk_bool
nk_group_begin(struct nk_context *ctx, const char *title, nk_flags flags)
{
  return nk_group_begin_titled(ctx, title, title, flags);
}

NK_API void
nk_group_end(struct nk_context *ctx)
{
  nk_group_scrolled_end(ctx);
}

NK_API void
nk_group_get_scroll(struct nk_context *ctx, const char *id, nk_uint *x_offset, nk_uint *y_offset, nk_uint *prev_max_x, nk_uint *prev_max_y, nk_uint *scroll_flags)
{
  int               id_len;
  nk_hash           id_hash;
  struct nk_window *win;
  nk_uint          *x_offset_ptr;
  nk_uint          *y_offset_ptr;
  nk_uint          *prev_max_x_ptr;
  nk_uint          *prev_max_y_ptr;
  nk_uint          *scroll_flags_ptr;

  NK_ASSERT(ctx);
  NK_ASSERT(id);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !id) {
    return;
  }

  /* find persistent group scrollbar value */
  win     = ctx->current;
  id_len  = (int)nk_strlen(id);
  id_hash = nk_murmur_hash(id, (int)id_len, NK_PANEL_GROUP);

  x_offset_ptr     = nk_find_value(win, id_hash + 0);
  y_offset_ptr     = nk_find_value(win, id_hash + 1);
  prev_max_x_ptr   = nk_find_value(win, id_hash + 2);
  prev_max_y_ptr   = nk_find_value(win, id_hash + 3);
  scroll_flags_ptr = nk_find_value(win, id_hash + 4);

  if (!x_offset_ptr) {
    x_offset_ptr = nk_add_value(ctx, win, id_hash + 0, 0);
  }

  if (!y_offset_ptr) {
    y_offset_ptr = nk_add_value(ctx, win, id_hash + 1, 0);
  }

  if (!prev_max_x_ptr) {
    prev_max_x_ptr = nk_add_value(ctx, win, id_hash + 2, 0);
  }

  if (!prev_max_y_ptr) {
    prev_max_y_ptr = nk_add_value(ctx, win, id_hash + 3, 0);
  }

  if (!scroll_flags_ptr) {
    scroll_flags_ptr = nk_add_value(ctx, win, id_hash + 4, 0);
  }

  if (!x_offset_ptr || !y_offset_ptr || !prev_max_x_ptr || !prev_max_y_ptr || !scroll_flags_ptr) {
    return;
  }

  if (x_offset) {
    *x_offset = *x_offset_ptr;
  }

  if (y_offset) {
    *y_offset = *y_offset_ptr;
  }

  if (prev_max_x) {
    *prev_max_x = *prev_max_x_ptr;
  }

  if (prev_max_y) {
    *prev_max_y = *prev_max_y_ptr;
  }

  if (scroll_flags) {
    *scroll_flags = *scroll_flags_ptr;
  }
}

NK_API void
nk_group_set_scroll(struct nk_context *ctx, const char *id, nk_uint x_offset, nk_uint y_offset)
{
  int               id_len;
  nk_hash           id_hash;
  struct nk_window *win;
  nk_uint          *x_offset_ptr;
  nk_uint          *y_offset_ptr;

  NK_ASSERT(ctx);
  NK_ASSERT(id);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !id) {
    return;
  }

  /* find persistent group scrollbar value */
  win     = ctx->current;
  id_len  = (int)nk_strlen(id);
  id_hash = nk_murmur_hash(id, (int)id_len, NK_PANEL_GROUP);

  x_offset_ptr = nk_find_value(win, id_hash + 0);
  y_offset_ptr = nk_find_value(win, id_hash + 1);

  if (!x_offset_ptr) {
    x_offset_ptr = nk_add_value(ctx, win, id_hash + 0, 0);
  }

  if (!y_offset_ptr) {
    y_offset_ptr = nk_add_value(ctx, win, id_hash + 1, 0);
  }

  if (!x_offset_ptr || !y_offset_ptr) {
    return;
  }

  *x_offset_ptr = x_offset;
  *y_offset_ptr = y_offset;
}

/* ===============================================================
 *
 *                          LIST VIEW
 *
 * ===============================================================*/
NK_API nk_bool
nk_list_view_begin(struct nk_context *ctx, struct nk_list_view *view, const char *title, nk_flags flags, int row_height, int row_count)
{
  int      title_len;
  nk_hash  title_hash;
  nk_uint *x_offset;
  nk_uint *y_offset;
  nk_uint *prev_max_x;
  nk_uint *prev_max_y;
  nk_uint *scroll_flags;

  int                    result;
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_style *style;
  struct nk_vec2         item_spacing;

  NK_ASSERT(ctx);
  NK_ASSERT(view);
  NK_ASSERT(title);

  if (!ctx || !view || !title) {
    return 0;
  }

  win          = ctx->current;
  style        = &ctx->style;
  item_spacing = style->window.spacing;
  row_height  += NK_MAX(0, (int)item_spacing.y);

  /* find persistent list view scrollbar offset */
  title_len  = (int)nk_strlen(title);
  title_hash = nk_murmur_hash(title, (int)title_len, NK_PANEL_GROUP);

  x_offset     = nk_find_value(win, title_hash + 0);
  y_offset     = nk_find_value(win, title_hash + 1);
  prev_max_x   = nk_find_value(win, title_hash + 2);
  prev_max_y   = nk_find_value(win, title_hash + 3);
  scroll_flags = nk_find_value(win, title_hash + 4);

  if (!x_offset) {
    x_offset = nk_add_value(ctx, win, title_hash + 0, 0);
  }

  if (!y_offset) {
    y_offset = nk_add_value(ctx, win, title_hash + 1, 0);
  }

  if (!prev_max_x) {
    prev_max_x = nk_add_value(ctx, win, title_hash + 2, 0);
  }

  if (!prev_max_y) {
    prev_max_y = nk_add_value(ctx, win, title_hash + 3, 0);
  }

  if (!scroll_flags) {
    scroll_flags = nk_add_value(ctx, win, title_hash + 4, 0);
  }

  if (!x_offset || !y_offset || !prev_max_x || !prev_max_y || !scroll_flags) {
    return 0;
  }

  view->scroll_value       = *y_offset;
  view->scroll_pointer     = y_offset;
  view->prev_max_value     = *prev_max_y;
  view->prev_max_pointer   = prev_max_y;
  view->prev_flags_pointer = scroll_flags;

  *y_offset = 0;
  result    = nk_group_scrolled_offset_begin(ctx, x_offset, y_offset, NULL, NULL, NULL, title, flags);
  win       = ctx->current;
  layout    = win->layout;

  view->total_height = row_height * NK_MAX(row_count, 1);

  {
    float   old_max_offset = (float)view->prev_max_value;
    float   new_max_offset = NK_MAX((float)view->total_height - layout->clip.h, 0.0f);
    nk_bool has_prev       = (*view->prev_flags_pointer & NK_SCROLL_HAS_PREV_MAX_Y) != 0;

    if (has_prev) {
      nk_bool had_scrollbar = old_max_offset > 0.0f;
      nk_bool was_at_end    = had_scrollbar && ((float)view->scroll_value >= NK_MAX(old_max_offset - 1.0f, 0.0f));

      if (was_at_end && new_max_offset > old_max_offset) {
        view->scroll_value = (nk_uint)new_max_offset;
      }
    }

    if ((float)view->scroll_value > new_max_offset) {
      view->scroll_value = (nk_uint)new_max_offset;
    }

    *view->prev_max_pointer    = (nk_uint)new_max_offset;
    *view->prev_flags_pointer |= NK_SCROLL_HAS_PREV_MAX_Y;
  }

  view->begin = (int)NK_MAX(((float)view->scroll_value / (float)row_height), 0.0f);
  view->begin = NK_MIN(view->begin, NK_MAX(row_count - 1, 0));
  view->count = (int)NK_MAX(nk_iceilf(layout->clip.h / (float)row_height), 0);
  view->count = NK_MIN(view->count, NK_MAX(row_count - view->begin, 0));
  view->end   = view->begin + view->count;
  view->ctx   = ctx;
  return result;
}

NK_API void
nk_list_view_end(struct nk_list_view *view)
{
  struct nk_context *ctx;
  struct nk_window  *win;
  struct nk_panel   *layout;

  NK_ASSERT(view);
  NK_ASSERT(view->ctx);
  NK_ASSERT(view->scroll_pointer);

  if (!view || !view->ctx) {
    return;
  }

  ctx                   = view->ctx;
  win                   = ctx->current;
  layout                = win->layout;
  layout->at_y          = layout->bounds.y + (float)view->total_height;
  *view->scroll_pointer = *view->scroll_pointer + view->scroll_value;
  nk_group_end(view->ctx);
}

/* ===============================================================
 *
 *                              WIDGET
 *
 * ===============================================================*/
NK_API struct nk_rect
nk_widget_bounds(const struct nk_context *ctx)
{
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_rect(0, 0, 0, 0);
  }

  nk_layout_peek(&bounds, ctx);
  return bounds;
}

NK_API struct nk_vec2
nk_widget_position(const struct nk_context *ctx)
{
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }

  nk_layout_peek(&bounds, ctx);
  return nk_vec2(bounds.x, bounds.y);
}

NK_API struct nk_vec2
nk_widget_size(const struct nk_context *ctx)
{
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return nk_vec2(0, 0);
  }

  nk_layout_peek(&bounds, ctx);
  return nk_vec2(bounds.w, bounds.h);
}

NK_API float
nk_widget_width(const struct nk_context *ctx)
{
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return 0;
  }

  nk_layout_peek(&bounds, ctx);
  return bounds.w;
}

NK_API float
nk_widget_height(const struct nk_context *ctx)
{
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return 0;
  }

  nk_layout_peek(&bounds, ctx);
  return bounds.h;
}

NK_API nk_bool
nk_widget_is_hovered(const struct nk_context *ctx)
{
  struct nk_rect c, v;
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || (ctx->active != ctx->current && !((int)ctx->current->layout->type & (int)NK_PANEL_SET_POPUP))) {
    return 0;
  }

  c   = ctx->current->layout->clip;
  c.x = (float)((int)c.x);
  c.y = (float)((int)c.y);
  c.w = (float)((int)c.w);
  c.h = (float)((int)c.h);

  nk_layout_peek(&bounds, ctx);
  nk_unify(&v, &c, bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h);

  if (!NK_INTERSECT(c.x, c.y, c.w, c.h, bounds.x, bounds.y, bounds.w, bounds.h)) {
    return 0;
  }
  return nk_input_is_mouse_hovering_rect(&ctx->input, bounds);
}

NK_API nk_bool
nk_widget_is_mouse_clicked(const struct nk_context *ctx, enum nk_buttons btn)
{
  struct nk_rect c, v;
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || (ctx->active != ctx->current && !((int)ctx->current->layout->type & (int)NK_PANEL_SET_POPUP))) {
    return 0;
  }

  c   = ctx->current->layout->clip;
  c.x = (float)((int)c.x);
  c.y = (float)((int)c.y);
  c.w = (float)((int)c.w);
  c.h = (float)((int)c.h);

  nk_layout_peek(&bounds, ctx);
  nk_unify(&v, &c, bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h);

  if (!NK_INTERSECT(c.x, c.y, c.w, c.h, bounds.x, bounds.y, bounds.w, bounds.h)) {
    return 0;
  }
  return nk_input_mouse_clicked(&ctx->input, btn, bounds);
}

NK_API nk_bool
nk_widget_has_mouse_click_down(const struct nk_context *ctx, enum nk_buttons btn, nk_bool down)
{
  struct nk_rect c, v;
  struct nk_rect bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || (ctx->active != ctx->current && !((int)ctx->current->layout->type & (int)NK_PANEL_SET_POPUP))) {
    return 0;
  }

  c   = ctx->current->layout->clip;
  c.x = (float)((int)c.x);
  c.y = (float)((int)c.y);
  c.w = (float)((int)c.w);
  c.h = (float)((int)c.h);

  nk_layout_peek(&bounds, ctx);
  nk_unify(&v, &c, bounds.x, bounds.y, bounds.x + bounds.w, bounds.y + bounds.h);

  if (!NK_INTERSECT(c.x, c.y, c.w, c.h, bounds.x, bounds.y, bounds.w, bounds.h)) {
    return 0;
  }
  return nk_input_has_mouse_click_down_in_rect(&ctx->input, btn, bounds, down);
}

NK_API enum nk_widget_layout_states
nk_widget(struct nk_rect *bounds, const struct nk_context *ctx)
{
  struct nk_rect         c, v;
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return NK_WIDGET_INVALID;
  }

  /* allocate space and check if the widget needs to be updated and drawn */
  nk_panel_alloc_space(bounds, ctx);
  win    = ctx->current;
  layout = win->layout;
  in     = &ctx->input;
  c      = layout->clip;

  /*  if one of these triggers you forgot to add an `if` condition around either
      a window, group, popup, combobox or contextual menu `begin` and `end` block.
      Example:
          if (nk_begin(...) {...} nk_end(...); or
          if (nk_group_begin(...) { nk_group_end(...);} */
  NK_ASSERT(!(layout->flags & NK_WINDOW_MINIMIZED));
  NK_ASSERT(!(layout->flags & NK_WINDOW_HIDDEN));
  NK_ASSERT(!(layout->flags & NK_WINDOW_CLOSED));

  /* need to convert to int here to remove floating point errors */
  bounds->x = (float)((int)bounds->x);
  bounds->y = (float)((int)bounds->y);
  bounds->w = (float)((int)bounds->w);
  bounds->h = (float)((int)bounds->h);

  c.x = (float)((int)c.x);
  c.y = (float)((int)c.y);
  c.w = (float)((int)c.w);
  c.h = (float)((int)c.h);

  nk_unify(&v, &c, bounds->x, bounds->y, bounds->x + bounds->w, bounds->y + bounds->h);

  if (!NK_INTERSECT(c.x, c.y, c.w, c.h, bounds->x, bounds->y, bounds->w, bounds->h)) {
    return NK_WIDGET_INVALID;
  }

  if (win->widgets_disabled) {
    return NK_WIDGET_DISABLED;
  }

  if (!NK_INBOX(in->mouse.pos.x, in->mouse.pos.y, v.x, v.y, v.w, v.h)) {
    return NK_WIDGET_ROM;
  }
  return NK_WIDGET_VALID;
}

/* out of view: a hidden stop */
NK_LIB enum nk_widget_layout_states
nk_widget_nav(struct nk_rect *bounds, struct nk_context *ctx)
{
  enum nk_widget_layout_states state = nk_widget(bounds, ctx);

  if (state == NK_WIDGET_INVALID && ctx && ctx->current && ctx->current->layout) {
    nk_nav_stop(ctx, *bounds, NK_NAV_STOP_HIDDEN);
  }
  return state;
}

NK_API enum nk_widget_layout_states
nk_widget_fitting(struct nk_rect *bounds, const struct nk_context *ctx, struct nk_vec2 item_padding)
{
  /* update the bounds to stand without padding  */
  enum nk_widget_layout_states state;

  NK_UNUSED(item_padding);

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return NK_WIDGET_INVALID;
  }

  state = nk_widget(bounds, ctx);
  return state;
}

NK_API void
nk_spacing(struct nk_context *ctx, int cols)
{
  struct nk_window *win;
  struct nk_panel  *layout;
  struct nk_rect    none;
  int               i, index, rows;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  /* spacing over row boundaries */
  win    = ctx->current;
  layout = win->layout;
  index  = (layout->row.index + cols) % layout->row.columns;
  rows   = (layout->row.index + cols) / layout->row.columns;

  if (rows) {
    for (i = 0; i < rows; ++i) {
      nk_panel_alloc_row(ctx, win);
    }

    cols = index;
  }

  /* non table layout need to allocate space */
  if (layout->row.type != NK_LAYOUT_DYNAMIC_FIXED && layout->row.type != NK_LAYOUT_STATIC_FIXED) {
    for (i = 0; i < cols; ++i) {
      nk_panel_alloc_space(&none, ctx);
    }
  }

  layout->row.index = index;
}

NK_API void
nk_widget_disable_begin(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_style  *style;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win   = ctx->current;
  style = &ctx->style;

  win->widgets_disabled = nk_true;

  style->button.color_factor_text                        = style->button.disabled_factor;
  style->button.color_factor_background                  = style->button.disabled_factor;
  style->chart.color_factor                              = style->chart.disabled_factor;
  style->checkbox.color_factor                           = style->checkbox.disabled_factor;
  style->combo.color_factor                              = style->combo.disabled_factor;
  style->combo.button.color_factor_text                  = style->combo.button.disabled_factor;
  style->combo.button.color_factor_background            = style->combo.button.disabled_factor;
  style->contextual_button.color_factor_text             = style->contextual_button.disabled_factor;
  style->contextual_button.color_factor_background       = style->contextual_button.disabled_factor;
  style->edit.color_factor                               = style->edit.disabled_factor;
  style->edit.scrollbar.color_factor                     = style->edit.scrollbar.disabled_factor;
  style->menu_button.color_factor_text                   = style->menu_button.disabled_factor;
  style->menu_button.color_factor_background             = style->menu_button.disabled_factor;
  style->option.color_factor                             = style->option.disabled_factor;
  style->progress.color_factor                           = style->progress.disabled_factor;
  style->property.color_factor                           = style->property.disabled_factor;
  style->property.inc_button.color_factor_text           = style->property.inc_button.disabled_factor;
  style->property.inc_button.color_factor_background     = style->property.inc_button.disabled_factor;
  style->property.dec_button.color_factor_text           = style->property.dec_button.disabled_factor;
  style->property.dec_button.color_factor_background     = style->property.dec_button.disabled_factor;
  style->property.edit.color_factor                      = style->property.edit.disabled_factor;
  style->scrollh.color_factor                            = style->scrollh.disabled_factor;
  style->scrollh.inc_button.color_factor_text            = style->scrollh.inc_button.disabled_factor;
  style->scrollh.inc_button.color_factor_background      = style->scrollh.inc_button.disabled_factor;
  style->scrollh.dec_button.color_factor_text            = style->scrollh.dec_button.disabled_factor;
  style->scrollh.dec_button.color_factor_background      = style->scrollh.dec_button.disabled_factor;
  style->scrollv.color_factor                            = style->scrollv.disabled_factor;
  style->scrollv.inc_button.color_factor_text            = style->scrollv.inc_button.disabled_factor;
  style->scrollv.inc_button.color_factor_background      = style->scrollv.inc_button.disabled_factor;
  style->scrollv.dec_button.color_factor_text            = style->scrollv.dec_button.disabled_factor;
  style->scrollv.dec_button.color_factor_background      = style->scrollv.dec_button.disabled_factor;
  style->selectable.color_factor                         = style->selectable.disabled_factor;
  style->slider.color_factor                             = style->slider.disabled_factor;
  style->slider.inc_button.color_factor_text             = style->slider.inc_button.disabled_factor;
  style->slider.inc_button.color_factor_background       = style->slider.inc_button.disabled_factor;
  style->slider.dec_button.color_factor_text             = style->slider.dec_button.disabled_factor;
  style->slider.dec_button.color_factor_background       = style->slider.dec_button.disabled_factor;
  style->tab.color_factor                                = style->tab.disabled_factor;
  style->tab.node_maximize_button.color_factor_text      = style->tab.node_maximize_button.disabled_factor;
  style->tab.node_minimize_button.color_factor_text      = style->tab.node_minimize_button.disabled_factor;
  style->tab.tab_maximize_button.color_factor_text       = style->tab.tab_maximize_button.disabled_factor;
  style->tab.tab_maximize_button.color_factor_background = style->tab.tab_maximize_button.disabled_factor;
  style->tab.tab_minimize_button.color_factor_text       = style->tab.tab_minimize_button.disabled_factor;
  style->tab.tab_minimize_button.color_factor_background = style->tab.tab_minimize_button.disabled_factor;
  style->text.color_factor                               = style->text.disabled_factor;
}

NK_API void
nk_widget_disable_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_style  *style;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win   = ctx->current;
  style = &ctx->style;

  win->widgets_disabled = nk_false;

  style->button.color_factor_text                        = 1.0f;
  style->button.color_factor_background                  = 1.0f;
  style->chart.color_factor                              = 1.0f;
  style->checkbox.color_factor                           = 1.0f;
  style->combo.color_factor                              = 1.0f;
  style->combo.button.color_factor_text                  = 1.0f;
  style->combo.button.color_factor_background            = 1.0f;
  style->contextual_button.color_factor_text             = 1.0f;
  style->contextual_button.color_factor_background       = 1.0f;
  style->edit.color_factor                               = 1.0f;
  style->edit.scrollbar.color_factor                     = 1.0f;
  style->menu_button.color_factor_text                   = 1.0f;
  style->menu_button.color_factor_background             = 1.0f;
  style->option.color_factor                             = 1.0f;
  style->progress.color_factor                           = 1.0f;
  style->property.color_factor                           = 1.0f;
  style->property.inc_button.color_factor_text           = 1.0f;
  style->property.inc_button.color_factor_background     = 1.0f;
  style->property.dec_button.color_factor_text           = 1.0f;
  style->property.dec_button.color_factor_background     = 1.0f;
  style->property.edit.color_factor                      = 1.0f;
  style->scrollh.color_factor                            = 1.0f;
  style->scrollh.inc_button.color_factor_text            = 1.0f;
  style->scrollh.inc_button.color_factor_background      = 1.0f;
  style->scrollh.dec_button.color_factor_text            = 1.0f;
  style->scrollh.dec_button.color_factor_background      = 1.0f;
  style->scrollv.color_factor                            = 1.0f;
  style->scrollv.inc_button.color_factor_text            = 1.0f;
  style->scrollv.inc_button.color_factor_background      = 1.0f;
  style->scrollv.dec_button.color_factor_text            = 1.0f;
  style->scrollv.dec_button.color_factor_background      = 1.0f;
  style->selectable.color_factor                         = 1.0f;
  style->slider.color_factor                             = 1.0f;
  style->slider.inc_button.color_factor_text             = 1.0f;
  style->slider.inc_button.color_factor_background       = 1.0f;
  style->slider.dec_button.color_factor_text             = 1.0f;
  style->slider.dec_button.color_factor_background       = 1.0f;
  style->tab.color_factor                                = 1.0f;
  style->tab.node_maximize_button.color_factor_text      = 1.0f;
  style->tab.node_minimize_button.color_factor_text      = 1.0f;
  style->tab.tab_maximize_button.color_factor_text       = 1.0f;
  style->tab.tab_maximize_button.color_factor_background = 1.0f;
  style->tab.tab_minimize_button.color_factor_text       = 1.0f;
  style->tab.tab_minimize_button.color_factor_background = 1.0f;
  style->text.color_factor                               = 1.0f;
}

/* ===============================================================
 *
 *                              TEXT
 *
 * ===============================================================*/
NK_INTERN struct nk_rect
nk_widget_text_layout(const struct nk_context *ctx, struct nk_rect bounds, const struct nk_text *text, nk_flags alignment, float text_width)
{
  struct nk_rect label;

  if (!(alignment & (NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_CENTERED | NK_TEXT_ALIGN_RIGHT))) {
    alignment |= NK_TEXT_ALIGN_LEFT;
  }

  if (!(alignment & (NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_BOTTOM))) {
    alignment |= NK_TEXT_ALIGN_TOP;
  }

  if (alignment & NK_TEXT_ALIGN_LEFT) {
    label.x = bounds.x + text->padding.x;
    label.w = NK_MAX(0, bounds.w - 2 * text->padding.x);
  } else if (alignment & NK_TEXT_ALIGN_CENTERED) {
    label.w = NK_MAX(1, 2 * text->padding.x + text_width);
    label.x = bounds.x + text->padding.x + ((bounds.w - 2 * text->padding.x) - label.w) / 2;
    label.x = NK_MAX(bounds.x + text->padding.x, label.x);
    label.w = NK_MIN(bounds.x + bounds.w, label.x + label.w);

    if (label.w >= label.x) {
      label.w -= label.x;
    }
  } else {
    label.x = NK_MAX(bounds.x + text->padding.x, bounds.x + bounds.w - (2 * text->padding.x + text_width));
    label.w = text_width + 2 * text->padding.x;
  }

  if (alignment & NK_TEXT_ALIGN_TOP) {
    label.y = bounds.y + text->padding.y;
    label.h = NK_MIN(ctx->style.font_size, bounds.h - 2 * text->padding.y);
  } else if (alignment & NK_TEXT_ALIGN_MIDDLE) {
    label.h = NK_MIN(ctx->style.font_size, NK_MAX(0, bounds.h - 2 * text->padding.y));
    label.y = bounds.y + (bounds.h - label.h) / 2.0f;
  } else {
    label.y = bounds.y + bounds.h - ctx->style.font_size;
    label.h = ctx->style.font_size;
  }
  return label;
}

NK_LIB void
nk_widget_text(struct nk_command_buffer *o, struct nk_rect b, const char *string, int len, const struct nk_text *t, nk_flags a, const struct nk_user_font *f)
{
  struct nk_text_shape *shape;
  struct nk_rect        label;
  float                 text_width;

  NK_ASSERT(o);
  NK_ASSERT(t);

  if (!o || !t || !o->context) {
    return;
  }

  b.h = NK_MAX(b.h, 2 * t->padding.y);

  shape       = nk_text_shape_build_default(o->context, f, o->context->style.font_size, string, len);
  text_width  = shape ? shape->advance.x : 0.0f;
  text_width += (2.0f * t->padding.x);

  label = nk_widget_text_layout(o->context, b, t, a, text_width);
  nk_draw_text_shaped(o, label, (const char *)string, len, f, t->background, t->text, shape);
}

NK_LIB void
nk_widget_text_wrap(struct nk_command_buffer *o, struct nk_rect b, const char *string, int len, const struct nk_text *t, const struct nk_user_font *f)
{
  struct nk_text_shape         *shape;
  struct nk_text_wrap_iterator  iterator;
  struct nk_text_wrap_line      wrapped;
  struct nk_rect                line;
  float                         bottom;

  NK_ASSERT(o);
  NK_ASSERT(t);

  if (!o || !t || !o->context) {
    return;
  }

  b.w = NK_MAX(b.w, 2 * t->padding.x);
  b.h = NK_MAX(b.h, 2 * t->padding.y);

  line.x = b.x + t->padding.x;
  line.y = b.y + t->padding.y;
  line.w = b.w - 2 * t->padding.x;
  line.h = 2 * t->padding.y + o->context->style.font_size;
  bottom = b.y + b.h - t->padding.y;

  shape = nk_text_shape_build_default(o->context, f, o->context->style.font_size, string, len);

  if (!shape) {
    return;
  }

  nk_text_wrap_iterator_begin(&iterator, shape, line.w);

  while (line.y + line.h <= bottom && nk_text_wrap_iterator_next(&iterator, &wrapped)) {
    int draw_end = wrapped.source_end;

    while (draw_end > wrapped.source_begin && (string[draw_end - 1] == '\r' || string[draw_end - 1] == '\n')) {
      draw_end -= 1;
    }

    nk_push_draw_text(o, line, string + wrapped.source_begin, draw_end - wrapped.source_begin,
                      f, o->context->style.font_size, t->background, t->text);
    line.y += line.h;
  }
}

NK_INTERN void
nk_text_widget_colored(struct nk_context *ctx, const char *str, int len, nk_flags alignment, struct nk_color color)
{
  struct nk_window *win;
  struct nk_style  *style;

  struct nk_vec2 item_padding;
  struct nk_rect bounds;
  struct nk_text text;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win   = ctx->current;
  style = &ctx->style;
  nk_panel_alloc_space(&bounds, ctx);
  item_padding = style->text.padding;

  text.padding.x  = item_padding.x;
  text.padding.y  = item_padding.y;
  text.background = style->window.background;
  text.text       = nk_rgb_factor(color, style->text.color_factor);
  nk_widget_text(&win->buffer, bounds, str, len, &text, alignment, style->font);
}

NK_INTERN void
nk_text_wrap_widget_colored(struct nk_context *ctx, const char *str, int len, struct nk_color color)
{
  struct nk_window *win;
  struct nk_style  *style;

  struct nk_vec2 item_padding;
  struct nk_rect bounds;
  struct nk_text text;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win   = ctx->current;
  style = &ctx->style;
  nk_panel_alloc_space(&bounds, ctx);
  item_padding = style->text.padding;

  text.padding.x  = item_padding.x;
  text.padding.y  = item_padding.y;
  text.background = style->window.background;
  text.text       = nk_rgb_factor(color, style->text.color_factor);
  nk_widget_text_wrap(&win->buffer, bounds, str, len, &text, style->font);
}

NK_API void
nk_text_colored(struct nk_context *ctx, const char *str, int len, nk_flags alignment, struct nk_color color)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = alignment;
  options.color     = color;
#ifdef NK_DEFAULT_TEXT_SELECTABLE
  options.flags |= NK_TEXT_OPTION_SELECTABLE;
#endif

  (void)nk_text_ex(ctx, str, len, &options);
}

NK_API void
nk_text_wrap_colored(struct nk_context *ctx, const char *str, int len, struct nk_color color)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = NK_TEXT_LEFT;
  options.flags     = NK_TEXT_OPTION_WRAP;
  options.color     = color;
#ifdef NK_DEFAULT_TEXT_SELECTABLE
  options.flags |= NK_TEXT_OPTION_SELECTABLE;
#endif

  (void)nk_text_ex(ctx, str, len, &options);
}

#ifdef NK_INCLUDE_STANDARD_VARARGS
NK_API void
nk_labelf_colored(struct nk_context *ctx, nk_flags flags, struct nk_color color, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_labelfv_colored(ctx, flags, color, fmt, args);
  va_end(args);
}

NK_API void
nk_labelf_colored_wrap(struct nk_context *ctx, struct nk_color color, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_labelfv_colored_wrap(ctx, color, fmt, args);
  va_end(args);
}

NK_API void
nk_labelf(struct nk_context *ctx, nk_flags flags, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_labelfv(ctx, flags, fmt, args);
  va_end(args);
}

NK_API void
nk_labelf_wrap(struct nk_context *ctx, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_labelfv_wrap(ctx, fmt, args);
  va_end(args);
}

NK_API void
nk_labelfv_colored(struct nk_context *ctx, nk_flags flags, struct nk_color color, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_label_colored(ctx, buf, flags, color);
}

NK_API void
nk_labelfv_colored_wrap(struct nk_context *ctx, struct nk_color color, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_label_colored_wrap(ctx, buf, color);
}

NK_API void
nk_labelfv(struct nk_context *ctx, nk_flags flags, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_label(ctx, buf, flags);
}

NK_API void
nk_labelfv_wrap(struct nk_context *ctx, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_label_wrap(ctx, buf);
}

NK_API void
nk_value_bool(struct nk_context *ctx, const char *prefix, int value)
{
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: %s", prefix, ((value) ? "true" : "false"));
}

NK_API void
nk_value_int(struct nk_context *ctx, const char *prefix, int value)
{
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: %d", prefix, value);
}

NK_API void
nk_value_uint(struct nk_context *ctx, const char *prefix, unsigned int value)
{
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: %u", prefix, value);
}

NK_API void
nk_value_float(struct nk_context *ctx, const char *prefix, float value)
{
  double double_value = (double)value;

  nk_labelf(ctx, NK_TEXT_LEFT, "%s: %.3f", prefix, double_value);
}

NK_API void
nk_value_color_byte(struct nk_context *ctx, const char *p, struct nk_color c)
{
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: (%d, %d, %d, %d)", p, c.r, c.g, c.b, c.a);
}

NK_API void
nk_value_color_float(struct nk_context *ctx, const char *p, struct nk_color color)
{
  double c[4];

  nk_color_dv(c, color);
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: (%.2f, %.2f, %.2f, %.2f)", p, c[0], c[1], c[2], c[3]);
}

NK_API void
nk_value_color_hex(struct nk_context *ctx, const char *prefix, struct nk_color color)
{
  char hex[16];

  nk_color_hex_rgba(hex, color);
  nk_labelf(ctx, NK_TEXT_LEFT, "%s: %s", prefix, hex);
}
#endif

NK_API void
nk_text(struct nk_context *ctx, const char *str, int len, nk_flags alignment)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  nk_text_colored(ctx, str, len, alignment, ctx->style.text.color);
}

NK_API void
nk_text_wrap(struct nk_context *ctx, const char *str, int len)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  nk_text_wrap_colored(ctx, str, len, ctx->style.text.color);
}

NK_API void
nk_label(struct nk_context *ctx, const char *str, nk_flags alignment)
{
  nk_text(ctx, str, nk_strlen(str), alignment);
}

NK_API void
nk_label_colored(struct nk_context *ctx, const char *str, nk_flags align, struct nk_color color)
{
  nk_text_colored(ctx, str, nk_strlen(str), align, color);
}

NK_API void
nk_label_wrap(struct nk_context *ctx, const char *str)
{
  nk_text_wrap(ctx, str, nk_strlen(str));
}

NK_API void
nk_label_colored_wrap(struct nk_context *ctx, const char *str, struct nk_color color)
{
  nk_text_wrap_colored(ctx, str, nk_strlen(str), color);
}

NK_INTERN void
nk_text_selection_collapse(const struct nk_text_shape *shape, struct nk_text_selection *selection, int direction)
{
  struct nk_text_cursor anchor     = nk_text_shape_cursor_at(shape, selection->anchor, (enum nk_text_affinity)selection->anchor_affinity);
  struct nk_text_cursor cursor     = nk_text_shape_cursor_at(shape, selection->cursor, (enum nk_text_affinity)selection->affinity);
  nk_bool               use_anchor = direction < 0 ? anchor.x < cursor.x : anchor.x > cursor.x;
  struct nk_text_cursor edge       = use_anchor ? anchor : cursor;

  selection->anchor          = edge.source_offset;
  selection->cursor          = edge.source_offset;
  selection->affinity        = (unsigned char)edge.affinity;
  selection->anchor_affinity = (unsigned char)edge.affinity;
}

NK_INTERN struct nk_text_selection_context *
nk_text_selection_panel_context(struct nk_context *ctx)
{
  struct nk_panel *panel = ctx->current->layout;

  if (!panel->text_selection_loaded) {
    nk_text_selection_panel_load(ctx, panel);
  }
  return &panel->text_selection;
}

struct nk_text_selection_copy_chunk {
  struct nk_command header;
  nk_size           next;
  int               length;
  char              data[1];
};

struct nk_text_selection_copy_result {
  struct nk_command header;
  char              data[1];
};

NK_INTERN struct nk_text_selection_copy_chunk *
nk_text_selection_copy_chunk_at(struct nk_context *ctx, nk_size offset)
{
  if (!offset) {
    return NULL;
  }
  return nk_ptr_add(struct nk_text_selection_copy_chunk, ctx->memory.memory.ptr, offset - 1);
}

NK_INTERN nk_bool
nk_text_selection_context_empty(const struct nk_text_selection_context *context)
{
  return context->anchor_widget == context->cursor_widget && context->selection.anchor == context->selection.cursor;
}

NK_INTERN void
nk_text_selection_context_collapse(struct nk_text_selection_context *context, const struct nk_text_shape *shape, int direction)
{
  struct nk_text_selection *selection = &context->selection;
  nk_bool                   use_anchor;

  if (context->anchor_widget == context->cursor_widget) {
    nk_text_selection_collapse(shape, selection, direction);
    return;
  }

  use_anchor = direction < 0 ? context->anchor_widget < context->cursor_widget
                             : context->anchor_widget > context->cursor_widget;

  if (use_anchor) {
    context->cursor_widget = context->anchor_widget;
    selection->cursor      = selection->anchor;
    selection->affinity    = selection->anchor_affinity;
  } else {
    context->anchor_widget     = context->cursor_widget;
    selection->anchor          = selection->cursor;
    selection->anchor_affinity = selection->affinity;
  }

  context->anchor_widget     = context->cursor_widget;
  selection->anchor          = selection->cursor;
  selection->anchor_affinity = selection->affinity;
}

NK_INTERN nk_bool
nk_text_selection_widget_range(struct nk_text_selection_context *context, unsigned int widget, int len, int *begin, int *end)
{
  struct nk_text_selection *selection = &context->selection;

  if (!selection->active) {
    return nk_false;
  }

  if (context->anchor_widget == context->cursor_widget) {
    if (widget != context->anchor_widget) {
      return nk_false;
    }

    *begin = NK_MIN(selection->anchor, selection->cursor);
    *end   = NK_MAX(selection->anchor, selection->cursor);
  } else if (context->anchor_widget < context->cursor_widget) {
    if (widget < context->anchor_widget || widget > context->cursor_widget) {
      return nk_false;
    }

    *begin = widget == context->anchor_widget ? selection->anchor : 0;
    *end   = widget == context->cursor_widget ? selection->cursor : len;
  } else {
    if (widget < context->cursor_widget || widget > context->anchor_widget) {
      return nk_false;
    }

    *begin = widget == context->cursor_widget ? selection->cursor : 0;
    *end   = widget == context->anchor_widget ? selection->anchor : len;
  }

  *begin = NK_CLAMP(0, *begin, len);
  *end   = NK_CLAMP(0, *end, len);
  return nk_true;
}

NK_INTERN void
nk_text_selection_copy_append(struct nk_context *ctx, struct nk_text_selection_context *context,
                              const char *text, int length, struct nk_rect bounds)
{
  int                                  prefix = context->copy_first ? 1 : 0;
  nk_size                              size   = NK_OFFSETOF(struct nk_text_selection_copy_chunk, data) + prefix + length;
  struct nk_text_selection_copy_chunk *chunk;
  nk_size                              offset;

  chunk = nk_command_buffer_push(&ctx->current->buffer, NK_COMMAND_NOP, size);

  if (!chunk) {
    return;
  }

  offset        = (nk_size)((nk_byte *)chunk - (nk_byte *)ctx->memory.memory.ptr) + 1;
  chunk->next   = 0;
  chunk->length = prefix + length;

  if (prefix) {
    nk_bool same_row = bounds.y < context->copy_bounds.y + context->copy_bounds.h && context->copy_bounds.y < bounds.y + bounds.h;

    chunk->data[0] = same_row ? '\t' : '\n';
  }

  if (length) {
    NK_MEMCPY(chunk->data + prefix, text, length);
  }

  if (context->copy_last) {
    nk_text_selection_copy_chunk_at(ctx, context->copy_last)->next = offset;
  } else {
    context->copy_first = offset;
  }

  context->copy_last    = offset;
  context->copy_length += chunk->length;
  context->copy_bounds  = bounds;
}

NK_INTERN void
nk_text_selection_copy_finish(struct nk_context *ctx, struct nk_text_selection_context *context)
{
  struct nk_text_selection_copy_result *result;
  nk_size                               chunk_offset = context->copy_first;
  int                                   text_offset  = 0;

  if (!chunk_offset || !ctx->clip.copy) {
    return;
  }

  result = nk_command_buffer_push(&ctx->current->buffer, NK_COMMAND_NOP, NK_OFFSETOF(struct nk_text_selection_copy_result, data) + context->copy_length);

  if (!result) {
    return;
  }

  while (chunk_offset) {
    struct nk_text_selection_copy_chunk *chunk = nk_text_selection_copy_chunk_at(ctx, chunk_offset);

    NK_MEMCPY(result->data + text_offset, chunk->data, chunk->length);
    text_offset += chunk->length;
    chunk_offset = chunk->next;
  }

  ctx->clip.copy(ctx->clip.userdata, result->data, context->copy_length);
}

NK_INTERN unsigned int
nk_text_selection_widget_begin(struct nk_context *ctx, struct nk_text_selection_context *context)
{
  if (context->frame != ctx->seq) {
    context->widget_count = context->widget_index;
    context->frame        = ctx->seq;
    context->widget_index = 0;
    context->copy_first   = 0;
    context->copy_last    = 0;
    context->copy_length  = 0;

    if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) || nk_text_selection_storage_window(ctx->current)->edit.active) {
      context->selection.active   = nk_false;
      context->selection.dragging = nk_false;
    } else if (!ctx->input.mouse.buttons[NK_BUTTON_LEFT].down && !nk_input_is_mouse_released(&ctx->input, NK_BUTTON_LEFT)) {
      context->selection.dragging = nk_false;
    }
  }
  return ++context->widget_index;
}

NK_INTERN void
nk_text_selection_clamp(struct nk_text_selection_context *context, unsigned int widget, const struct nk_text_shape *shape, int len)
{
  struct nk_text_selection *selection = &context->selection;

  if (widget == context->anchor_widget) {
    selection->anchor = NK_CLAMP(0, selection->anchor, len);

    if (shape && selection->anchor > 0 && selection->anchor < len) {
      selection->anchor = nk_text_shape_boundary_previous(shape, selection->anchor + 1, NK_TEXT_BOUNDARY_GRAPHEME);
    }
  }

  if (widget == context->cursor_widget) {
    selection->cursor = NK_CLAMP(0, selection->cursor, len);

    if (shape && selection->cursor > 0 && selection->cursor < len) {
      selection->cursor = nk_text_shape_boundary_previous(shape, selection->cursor + 1, NK_TEXT_BOUNDARY_GRAPHEME);
    }
  }
}

NK_INTERN nk_bool
nk_text_selection_copy(struct nk_context *ctx, struct nk_text_selection_context *context, unsigned int widget,
                       const char *text, int len, struct nk_rect bounds, int *begin, int *end)
{
  if (!nk_text_selection_widget_range(context, widget, len, begin, end)) {
    return nk_false;
  }

  if (nk_input_is_key_pressed(&ctx->input, NK_KEY_COPY) && ctx->clip.copy) {
    nk_text_selection_copy_append(ctx, context, text + *begin, *end - *begin, bounds);

    if (widget == NK_MAX(context->anchor_widget, context->cursor_widget)) {
      nk_text_selection_copy_finish(ctx, context);
    }
  }
  return nk_true;
}

NK_INTERN nk_bool
nk_text_selection_widget_clipped(struct nk_context *ctx, const char *text, int len,
                                 const struct nk_text_options *options, struct nk_rect bounds)
{
  struct nk_text_selection_context *context;
  unsigned int                      widget;
  int                               selected_begin;
  int                               selected_end;

  if (!(options->flags & NK_TEXT_OPTION_SELECTABLE)) {
    return nk_false;
  }

  context = options->selection ? options->selection : nk_text_selection_panel_context(ctx);
  widget  = nk_text_selection_widget_begin(ctx, context);
  nk_text_selection_copy(ctx, context, widget, text, len, bounds, &selected_begin, &selected_end);
  return nk_false;
}

NK_INTERN void
nk_text_selection_keyboard(struct nk_context *ctx, const struct nk_text_shape *shape, struct nk_text_selection_context *context, unsigned int widget)
{
  struct nk_input          *input     = &ctx->input;
  struct nk_text_selection *selection = &context->selection;
  nk_bool                   shift;

  if (!selection->active || context->cursor_widget != widget || context->keyboard_frame == ctx->seq) {
    return;
  }

  shift = input->keyboard.keys[NK_KEY_SHIFT].down;

  if (nk_input_is_key_pressed(input, NK_KEY_TEXT_SELECT_ALL)) {
    context->keyboard_frame    = ctx->seq;
    context->anchor_widget     = 1;
    context->cursor_widget     = context->widget_count;
    selection->anchor          = 0;
    selection->cursor          = NK_SINT_MAX;
    selection->anchor_affinity = NK_TEXT_AFFINITY_LEADING;
    selection->affinity        = NK_TEXT_AFFINITY_TRAILING;
  } else if (nk_input_is_key_pressed(input, NK_KEY_LEFT)) {
    context->keyboard_frame = ctx->seq;

    if (!shift && !nk_text_selection_context_empty(context)) {
      nk_text_selection_context_collapse(context, shape, -1);
    } else {
      struct nk_text_cursor cursor;

      cursor.source_offset = selection->cursor;
      cursor.affinity      = (enum nk_text_affinity)selection->affinity;
      cursor               = nk_text_shape_cursor_move(shape, cursor, -1);

      if (cursor.source_offset == selection->cursor && widget > 1) {
        context->cursor_widget = widget - 1;
        selection->cursor      = NK_SINT_MAX;
        selection->affinity    = NK_TEXT_AFFINITY_TRAILING;
      } else {
        selection->cursor   = cursor.source_offset;
        selection->affinity = (unsigned char)cursor.affinity;
      }

      if (!shift) {
        context->anchor_widget     = context->cursor_widget;
        selection->anchor          = selection->cursor;
        selection->anchor_affinity = selection->affinity;
      }
    }
  } else if (nk_input_is_key_pressed(input, NK_KEY_RIGHT)) {
    context->keyboard_frame = ctx->seq;

    if (!shift && !nk_text_selection_context_empty(context)) {
      nk_text_selection_context_collapse(context, shape, 1);
    } else {
      struct nk_text_cursor cursor;

      cursor.source_offset = selection->cursor;
      cursor.affinity      = (enum nk_text_affinity)selection->affinity;
      cursor               = nk_text_shape_cursor_move(shape, cursor, 1);

      if (cursor.source_offset == selection->cursor && widget < context->widget_count) {
        context->cursor_widget = widget + 1;
        selection->cursor      = 0;
        selection->affinity    = NK_TEXT_AFFINITY_LEADING;
      } else {
        selection->cursor   = cursor.source_offset;
        selection->affinity = (unsigned char)cursor.affinity;
      }

      if (!shift) {
        context->anchor_widget     = context->cursor_widget;
        selection->anchor          = selection->cursor;
        selection->anchor_affinity = selection->affinity;
      }
    }
  } else if (nk_input_is_key_pressed(input, NK_KEY_TEXT_WORD_LEFT)) {
    int cursor = selection->cursor;

    context->keyboard_frame = ctx->seq;
    selection->cursor       = nk_text_shape_boundary_previous(shape, selection->cursor, NK_TEXT_BOUNDARY_WORD);
    selection->affinity     = selection->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;

    if (selection->cursor == cursor && widget > 1) {
      context->cursor_widget = widget - 1;
      selection->cursor      = NK_SINT_MAX;
      selection->affinity    = NK_TEXT_AFFINITY_TRAILING;
    }

    if (!shift) {
      context->anchor_widget     = context->cursor_widget;
      selection->anchor          = selection->cursor;
      selection->anchor_affinity = selection->affinity;
    }
  } else if (nk_input_is_key_pressed(input, NK_KEY_TEXT_WORD_RIGHT)) {
    int cursor = selection->cursor;

    context->keyboard_frame = ctx->seq;
    selection->cursor       = nk_text_shape_boundary_next(shape, selection->cursor, NK_TEXT_BOUNDARY_WORD);
    selection->affinity     = selection->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;

    if (selection->cursor == cursor && widget < context->widget_count) {
      context->cursor_widget = widget + 1;
      selection->cursor      = 0;
      selection->affinity    = NK_TEXT_AFFINITY_LEADING;
    }

    if (!shift) {
      context->anchor_widget     = context->cursor_widget;
      selection->anchor          = selection->cursor;
      selection->anchor_affinity = selection->affinity;
    }
  }
}

NK_INTERN nk_bool
nk_text_selection_panel_accepts_input(struct nk_context *ctx, enum nk_widget_layout_states widget_state)
{
  return widget_state != NK_WIDGET_INVALID &&
         widget_state != NK_WIDGET_DISABLED &&
         !(ctx->current->layout->flags & NK_WINDOW_ROM) &&
         !(ctx->current->layout->flags & NK_WINDOW_NO_INPUT) &&
         !ctx->current->widgets_disabled;
}

NK_INTERN nk_bool
nk_text_selectable_widget(struct nk_context *ctx, const char *str, int len, nk_flags align, struct nk_color color, struct nk_text_selection_context *selection_context)
{
  struct nk_window             *win;
  struct nk_style              *style;
  struct nk_text_shape         *shape;
  struct nk_text_selection     *selection;
  struct nk_text                text;
  struct nk_rect                bounds;
  struct nk_rect                label;
  enum nk_widget_layout_states  widget_state;
  unsigned int                  widget;
  unsigned int                  old_anchor_widget;
  unsigned int                  old_cursor_widget;
  nk_bool                       selected_widget;
  int                           selected_begin = 0;
  int                           selected_end   = 0;
  int                           old_anchor;
  int                           old_cursor;

  widget            = nk_text_selection_widget_begin(ctx, selection_context);
  selection         = &selection_context->selection;
  old_anchor_widget = selection_context->anchor_widget;
  old_cursor_widget = selection_context->cursor_widget;
  old_anchor        = selection->anchor;
  old_cursor        = selection->cursor;

  win             = ctx->current;
  style           = &ctx->style;
  widget_state    = nk_widget(&bounds, ctx);
  text.padding    = style->text.padding;
  text.background = style->window.background;
  text.text       = nk_rgb_factor(color, style->text.color_factor);

  shape = nk_text_shape_build_default(ctx, style->font, style->font_size, str, len);
  label = nk_widget_text_layout(ctx, bounds, &text, align, (shape ? shape->advance.x : 0.0f) + 2 * text.padding.x);

  nk_text_selection_clamp(selection_context, widget, shape, len);

  if (widget_state != NK_WIDGET_INVALID) {
    struct nk_input *input             = &ctx->input;
    nk_bool          pressed           = nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT);
    nk_bool          released          = nk_input_is_mouse_released(input, NK_BUTTON_LEFT);
    struct nk_vec2   position          = released ? input->mouse.buttons[NK_BUTTON_LEFT].clicked_pos : input->mouse.pos;
    nk_bool          pressed_on_widget = pressed && nk_input_has_mouse_click_down_in_rect(input, NK_BUTTON_LEFT, bounds, nk_true);
    nk_bool          on_widget         = NK_INBOX(position.x, position.y, bounds.x, bounds.y, bounds.w, bounds.h);
    nk_bool          panel_input       = nk_text_selection_panel_accepts_input(ctx, widget_state);

    if (pressed_on_widget && panel_input && shape) {
      struct nk_text_cursor cursor;

      cursor                           = nk_text_shape_hit_test(shape, input->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x - label.x);
      selection->anchor                = cursor.source_offset;
      selection->cursor                = cursor.source_offset;
      selection->affinity              = (unsigned char)cursor.affinity;
      selection->anchor_affinity       = (unsigned char)cursor.affinity;
      selection->active                = nk_true;
      selection->dragging              = nk_true;
      selection_context->anchor_widget = widget;
      selection_context->cursor_widget = widget;
    }

    if (!pressed && selection->active && selection->dragging && panel_input && on_widget && (input->mouse.buttons[NK_BUTTON_LEFT].down || released) && shape) {
      struct nk_text_cursor cursor;

      cursor                           = nk_text_shape_hit_test(shape, position.x - label.x);
      selection_context->cursor_widget = widget;
      selection->cursor                = cursor.source_offset;
      selection->affinity              = (unsigned char)cursor.affinity;

      if (released) {
        selection->dragging = nk_false;
      }
    }

    if (shape) {
      nk_text_selection_keyboard(ctx, shape, selection_context, widget);
    }
  }

  selected_widget = nk_text_selection_copy(ctx, selection_context, widget, str, len, bounds, &selected_begin, &selected_end);

  nk_draw_text_shaped(&win->buffer, label, str, len, style->font, text.background, text.text, shape);

  if (shape && selected_widget) {
    if (selected_begin != selected_end) {
      struct nk_text_selection_iterator iterator;
      struct nk_text_selection_span     span;

      nk_text_selection_iterator_begin(&iterator, shape, selected_begin, selected_end);

      while (nk_text_selection_iterator_next(&iterator, &span)) {
        struct nk_rect selected = nk_rect(label.x + span.x, label.y, span.width, label.h);
        struct nk_rect old_clip = win->buffer.clip;
        struct nk_rect selected_clip;

        nk_fill_rect(&win->buffer, selected, 0, style->edit.selected_normal);
        nk_unify(&selected_clip, &old_clip, selected.x, selected.y, selected.x + selected.w, selected.y + selected.h);
        nk_push_scissor(&win->buffer, selected_clip);
        nk_draw_text(&win->buffer, label, str, len, style->font, style->edit.selected_normal, style->edit.selected_text_normal);
        nk_push_scissor(&win->buffer, old_clip);
      }
    }
  }
  return old_anchor_widget != selection_context->anchor_widget ||
         old_cursor_widget != selection_context->cursor_widget ||
         old_anchor != selection->anchor || old_cursor != selection->cursor;
}

NK_INTERN int
nk_text_wrap_draw_end(const char *text, struct nk_text_wrap_line line)
{
  while (line.source_end > line.source_begin && (text[line.source_end - 1] == '\r' || text[line.source_end - 1] == '\n')) {
    line.source_end -= 1;
  }
  return line.source_end;
}

NK_INTERN nk_bool
nk_text_selectable_wrap_hit_test(struct nk_context *ctx, const struct nk_text_shape *shape,
                                 const char *str, struct nk_rect bounds, struct nk_vec2 position,
                                 struct nk_text_cursor *cursor)
{
  struct nk_style             *style = &ctx->style;
  struct nk_text_wrap_iterator iterator;
  struct nk_text_wrap_line     wrapped;
  struct nk_rect               line;
  struct nk_rect               target_bounds;
  nk_bool                      found = nk_false;
  float                        bottom;

  cursor->source_offset = 0;
  cursor->affinity      = NK_TEXT_AFFINITY_LEADING;

  if (!shape->source_len) {
    return nk_true;
  }

  line.x = bounds.x + style->text.padding.x;
  line.y = bounds.y + style->text.padding.y;
  line.w = NK_MAX(1.0f, bounds.w - 2.0f * style->text.padding.x);
  line.h = style->font_size + 2.0f * style->text.padding.y;
  bottom = bounds.y + bounds.h - style->text.padding.y;

  nk_text_wrap_iterator_begin(&iterator, shape, line.w);

  while (line.y + line.h <= bottom && nk_text_wrap_iterator_next(&iterator, &wrapped)) {
    found         = nk_true;
    target_bounds = line;

    if (position.y < line.y + line.h) {
      break;
    }

    line.y += line.h;
  }

  if (found) {
    int                   draw_end;
    struct nk_text_shape *line_shape;

    draw_end   = nk_text_wrap_draw_end(str, wrapped);
    line_shape = nk_text_shape_build_default(ctx, style->font, style->font_size, str + wrapped.source_begin, draw_end - wrapped.source_begin);

    if (line_shape) {
      struct nk_text_cursor line_cursor;

      line_cursor           = nk_text_shape_hit_test(line_shape, position.x - target_bounds.x);
      cursor->source_offset = wrapped.source_begin + line_cursor.source_offset;
      cursor->affinity      = line_cursor.affinity;
    }
  }
  return found;
}

NK_INTERN nk_bool
nk_text_shape_is_stable(struct nk_context *ctx, const struct nk_text_shape *shape)
{
  const struct nk_buffer *temporary = ctx->text_backend.temporary;
  const nk_byte          *begin;

  if (!temporary || !temporary->memory.ptr) {
    return nk_true;
  }

  begin = (const nk_byte *)temporary->memory.ptr;
  return (const nk_byte *)shape < begin || (const nk_byte *)shape >= begin + temporary->memory.size;
}

NK_INTERN void
nk_text_selectable_wrap_draw(struct nk_context *ctx, const struct nk_text_shape *shape,
                             const char *str, struct nk_rect bounds, struct nk_color color,
                             nk_bool selected_widget, int selected_begin, int selected_end, nk_bool shape_cached)
{
  struct nk_window             *win   = ctx->current;
  struct nk_style              *style = &ctx->style;
  struct nk_text_wrap_iterator  iterator;
  struct nk_text_wrap_line      wrapped;
  struct nk_rect                line;
  float                         bottom;

  line.x = bounds.x + style->text.padding.x;
  line.y = bounds.y + style->text.padding.y;
  line.w = NK_MAX(1.0f, bounds.w - 2.0f * style->text.padding.x);
  line.h = style->font_size + 2.0f * style->text.padding.y;
  bottom = bounds.y + bounds.h - style->text.padding.y;

  nk_text_wrap_iterator_begin(&iterator, shape, line.w);

  while (line.y + line.h <= bottom && nk_text_wrap_iterator_next(&iterator, &wrapped)) {
    int                   draw_end;
    int                   source_begin;
    int                   line_len;
    struct nk_text_shape *line_shape = 0;

    draw_end     = nk_text_wrap_draw_end(str, wrapped);
    source_begin = wrapped.source_begin;
    line_len     = draw_end - source_begin;

    nk_push_draw_text(&win->buffer, line, str + source_begin, line_len, style->font, style->font_size, style->window.background, color);

    if (selected_widget && shape_cached) {
      line_shape = nk_text_shape_build_default(ctx, style->font, style->font_size, str + source_begin, line_len);
    }

    if (line_shape) {
      int line_begin = NK_CLAMP(source_begin, selected_begin, draw_end) - source_begin;
      int line_end   = NK_CLAMP(source_begin, selected_end, draw_end) - source_begin;

      if (line_begin != line_end) {
        struct nk_text_selection_iterator selection_iterator;
        struct nk_text_selection_span     span;

        nk_text_selection_iterator_begin(&selection_iterator, line_shape, line_begin, line_end);

        while (nk_text_selection_iterator_next(&selection_iterator, &span)) {
          struct nk_rect selected;
          struct nk_rect old_clip;
          struct nk_rect selected_clip;

          selected = nk_rect(line.x + span.x, line.y, span.width, line.h);
          old_clip = win->buffer.clip;

          nk_fill_rect(&win->buffer, selected, 0, style->edit.selected_normal);
          nk_unify(&selected_clip, &old_clip, selected.x, selected.y, selected.x + selected.w, selected.y + selected.h);
          nk_push_scissor(&win->buffer, selected_clip);
          nk_draw_text(&win->buffer, line, str + source_begin, line_len, style->font, style->edit.selected_normal, style->edit.selected_text_normal);
          nk_push_scissor(&win->buffer, old_clip);
        }
      }
    }

    line.y += line.h;
  }
}

NK_INTERN nk_bool
nk_text_selectable_wrap_widget(struct nk_context *ctx, const char *str, int len, struct nk_color color,
                               struct nk_text_selection_context *selection_context)
{
  struct nk_style             *style = &ctx->style;
  struct nk_text_shape        *shape;
  struct nk_text_selection    *selection = &selection_context->selection;
  struct nk_rect               bounds;
  enum nk_widget_layout_states widget_state;
  unsigned int                 widget;
  unsigned int                 old_anchor_widget;
  unsigned int                 old_cursor_widget;
  nk_bool                      selected_widget;
  nk_bool                      shape_invalidated = nk_false;
  nk_bool                      shape_cached;
  int                          selected_begin = 0;
  int                          selected_end   = 0;
  int                          old_anchor;
  int                          old_cursor;

  widget            = nk_text_selection_widget_begin(ctx, selection_context);
  old_anchor_widget = selection_context->anchor_widget;
  old_cursor_widget = selection_context->cursor_widget;
  old_anchor        = selection->anchor;
  old_cursor        = selection->cursor;
  widget_state      = nk_widget(&bounds, ctx);
  shape             = nk_text_shape_build_default(ctx, style->font, style->font_size, str, len);
  shape_cached      = shape && nk_text_shape_is_stable(ctx, shape);

  nk_text_selection_clamp(selection_context, widget, shape, len);

  if (widget_state != NK_WIDGET_INVALID && shape) {
    struct nk_input *input             = &ctx->input;
    nk_bool          pressed           = nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT);
    nk_bool          released          = nk_input_is_mouse_released(input, NK_BUTTON_LEFT);
    struct nk_vec2   position          = released ? input->mouse.buttons[NK_BUTTON_LEFT].clicked_pos : input->mouse.pos;
    nk_bool          pressed_on_widget = pressed && nk_input_has_mouse_click_down_in_rect(input, NK_BUTTON_LEFT, bounds, nk_true);
    nk_bool          on_widget         = NK_INBOX(position.x, position.y, bounds.x, bounds.y, bounds.w, bounds.h);
    nk_bool          panel_input       = nk_text_selection_panel_accepts_input(ctx, widget_state);

    if (pressed_on_widget && panel_input) {
      struct nk_text_cursor cursor;

      nk_text_selectable_wrap_hit_test(ctx, shape, str, bounds, input->mouse.buttons[NK_BUTTON_LEFT].clicked_pos, &cursor);
      shape_invalidated                = nk_true;
      selection->anchor                = cursor.source_offset;
      selection->cursor                = cursor.source_offset;
      selection->affinity              = (unsigned char)cursor.affinity;
      selection->anchor_affinity       = (unsigned char)cursor.affinity;
      selection->active                = nk_true;
      selection->dragging              = nk_true;
      selection_context->anchor_widget = widget;
      selection_context->cursor_widget = widget;
    }

    if (!pressed && selection->active && selection->dragging && panel_input && on_widget && (input->mouse.buttons[NK_BUTTON_LEFT].down || released)) {
      struct nk_text_cursor cursor;

      nk_text_selectable_wrap_hit_test(ctx, shape, str, bounds, position, &cursor);
      shape_invalidated                = nk_true;
      selection_context->cursor_widget = widget;
      selection->cursor                = cursor.source_offset;
      selection->affinity              = (unsigned char)cursor.affinity;

      if (released) {
        selection->dragging = nk_false;
      }
    }

    if (shape_invalidated) {
      shape        = nk_text_shape_build_default(ctx, style->font, style->font_size, str, len);
      shape_cached = shape && nk_text_shape_is_stable(ctx, shape);
    }

    if (shape) {
      nk_text_selection_keyboard(ctx, shape, selection_context, widget);
    }
  }

  selected_widget = nk_text_selection_copy(ctx, selection_context, widget, str, len, bounds, &selected_begin, &selected_end);

  if (shape && widget_state != NK_WIDGET_INVALID) {
    nk_text_selectable_wrap_draw(ctx, shape, str, bounds, nk_rgb_factor(color, style->text.color_factor),
                                 selected_widget, selected_begin, selected_end, shape_cached);
  }
  return old_anchor_widget != selection_context->anchor_widget ||
         old_cursor_widget != selection_context->cursor_widget ||
         old_anchor != selection->anchor || old_cursor != selection->cursor;
}

NK_INTERN void
nk_text_layout_auto_height(struct nk_context *ctx, const char *str, int len)
{
  struct nk_panel             *layout = ctx->current->layout;
  struct nk_text_wrap_iterator iterator;
  struct nk_text_wrap_line     line;
  struct nk_text_shape        *shape;
  struct nk_vec2               padding = ctx->style.text.padding;
  float                        width;
  float                        line_height;
  int                          line_count = 0;

  width = nk_layout_row_calculate_usable_space(&ctx->style, layout->type, layout->bounds.w, 1);
  width = NK_MAX(1.0f, width - 2.0f * padding.x);

  shape = nk_text_shape_build_default(ctx, ctx->style.font, ctx->style.font_size, str, len);

  if (shape) {
    nk_text_wrap_iterator_begin(&iterator, shape, width);

    while (nk_text_wrap_iterator_next(&iterator, &line)) {
      line_count += 1;
    }
  }

  line_count  = NK_MAX(1, line_count);
  line_height = ctx->style.font_size + 2.0f * padding.y;
  nk_layout_row_dynamic(ctx, line_height * (float)line_count + 2.0f * padding.y, 1);
}

NK_API nk_bool
nk_text_ex(struct nk_context *ctx, const char *str, int len, const struct nk_text_options *options)
{
  struct nk_text_selection_context *selection;

  NK_ASSERT(ctx);
  NK_ASSERT(options);

  if (!ctx || !options) {
    return nk_false;
  }

  if ((options->flags & (NK_TEXT_OPTION_WRAP | NK_TEXT_OPTION_AUTO_HEIGHT)) ==
      (NK_TEXT_OPTION_WRAP | NK_TEXT_OPTION_AUTO_HEIGHT)) {
    nk_text_layout_auto_height(ctx, str, len);
  }

  if (options->flags & NK_TEXT_OPTION_SELECTABLE) {
    selection = options->selection ? options->selection : nk_text_selection_panel_context(ctx);

    if (options->flags & NK_TEXT_OPTION_WRAP) {
      return nk_text_selectable_wrap_widget(ctx, str, len, options->color, selection);
    }
    return nk_text_selectable_widget(ctx, str, len, options->alignment, options->color, selection);
  }

  if (options->flags & NK_TEXT_OPTION_WRAP) {
    nk_text_wrap_widget_colored(ctx, str, len, options->color);
  } else {
    nk_text_widget_colored(ctx, str, len, options->alignment, options->color);
  }
  return nk_false;
}

#ifdef NK_INCLUDE_STANDARD_VARARGS
NK_API nk_bool
nk_textfv_ex(struct nk_context *ctx, const struct nk_text_options *options, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_text_ex(ctx, buf, nk_strlen(buf), options);
}

NK_API nk_bool
nk_textf_ex(struct nk_context *ctx, const struct nk_text_options *options, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_ex(ctx, options, fmt, args);
  va_end(args);
  return result;
}
#endif

NK_API nk_bool
nk_text_selectable_ctx(struct nk_context *ctx, const char *str, int len, nk_flags align,
                       struct nk_text_selection_context *selection)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = align;
  options.flags     = NK_TEXT_OPTION_SELECTABLE;
  options.color     = ctx->style.text.color;
  options.selection = selection;
  return nk_text_ex(ctx, str, len, &options);
}

NK_API nk_bool
nk_text_selectable_colored_ctx(struct nk_context *ctx, const char *str, int len, nk_flags align, struct nk_color color,
                               struct nk_text_selection_context *selection)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = align;
  options.flags     = NK_TEXT_OPTION_SELECTABLE;
  options.color     = color;
  options.selection = selection;
  return nk_text_ex(ctx, str, len, &options);
}

NK_API nk_bool
nk_label_selectable_ctx(struct nk_context *ctx, const char *str, nk_flags align,
                        struct nk_text_selection_context *selection)
{
  return nk_text_selectable_ctx(ctx, str, nk_strlen(str), align, selection);
}

NK_API nk_bool
nk_label_selectable_colored_ctx(struct nk_context *ctx, const char *str, nk_flags align, struct nk_color color,
                                struct nk_text_selection_context *selection)
{
  return nk_text_selectable_colored_ctx(ctx, str, nk_strlen(str), align, color, selection);
}

NK_API nk_bool
nk_text_selectable(struct nk_context *ctx, const char *str, int len, nk_flags align)
{
  return nk_text_selectable_ctx(ctx, str, len, align, NULL);
}

NK_API nk_bool
nk_text_selectable_colored(struct nk_context *ctx, const char *str, int len, nk_flags align, struct nk_color color)
{
  return nk_text_selectable_colored_ctx(ctx, str, len, align, color, NULL);
}

NK_API nk_bool
nk_label_selectable(struct nk_context *ctx, const char *str, nk_flags align)
{
  return nk_text_selectable(ctx, str, nk_strlen(str), align);
}

NK_API nk_bool
nk_label_selectable_colored(struct nk_context *ctx, const char *str, nk_flags align, struct nk_color color)
{
  return nk_text_selectable_colored(ctx, str, nk_strlen(str), align, color);
}

NK_API nk_bool
nk_text_selectable_wrap_ctx(struct nk_context *ctx, const char *str, int len, struct nk_text_selection_context *selection)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = NK_TEXT_LEFT;
  options.flags     = NK_TEXT_OPTION_WRAP | NK_TEXT_OPTION_SELECTABLE;
  options.color     = ctx->style.text.color;
  options.selection = selection;
  return nk_text_ex(ctx, str, len, &options);
}

NK_API nk_bool
nk_text_selectable_wrap_colored_ctx(struct nk_context *ctx, const char *str, int len, struct nk_color color, struct nk_text_selection_context *selection)
{
  struct nk_text_options options;

  NK_MEMSET(&options, 0, sizeof(options));
  options.alignment = NK_TEXT_LEFT;
  options.flags     = NK_TEXT_OPTION_WRAP | NK_TEXT_OPTION_SELECTABLE;
  options.color     = color;
  options.selection = selection;
  return nk_text_ex(ctx, str, len, &options);
}

NK_API nk_bool
nk_label_selectable_wrap_ctx(struct nk_context *ctx, const char *str, struct nk_text_selection_context *selection)
{
  return nk_text_selectable_wrap_ctx(ctx, str, nk_strlen(str), selection);
}

NK_API nk_bool
nk_label_selectable_colored_wrap_ctx(struct nk_context *ctx, const char *str, struct nk_color color, struct nk_text_selection_context *selection)
{
  return nk_text_selectable_wrap_colored_ctx(ctx, str, nk_strlen(str), color, selection);
}

NK_API nk_bool
nk_text_selectable_wrap(struct nk_context *ctx, const char *str, int len)
{
  return nk_text_selectable_wrap_ctx(ctx, str, len, NULL);
}

NK_API nk_bool
nk_text_selectable_wrap_colored(struct nk_context *ctx, const char *str, int len, struct nk_color color)
{
  return nk_text_selectable_wrap_colored_ctx(ctx, str, len, color, NULL);
}

NK_API nk_bool
nk_label_selectable_wrap(struct nk_context *ctx, const char *str)
{
  return nk_text_selectable_wrap(ctx, str, nk_strlen(str));
}

NK_API nk_bool
nk_label_selectable_colored_wrap(struct nk_context *ctx, const char *str, struct nk_color color)
{
  return nk_text_selectable_wrap_colored(ctx, str, nk_strlen(str), color);
}

#ifdef NK_INCLUDE_STANDARD_VARARGS
NK_API nk_bool
nk_labelfv_selectable_ctx(struct nk_context *ctx, nk_flags align, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_ctx(ctx, buf, align, selection);
}

NK_API nk_bool
nk_labelfv_selectable_colored_ctx(struct nk_context *ctx, nk_flags align, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_colored_ctx(ctx, buf, align, color, selection);
}

NK_API nk_bool
nk_labelfv_selectable(struct nk_context *ctx, nk_flags align, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable(ctx, buf, align);
}

NK_API nk_bool
nk_labelfv_selectable_colored(struct nk_context *ctx, nk_flags align, struct nk_color color, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_colored(ctx, buf, align, color);
}

NK_API nk_bool
nk_labelf_selectable_ctx(struct nk_context *ctx, nk_flags align, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_ctx(ctx, align, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable_colored_ctx(struct nk_context *ctx, nk_flags align, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_colored_ctx(ctx, align, color, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable(struct nk_context *ctx, nk_flags align, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable(ctx, align, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable_colored(struct nk_context *ctx, nk_flags align, struct nk_color color, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_colored(ctx, align, color, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textfv_selectable_ctx(struct nk_context *ctx, nk_flags align, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_ctx(ctx, align, selection, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable_colored_ctx(struct nk_context *ctx, nk_flags align, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_colored_ctx(ctx, align, color, selection, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable(struct nk_context *ctx, nk_flags align, const char *fmt, va_list args)
{
  return nk_labelfv_selectable(ctx, align, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable_colored(struct nk_context *ctx, nk_flags align, struct nk_color color, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_colored(ctx, align, color, fmt, args);
}

NK_API nk_bool
nk_textf_selectable_ctx(struct nk_context *ctx, nk_flags align, struct nk_text_selection_context *selection,
                        const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_ctx(ctx, align, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable_colored_ctx(struct nk_context *ctx, nk_flags align, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_colored_ctx(ctx, align, color, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable(struct nk_context *ctx, nk_flags align, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable(ctx, align, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable_colored(struct nk_context *ctx, nk_flags align, struct nk_color color, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_colored(ctx, align, color, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelfv_selectable_wrap_ctx(struct nk_context *ctx, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_wrap_ctx(ctx, buf, selection);
}

NK_API nk_bool
nk_labelfv_selectable_colored_wrap_ctx(struct nk_context *ctx, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_colored_wrap_ctx(ctx, buf, color, selection);
}

NK_API nk_bool
nk_labelfv_selectable_wrap(struct nk_context *ctx, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_wrap(ctx, buf);
}

NK_API nk_bool
nk_labelfv_selectable_colored_wrap(struct nk_context *ctx, struct nk_color color, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  return nk_label_selectable_colored_wrap(ctx, buf, color);
}

NK_API nk_bool
nk_labelf_selectable_wrap_ctx(struct nk_context *ctx, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_wrap_ctx(ctx, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable_colored_wrap_ctx(struct nk_context *ctx, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_colored_wrap_ctx(ctx, color, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable_wrap(struct nk_context *ctx, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_wrap(ctx, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_labelf_selectable_colored_wrap(struct nk_context *ctx, struct nk_color color, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_labelfv_selectable_colored_wrap(ctx, color, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textfv_selectable_wrap_ctx(struct nk_context *ctx, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_wrap_ctx(ctx, selection, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable_wrap_colored_ctx(struct nk_context *ctx, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_colored_wrap_ctx(ctx, color, selection, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable_wrap(struct nk_context *ctx, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_wrap(ctx, fmt, args);
}

NK_API nk_bool
nk_textfv_selectable_wrap_colored(struct nk_context *ctx, struct nk_color color, const char *fmt, va_list args)
{
  return nk_labelfv_selectable_colored_wrap(ctx, color, fmt, args);
}

NK_API nk_bool
nk_textf_selectable_wrap_ctx(struct nk_context *ctx, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_wrap_ctx(ctx, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable_wrap_colored_ctx(struct nk_context *ctx, struct nk_color color, struct nk_text_selection_context *selection, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_wrap_colored_ctx(ctx, color, selection, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable_wrap(struct nk_context *ctx, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_wrap(ctx, fmt, args);
  va_end(args);
  return result;
}

NK_API nk_bool
nk_textf_selectable_wrap_colored(struct nk_context *ctx, struct nk_color color, const char *fmt, ...)
{
  nk_bool result;
  va_list args;

  va_start(args, fmt);
  result = nk_textfv_selectable_wrap_colored(ctx, color, fmt, args);
  va_end(args);
  return result;
}
#endif

/* ===============================================================
 *
 *                          IMAGE
 *
 * ===============================================================*/
NK_API nk_handle
nk_handle_ptr(void *ptr)
{
  nk_handle handle = {0};

  handle.ptr = ptr;
  return handle;
}

NK_API nk_handle
nk_handle_id(int id)
{
  nk_handle handle;

  nk_zero_struct(handle);
  handle.id = id;
  return handle;
}

NK_API struct nk_image
nk_subimage_ptr(void *ptr, nk_ushort w, nk_ushort h, struct nk_rect r)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  s.handle.ptr = ptr;
  s.w          = w;
  s.h          = h;
  s.region[0]  = (nk_ushort)r.x;
  s.region[1]  = (nk_ushort)r.y;
  s.region[2]  = (nk_ushort)r.w;
  s.region[3]  = (nk_ushort)r.h;
  return s;
}

NK_API struct nk_image
nk_subimage_id(int id, nk_ushort w, nk_ushort h, struct nk_rect r)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  s.handle.id = id;
  s.w         = w;
  s.h         = h;
  s.region[0] = (nk_ushort)r.x;
  s.region[1] = (nk_ushort)r.y;
  s.region[2] = (nk_ushort)r.w;
  s.region[3] = (nk_ushort)r.h;
  return s;
}

NK_API struct nk_image
nk_subimage_handle(nk_handle handle, nk_ushort w, nk_ushort h, struct nk_rect r)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  s.handle    = handle;
  s.w         = w;
  s.h         = h;
  s.region[0] = (nk_ushort)r.x;
  s.region[1] = (nk_ushort)r.y;
  s.region[2] = (nk_ushort)r.w;
  s.region[3] = (nk_ushort)r.h;
  return s;
}

NK_API struct nk_image
nk_image_handle(nk_handle handle)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  s.handle    = handle;
  s.w         = 0;
  s.h         = 0;
  s.region[0] = 0;
  s.region[1] = 0;
  s.region[2] = 0;
  s.region[3] = 0;
  return s;
}

NK_API struct nk_image
nk_image_ptr(void *ptr)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  NK_ASSERT(ptr);
  s.handle.ptr = ptr;
  s.w          = 0;
  s.h          = 0;
  s.region[0]  = 0;
  s.region[1]  = 0;
  s.region[2]  = 0;
  s.region[3]  = 0;
  return s;
}

NK_API struct nk_image
nk_image_id(int id)
{
  struct nk_image s;

  nk_zero(&s, sizeof(s));
  s.handle.id = id;
  s.w         = 0;
  s.h         = 0;
  s.region[0] = 0;
  s.region[1] = 0;
  s.region[2] = 0;
  s.region[3] = 0;
  return s;
}

NK_API nk_bool
nk_image_is_subimage(const struct nk_image *img)
{
  NK_ASSERT(img);
  return !(img->w == 0 && img->h == 0);
}

NK_API void
nk_image(struct nk_context *ctx, struct nk_image img)
{
  struct nk_window *win;
  struct nk_rect    bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win = ctx->current;

  if (!nk_widget(&bounds, ctx)) {
    return;
  }

  nk_draw_image(&win->buffer, bounds, &img, nk_white);
}

NK_API void
nk_image_color(struct nk_context *ctx, struct nk_image img, struct nk_color col)
{
  struct nk_window *win;
  struct nk_rect    bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win = ctx->current;

  if (!nk_widget(&bounds, ctx)) {
    return;
  }

  nk_draw_image(&win->buffer, bounds, &img, col);
}

/* ===============================================================
 *
 *                          9-SLICE
 *
 * ===============================================================*/
NK_API struct nk_nine_slice
nk_sub9slice_ptr(void *ptr, nk_ushort w, nk_ushort h, struct nk_rect rgn, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  i->handle.ptr = ptr;
  i->w          = w;
  i->h          = h;
  i->region[0]  = (nk_ushort)rgn.x;
  i->region[1]  = (nk_ushort)rgn.y;
  i->region[2]  = (nk_ushort)rgn.w;
  i->region[3]  = (nk_ushort)rgn.h;
  s.l           = l;
  s.t           = t;
  s.r           = r;
  s.b           = b;
  return s;
}

NK_API struct nk_nine_slice
nk_sub9slice_id(int id, nk_ushort w, nk_ushort h, struct nk_rect rgn, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  i->handle.id = id;
  i->w         = w;
  i->h         = h;
  i->region[0] = (nk_ushort)rgn.x;
  i->region[1] = (nk_ushort)rgn.y;
  i->region[2] = (nk_ushort)rgn.w;
  i->region[3] = (nk_ushort)rgn.h;
  s.l          = l;
  s.t          = t;
  s.r          = r;
  s.b          = b;
  return s;
}

NK_API struct nk_nine_slice
nk_sub9slice_handle(nk_handle handle, nk_ushort w, nk_ushort h, struct nk_rect rgn, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  i->handle    = handle;
  i->w         = w;
  i->h         = h;
  i->region[0] = (nk_ushort)rgn.x;
  i->region[1] = (nk_ushort)rgn.y;
  i->region[2] = (nk_ushort)rgn.w;
  i->region[3] = (nk_ushort)rgn.h;
  s.l          = l;
  s.t          = t;
  s.r          = r;
  s.b          = b;
  return s;
}

NK_API struct nk_nine_slice
nk_nine_slice_handle(nk_handle handle, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  i->handle    = handle;
  i->w         = 0;
  i->h         = 0;
  i->region[0] = 0;
  i->region[1] = 0;
  i->region[2] = 0;
  i->region[3] = 0;
  s.l          = l;
  s.t          = t;
  s.r          = r;
  s.b          = b;
  return s;
}

NK_API struct nk_nine_slice
nk_nine_slice_ptr(void *ptr, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  NK_ASSERT(ptr);
  i->handle.ptr = ptr;
  i->w          = 0;
  i->h          = 0;
  i->region[0]  = 0;
  i->region[1]  = 0;
  i->region[2]  = 0;
  i->region[3]  = 0;
  s.l           = l;
  s.t           = t;
  s.r           = r;
  s.b           = b;
  return s;
}

NK_API struct nk_nine_slice
nk_nine_slice_id(int id, nk_ushort l, nk_ushort t, nk_ushort r, nk_ushort b)
{
  struct nk_nine_slice s;
  struct nk_image     *i = &s.img;

  nk_zero(&s, sizeof(s));
  i->handle.id = id;
  i->w         = 0;
  i->h         = 0;
  i->region[0] = 0;
  i->region[1] = 0;
  i->region[2] = 0;
  i->region[3] = 0;
  s.l          = l;
  s.t          = t;
  s.r          = r;
  s.b          = b;
  return s;
}

NK_API int
nk_nine_slice_is_sub9slice(const struct nk_nine_slice *slice)
{
  NK_ASSERT(slice);
  return !(slice->img.w == 0 && slice->img.h == 0);
}

/* ==============================================================
 *
 *                          BUTTON
 *
 * ===============================================================*/
NK_LIB void
nk_draw_symbol(struct nk_command_buffer  *out,
               enum nk_symbol_type        type,
               struct nk_rect             content,
               struct nk_color            background,
               struct nk_color            foreground,
               float                      border_width,
               const struct nk_user_font *font)
{
  switch (type) {
    case NK_SYMBOL_X:
    case NK_SYMBOL_UNDERSCORE:
    case NK_SYMBOL_PLUS:
    case NK_SYMBOL_MINUS: {
      /* single character text symbol */
      const char *X = (type == NK_SYMBOL_X) ? "X" : (type == NK_SYMBOL_UNDERSCORE) ? "_" : (type == NK_SYMBOL_PLUS) ? "+" : "-";

      struct nk_text text;

      text.padding    = nk_vec2(0, 0);
      text.background = background;
      text.text       = foreground;
      nk_widget_text(out, content, X, 1, &text, NK_TEXT_CENTERED, font);
    } break;
    case NK_SYMBOL_CIRCLE_SOLID:
    case NK_SYMBOL_CIRCLE_OUTLINE:
    case NK_SYMBOL_RECT_SOLID:
    case NK_SYMBOL_RECT_OUTLINE: {
      /* simple empty/filled shapes */
      if (type == NK_SYMBOL_RECT_SOLID || type == NK_SYMBOL_RECT_OUTLINE) {
        nk_fill_rect(out, content, 0, foreground);

        if (type == NK_SYMBOL_RECT_OUTLINE) {
          nk_fill_rect(out, nk_shrink_rect(content, border_width), 0, background);
        }
      } else {
        nk_fill_circle(out, content, foreground);

        if (type == NK_SYMBOL_CIRCLE_OUTLINE) {
          nk_fill_circle(out, nk_shrink_rect(content, 1), background);
        }
      }
    } break;
    case NK_SYMBOL_TRIANGLE_UP:
    case NK_SYMBOL_TRIANGLE_DOWN:
    case NK_SYMBOL_TRIANGLE_LEFT:
    case NK_SYMBOL_TRIANGLE_RIGHT: {
      enum nk_heading heading;
      struct nk_vec2  points[3];

      heading = (type == NK_SYMBOL_TRIANGLE_RIGHT) ? NK_RIGHT : (type == NK_SYMBOL_TRIANGLE_LEFT) ? NK_LEFT : (type == NK_SYMBOL_TRIANGLE_UP) ? NK_UP : NK_DOWN;
      nk_triangle_from_direction(points, content, 0, 0, heading);
      nk_fill_triangle(out, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, foreground);
    } break;
    case NK_SYMBOL_TRIANGLE_UP_OUTLINE:
    case NK_SYMBOL_TRIANGLE_DOWN_OUTLINE:
    case NK_SYMBOL_TRIANGLE_LEFT_OUTLINE:
    case NK_SYMBOL_TRIANGLE_RIGHT_OUTLINE: {
      enum nk_heading heading;
      struct nk_vec2  points[3];

      heading = (type == NK_SYMBOL_TRIANGLE_RIGHT_OUTLINE)  ? NK_RIGHT
                : (type == NK_SYMBOL_TRIANGLE_LEFT_OUTLINE) ? NK_LEFT
                : (type == NK_SYMBOL_TRIANGLE_UP_OUTLINE)   ? NK_UP
                                                            : NK_DOWN;
      nk_triangle_from_direction(points, content, 0, 0, heading);
      nk_stroke_triangle(out, points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, border_width, foreground);
    } break;
    default:
    case NK_SYMBOL_NONE:
    case NK_SYMBOL_MAX:
      break;
  }
}

NK_LIB nk_bool
nk_button_behavior(nk_flags *state, struct nk_rect r, const struct nk_input *i, enum nk_button_behavior behavior)
{
  int ret = 0;

  nk_widget_state_reset(state);

  if (!i) {
    return 0;
  }

  if (nk_input_is_mouse_hovering_rect(i, r)) {
    *state = NK_WIDGET_STATE_HOVERED;

    if (nk_input_is_mouse_down(i, NK_BUTTON_LEFT)) {
      *state = NK_WIDGET_STATE_ACTIVE;
    }

    if (nk_input_has_mouse_click_in_button_rect(i, NK_BUTTON_LEFT, r)) {
      ret = (behavior != NK_BUTTON_DEFAULT) ? nk_input_is_mouse_down(i, NK_BUTTON_LEFT) :
#ifdef NK_BUTTON_TRIGGER_ON_RELEASE
                                            nk_input_is_mouse_released(i, NK_BUTTON_LEFT);
#else
                                            nk_input_is_mouse_pressed(i, NK_BUTTON_LEFT);
#endif
    }
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(i, r)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(i, r)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return ret;
}

NK_LIB const struct nk_style_item *
nk_draw_button(struct nk_command_buffer *out, const struct nk_rect *bounds, nk_flags state, const struct nk_style_button *style)
{
  const struct nk_style_item *background;

  if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
  } else {
    background = &style->normal;
  }

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor_background));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor_background));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *bounds, style->rounding, nk_rgb_factor(background->data.color, style->color_factor_background));
      nk_stroke_rect(out, *bounds, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor_background));
      break;
  }
  return background;
}

NK_LIB nk_bool
nk_do_button(nk_flags                     *state,
             struct nk_command_buffer     *out,
             struct nk_rect                r,
             const struct nk_style_button *style,
             const struct nk_input        *in,
             enum nk_button_behavior       behavior,
             struct nk_rect               *content)
{
  struct nk_rect bounds;

  NK_ASSERT(style);
  NK_ASSERT(state);
  NK_ASSERT(out);

  if (!out || !style) {
    return nk_false;
  }

  nk_nav_stop(out->context, r, 0);

  /* calculate button content space */
  content->x = r.x + style->padding.x + style->border + style->rounding;
  content->y = r.y + style->padding.y + style->border + style->rounding;
  content->w = r.w - (2 * (style->padding.x + style->border + style->rounding));
  content->h = r.h - (2 * (style->padding.y + style->border + style->rounding));

  /* execute button behavior */
  bounds.x = r.x - style->touch_padding.x;
  bounds.y = r.y - style->touch_padding.y;
  bounds.w = r.w + 2 * style->touch_padding.x;
  bounds.h = r.h + 2 * style->touch_padding.y;
  return nk_button_behavior(state, bounds, in, behavior);
}

NK_LIB void
nk_draw_button_text(struct nk_command_buffer     *out,
                    const struct nk_rect         *bounds,
                    const struct nk_rect         *content,
                    nk_flags                      state,
                    const struct nk_style_button *style,
                    const char                   *txt,
                    int                           len,
                    nk_flags                      text_alignment,
                    const struct nk_user_font    *font)
{
  struct nk_text              text;
  const struct nk_style_item *background;

  background = nk_draw_button(out, bounds, state, style);

  /* select correct colors/images */
  if (background->type == NK_STYLE_ITEM_COLOR) {
    text.background = background->data.color;
  } else {
    text.background = style->text_background;
  }

  if (state & NK_WIDGET_STATE_HOVER) {
    text.text = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    text.text = style->text_active;
  } else {
    text.text = style->text_normal;
  }

  text.text = nk_rgb_factor(text.text, style->color_factor_text);

  text.padding = nk_vec2(0, 0);
  nk_widget_text(out, *content, txt, len, &text, text_alignment, font);
}

NK_LIB nk_bool
nk_do_button_text(nk_flags                     *state,
                  struct nk_command_buffer     *out,
                  struct nk_rect                bounds,
                  const char                   *string,
                  int                           len,
                  nk_flags                      align,
                  enum nk_button_behavior       behavior,
                  const struct nk_style_button *style,
                  const struct nk_input        *in,
                  const struct nk_user_font    *font)
{
  struct nk_rect content;
  int            ret = nk_false;

  NK_ASSERT(state);
  NK_ASSERT(style);
  NK_ASSERT(out);
  NK_ASSERT(string);
  NK_ASSERT(font);

  if (!out || !style || !font || !string) {
    return nk_false;
  }

  ret = nk_do_button(state, out, bounds, style, in, behavior, &content);

  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_button_text(out, &bounds, &content, *state, style, string, len, align, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return ret;
}

NK_LIB void
nk_draw_button_symbol(struct nk_command_buffer     *out,
                      const struct nk_rect         *bounds,
                      const struct nk_rect         *content,
                      nk_flags                      state,
                      const struct nk_style_button *style,
                      enum nk_symbol_type           type,
                      const struct nk_user_font    *font)
{
  struct nk_color             sym, bg;
  const struct nk_style_item *background;

  /* select correct colors/images */
  background = nk_draw_button(out, bounds, state, style);

  if (background->type == NK_STYLE_ITEM_COLOR) {
    bg = background->data.color;
  } else {
    bg = style->text_background;
  }

  if (state & NK_WIDGET_STATE_HOVER) {
    sym = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    sym = style->text_active;
  } else {
    sym = style->text_normal;
  }

  sym = nk_rgb_factor(sym, style->color_factor_text);
  nk_draw_symbol(out, type, *content, bg, sym, 1, font);
}

NK_LIB nk_bool
nk_do_button_symbol(nk_flags                     *state,
                    struct nk_command_buffer     *out,
                    struct nk_rect                bounds,
                    enum nk_symbol_type           symbol,
                    enum nk_button_behavior       behavior,
                    const struct nk_style_button *style,
                    const struct nk_input        *in,
                    const struct nk_user_font    *font)
{
  int            ret;
  struct nk_rect content;

  NK_ASSERT(state);
  NK_ASSERT(style);
  NK_ASSERT(font);
  NK_ASSERT(out);

  if (!out || !style || !font || !state) {
    return nk_false;
  }

  ret = nk_do_button(state, out, bounds, style, in, behavior, &content);

  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_button_symbol(out, &bounds, &content, *state, style, symbol, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return ret;
}

NK_LIB void
nk_draw_button_image(
  struct nk_command_buffer *out, const struct nk_rect *bounds, const struct nk_rect *content, nk_flags state, const struct nk_style_button *style, const struct nk_image *img)
{
  nk_draw_button(out, bounds, state, style);
  nk_draw_image(out, *content, img, nk_rgb_factor(nk_white, style->color_factor_background));
}

NK_LIB nk_bool
nk_do_button_image(nk_flags                     *state,
                   struct nk_command_buffer     *out,
                   struct nk_rect                bounds,
                   struct nk_image               img,
                   enum nk_button_behavior       b,
                   const struct nk_style_button *style,
                   const struct nk_input        *in)
{
  int            ret;
  struct nk_rect content;

  NK_ASSERT(state);
  NK_ASSERT(style);
  NK_ASSERT(out);

  if (!out || !style || !state) {
    return nk_false;
  }

  ret        = nk_do_button(state, out, bounds, style, in, b, &content);
  content.x += style->image_padding.x;
  content.y += style->image_padding.y;
  content.w -= 2 * style->image_padding.x;
  content.h -= 2 * style->image_padding.y;

  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_button_image(out, &bounds, &content, *state, style, &img);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return ret;
}

NK_LIB void
nk_draw_button_text_symbol(struct nk_command_buffer     *out,
                           const struct nk_rect         *bounds,
                           const struct nk_rect         *label,
                           const struct nk_rect         *symbol,
                           nk_flags                      state,
                           const struct nk_style_button *style,
                           const char                   *str,
                           int                           len,
                           enum nk_symbol_type           type,
                           const struct nk_user_font    *font)
{
  struct nk_color             sym;
  struct nk_text              text;
  const struct nk_style_item *background;

  /* select correct background colors/images */
  background = nk_draw_button(out, bounds, state, style);

  if (background->type == NK_STYLE_ITEM_COLOR) {
    text.background = background->data.color;
  } else {
    text.background = style->text_background;
  }

  /* select correct text colors */
  if (state & NK_WIDGET_STATE_HOVER) {
    sym       = style->text_hover;
    text.text = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    sym       = style->text_active;
    text.text = style->text_active;
  } else {
    sym       = style->text_normal;
    text.text = style->text_normal;
  }

  sym          = nk_rgb_factor(sym, style->color_factor_text);
  text.text    = nk_rgb_factor(text.text, style->color_factor_text);
  text.padding = nk_vec2(0, 0);
  nk_draw_symbol(out, type, *symbol, style->text_background, sym, 0, font);
  nk_widget_text(out, *label, str, len, &text, NK_TEXT_CENTERED, font);
}

NK_LIB nk_bool
nk_do_button_text_symbol(nk_flags                     *state,
                         struct nk_command_buffer     *out,
                         struct nk_rect                bounds,
                         enum nk_symbol_type           symbol,
                         const char                   *str,
                         int                           len,
                         nk_flags                      align,
                         enum nk_button_behavior       behavior,
                         const struct nk_style_button *style,
                         const struct nk_user_font    *font,
                         const struct nk_input        *in)
{
  int            ret;
  struct nk_rect tri = {0, 0, 0, 0};
  struct nk_rect content;

  NK_ASSERT(style);
  NK_ASSERT(out);
  NK_ASSERT(font);

  if (!out || !style || !font) {
    return nk_false;
  }

  ret   = nk_do_button(state, out, bounds, style, in, behavior, &content);
  tri.y = content.y + (content.h / 2) - out->context->style.font_size / 2;
  tri.w = out->context->style.font_size;
  tri.h = out->context->style.font_size;

  if (align & NK_TEXT_ALIGN_LEFT) {
    tri.x = (content.x + content.w) - (2 * style->padding.x + tri.w);
    tri.x = NK_MAX(tri.x, 0);
  } else {
    tri.x = content.x + 2 * style->padding.x;
  }

  /* draw button */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_button_text_symbol(out, &bounds, &content, &tri, *state, style, str, len, symbol, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return ret;
}

NK_LIB void
nk_draw_button_text_image(struct nk_command_buffer     *out,
                          const struct nk_rect         *bounds,
                          const struct nk_rect         *label,
                          const struct nk_rect         *image,
                          nk_flags                      state,
                          const struct nk_style_button *style,
                          const char                   *str,
                          int                           len,
                          const struct nk_user_font    *font,
                          const struct nk_image        *img)
{
  struct nk_text              text;
  const struct nk_style_item *background;

  background = nk_draw_button(out, bounds, state, style);

  /* select correct colors */
  if (background->type == NK_STYLE_ITEM_COLOR) {
    text.background = background->data.color;
  } else {
    text.background = style->text_background;
  }

  if (state & NK_WIDGET_STATE_HOVER) {
    text.text = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    text.text = style->text_active;
  } else {
    text.text = style->text_normal;
  }

  text.text    = nk_rgb_factor(text.text, style->color_factor_text);
  text.padding = nk_vec2(0, 0);
  nk_widget_text(out, *label, str, len, &text, NK_TEXT_CENTERED, font);
  nk_draw_image(out, *image, img, nk_rgb_factor(nk_white, style->color_factor_background));
}

NK_LIB nk_bool
nk_do_button_text_image(nk_flags                     *state,
                        struct nk_command_buffer     *out,
                        struct nk_rect                bounds,
                        struct nk_image               img,
                        const char                   *str,
                        int                           len,
                        nk_flags                      align,
                        enum nk_button_behavior       behavior,
                        const struct nk_style_button *style,
                        const struct nk_user_font    *font,
                        const struct nk_input        *in)
{
  int            ret;
  struct nk_rect icon;
  struct nk_rect content;

  NK_ASSERT(style);
  NK_ASSERT(state);
  NK_ASSERT(font);
  NK_ASSERT(out);

  if (!out || !font || !style || !str) {
    return nk_false;
  }

  ret    = nk_do_button(state, out, bounds, style, in, behavior, &content);
  icon.y = bounds.y + style->padding.y;
  icon.w = icon.h = bounds.h - 2 * style->padding.y;

  if (align & NK_TEXT_ALIGN_LEFT) {
    icon.x = (bounds.x + bounds.w) - (2 * style->padding.x + icon.w);
    icon.x = NK_MAX(icon.x, 0);
  } else {
    icon.x = bounds.x + 2 * style->padding.x;
  }

  icon.x += style->image_padding.x;
  icon.y += style->image_padding.y;
  icon.w -= 2 * style->image_padding.x;
  icon.h -= 2 * style->image_padding.y;

  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_button_text_image(out, &bounds, &content, &icon, *state, style, str, len, font, &img);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return ret;
}

NK_API void
nk_button_set_behavior(struct nk_context *ctx, enum nk_button_behavior behavior)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return;
  }

  ctx->button_behavior = behavior;
}

NK_API nk_bool
nk_button_push_behavior(struct nk_context *ctx, enum nk_button_behavior behavior)
{
  struct nk_config_stack_button_behavior         *button_stack;
  struct nk_config_stack_button_behavior_element *element;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  button_stack = &ctx->stacks.button_behaviors;
  NK_ASSERT(button_stack->head < (int)NK_LEN(button_stack->elements));

  if (button_stack->head >= (int)NK_LEN(button_stack->elements)) {
    return 0;
  }

  element              = &button_stack->elements[button_stack->head++];
  element->address     = &ctx->button_behavior;
  element->old_value   = ctx->button_behavior;
  ctx->button_behavior = behavior;
  return 1;
}

NK_API nk_bool
nk_button_pop_behavior(struct nk_context *ctx)
{
  struct nk_config_stack_button_behavior         *button_stack;
  struct nk_config_stack_button_behavior_element *element;

  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }

  button_stack = &ctx->stacks.button_behaviors;
  NK_ASSERT(button_stack->head > 0);

  if (button_stack->head < 1) {
    return 0;
  }

  element           = &button_stack->elements[--button_stack->head];
  *element->address = element->old_value;
  return 1;
}

NK_API nk_bool
nk_button_text_styled(struct nk_context *ctx, const struct nk_style_button *style, const char *title, int len)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(style);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!style || !ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;
  state  = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_button_text(&ctx->last_widget_state, &win->buffer, bounds, title, len, style->text_alignment, ctx->button_behavior, style, in, ctx->style.font);
}

NK_API nk_bool
nk_button_text(struct nk_context *ctx, const char *title, int len)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }
  return nk_button_text_styled(ctx, &ctx->style.button, title, len);
}

NK_API nk_bool
nk_button_label_styled(struct nk_context *ctx, const struct nk_style_button *style, const char *title)
{
  return nk_button_text_styled(ctx, style, title, nk_strlen(title));
}

NK_API nk_bool
nk_button_label(struct nk_context *ctx, const char *title)
{
  return nk_button_text(ctx, title, nk_strlen(title));
}

NK_API nk_bool
nk_button_color(struct nk_context *ctx, struct nk_color color)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  struct nk_style_button button;

  int                          ret = 0;
  struct nk_rect               bounds;
  struct nk_rect               content;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  button        = ctx->style.button;
  button.normal = nk_style_item_color(color);
  button.hover  = nk_style_item_color(color);
  button.active = nk_style_item_color(color);
  ret           = nk_do_button(&ctx->last_widget_state, &win->buffer, bounds, &button, in, ctx->button_behavior, &content);
  nk_draw_button(&win->buffer, &bounds, ctx->last_widget_state, &button);
  return ret;
}

NK_API nk_bool
nk_button_symbol_styled(struct nk_context *ctx, const struct nk_style_button *style, enum nk_symbol_type symbol)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;
  state  = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_button_symbol(&ctx->last_widget_state, &win->buffer, bounds, symbol, ctx->button_behavior, style, in, ctx->style.font);
}

NK_API nk_bool
nk_button_symbol(struct nk_context *ctx, enum nk_symbol_type symbol)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }
  return nk_button_symbol_styled(ctx, &ctx->style.button, symbol);
}

NK_API nk_bool
nk_button_image_styled(struct nk_context *ctx, const struct nk_style_button *style, struct nk_image img)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_button_image(&ctx->last_widget_state, &win->buffer, bounds, img, ctx->button_behavior, style, in);
}

NK_API nk_bool
nk_button_image(struct nk_context *ctx, struct nk_image img)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }
  return nk_button_image_styled(ctx, &ctx->style.button, img);
}

NK_API nk_bool
nk_button_symbol_text_styled(struct nk_context *ctx, const struct nk_style_button *style, enum nk_symbol_type symbol, const char *text, int len, nk_flags align)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_button_text_symbol(&ctx->last_widget_state, &win->buffer, bounds, symbol, text, len, align, ctx->button_behavior, style, ctx->style.font, in);
}

NK_API nk_bool
nk_button_symbol_text(struct nk_context *ctx, enum nk_symbol_type symbol, const char *text, int len, nk_flags align)
{
  NK_ASSERT(ctx);

  if (!ctx) {
    return 0;
  }
  return nk_button_symbol_text_styled(ctx, &ctx->style.button, symbol, text, len, align);
}

NK_API nk_bool
nk_button_symbol_label(struct nk_context *ctx, enum nk_symbol_type symbol, const char *label, nk_flags align)
{
  return nk_button_symbol_text(ctx, symbol, label, nk_strlen(label), align);
}

NK_API nk_bool
nk_button_symbol_label_styled(struct nk_context *ctx, const struct nk_style_button *style, enum nk_symbol_type symbol, const char *title, nk_flags align)
{
  return nk_button_symbol_text_styled(ctx, style, symbol, title, nk_strlen(title), align);
}

NK_API nk_bool
nk_button_image_text_styled(struct nk_context *ctx, const struct nk_style_button *style, struct nk_image img, const char *text, int len, nk_flags align)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_button_text_image(&ctx->last_widget_state, &win->buffer, bounds, img, text, len, align, ctx->button_behavior, style, ctx->style.font, in);
}

NK_API nk_bool
nk_button_image_text(struct nk_context *ctx, struct nk_image img, const char *text, int len, nk_flags align)
{
  return nk_button_image_text_styled(ctx, &ctx->style.button, img, text, len, align);
}

NK_API nk_bool
nk_button_image_label(struct nk_context *ctx, struct nk_image img, const char *label, nk_flags align)
{
  return nk_button_image_text(ctx, img, label, nk_strlen(label), align);
}

NK_API nk_bool
nk_button_image_label_styled(struct nk_context *ctx, const struct nk_style_button *style, struct nk_image img, const char *label, nk_flags text_alignment)
{
  return nk_button_image_text_styled(ctx, style, img, label, nk_strlen(label), text_alignment);
}

/* ===============================================================
 *
 *                              TOGGLE
 *
 * ===============================================================*/
NK_LIB nk_bool
nk_toggle_behavior(const struct nk_input *in, struct nk_rect select, nk_flags *state, nk_bool active)
{
  nk_widget_state_reset(state);

  if (nk_button_behavior(state, select, in, NK_BUTTON_DEFAULT)) {
    *state = NK_WIDGET_STATE_ACTIVE;
    active = !active;
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, select)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, select)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return active;
}

NK_LIB void
nk_draw_checkbox(struct nk_command_buffer     *out,
                 nk_flags                      state,
                 const struct nk_style_toggle *style,
                 nk_bool                       active,
                 const struct nk_rect         *label,
                 const struct nk_rect         *selector,
                 const struct nk_rect         *cursors,
                 const char                   *string,
                 int                           len,
                 const struct nk_user_font    *font,
                 nk_flags                      text_alignment)
{
  const struct nk_style_item *background;
  const struct nk_style_item *cursor;
  struct nk_text              text;

  /* select correct colors/images */
  if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
    text.text  = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
    text.text  = style->text_active;
  } else {
    background = &style->normal;
    cursor     = &style->cursor_normal;
    text.text  = style->text_normal;
  }

  text.text       = nk_rgb_factor(text.text, style->color_factor);
  text.padding.x  = 0;
  text.padding.y  = 0;
  text.background = style->text_background;
  nk_widget_text(out, *label, string, len, &text, text_alignment, font);

  /* draw background and cursor */
  if (background->type == NK_STYLE_ITEM_COLOR) {
    nk_fill_rect(out, *selector, 0, nk_rgb_factor(style->border_color, style->color_factor));
    nk_fill_rect(out, nk_shrink_rect(*selector, style->border), 0, nk_rgb_factor(background->data.color, style->color_factor));
  } else {
    nk_draw_image(out, *selector, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
  }

  if (active) {
    if (cursor->type == NK_STYLE_ITEM_IMAGE) {
      nk_draw_image(out, *cursors, &cursor->data.image, nk_rgb_factor(nk_white, style->color_factor));
    } else {
      nk_fill_rect(out, *cursors, 0, cursor->data.color);
    }
  }
}

NK_LIB void
nk_draw_option(struct nk_command_buffer     *out,
               nk_flags                      state,
               const struct nk_style_toggle *style,
               nk_bool                       active,
               const struct nk_rect         *label,
               const struct nk_rect         *selector,
               const struct nk_rect         *cursors,
               const char                   *string,
               int                           len,
               const struct nk_user_font    *font,
               nk_flags                      text_alignment)
{
  const struct nk_style_item *background;
  const struct nk_style_item *cursor;
  struct nk_text              text;

  /* select correct colors/images */
  if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
    text.text  = style->text_hover;
  } else if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
    text.text  = style->text_active;
  } else {
    background = &style->normal;
    cursor     = &style->cursor_normal;
    text.text  = style->text_normal;
  }

  text.text       = nk_rgb_factor(text.text, style->color_factor);
  text.padding.x  = 0;
  text.padding.y  = 0;
  text.background = style->text_background;
  nk_widget_text(out, *label, string, len, &text, text_alignment, font);

  /* draw background and cursor */
  if (background->type == NK_STYLE_ITEM_COLOR) {
    nk_fill_circle(out, *selector, nk_rgb_factor(style->border_color, style->color_factor));
    nk_fill_circle(out, nk_shrink_rect(*selector, style->border), nk_rgb_factor(background->data.color, style->color_factor));
  } else {
    nk_draw_image(out, *selector, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
  }

  if (active) {
    if (cursor->type == NK_STYLE_ITEM_IMAGE) {
      nk_draw_image(out, *cursors, &cursor->data.image, nk_rgb_factor(nk_white, style->color_factor));
    } else {
      nk_fill_circle(out, *cursors, cursor->data.color);
    }
  }
}

NK_LIB nk_bool
nk_do_toggle(nk_flags                     *state,
             struct nk_command_buffer     *out,
             struct nk_rect                r,
             nk_bool                      *active,
             const char                   *str,
             int                           len,
             enum nk_toggle_type           type,
             const struct nk_style_toggle *style,
             const struct nk_input        *in,
             const struct nk_user_font    *font,
             nk_flags                      widget_alignment,
             nk_flags                      text_alignment)
{
  int            was_active;
  struct nk_rect bounds;
  struct nk_rect select;
  struct nk_rect cursor;
  struct nk_rect label;

  NK_ASSERT(style);
  NK_ASSERT(out);
  NK_ASSERT(font);

  if (!out || !style || !font || !active) {
    return 0;
  }

  r.w = NK_MAX(r.w, out->context->style.font_size + 2 * style->padding.x);
  r.h = NK_MAX(r.h, out->context->style.font_size + 2 * style->padding.y);

  /* add additional touch padding for touch screen devices */
  bounds.x = r.x - style->touch_padding.x;
  bounds.y = r.y - style->touch_padding.y;
  bounds.w = r.w + 2 * style->touch_padding.x;
  bounds.h = r.h + 2 * style->touch_padding.y;

  /* calculate the selector space */
  select.w = out->context->style.font_size;
  select.h = select.w;

  if (widget_alignment & NK_WIDGET_ALIGN_RIGHT) {
    select.x = r.x + r.w - out->context->style.font_size;

    /* label in front of the selector */
    label.x = r.x;
    label.w = r.w - select.w - style->spacing * 2;
  } else if (widget_alignment & NK_WIDGET_ALIGN_CENTERED) {
    select.x = r.x + (r.w - select.w) / 2;

    /* label in front of selector */
    label.x = r.x;
    label.w = (r.w - select.w - style->spacing * 2) / 2;
  } else { /* Default: NK_WIDGET_ALIGN_LEFT */
    select.x = r.x;

    /* label behind the selector */
    label.x = select.x + select.w + style->spacing;
    label.w = NK_MAX(r.x + r.w, label.x) - label.x;
  }

  if (widget_alignment & NK_WIDGET_ALIGN_TOP) {
    select.y = r.y;
  } else if (widget_alignment & NK_WIDGET_ALIGN_BOTTOM) {
    select.y = r.y + r.h - select.h - 2 * style->padding.y;
  } else { /* Default: NK_WIDGET_ALIGN_MIDDLE */
    select.y = r.y + r.h / 2.0f - select.h / 2.0f;
  }

  label.y = select.y;
  label.h = select.w;

  /* calculate the bounds of the cursor inside the selector */
  cursor.x = select.x + style->padding.x + style->border;
  cursor.y = select.y + style->padding.y + style->border;
  cursor.w = select.w - (2 * style->padding.x + 2 * style->border);
  cursor.h = select.h - (2 * style->padding.y + 2 * style->border);

  /* update selector */
  was_active = *active;
  nk_nav_stop(out->context, r, 0);
  *active = nk_toggle_behavior(in, bounds, state, *active);

  /* draw selector */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  if (type == NK_TOGGLE_CHECK) {
    nk_draw_checkbox(out, *state, style, *active, &label, &select, &cursor, str, len, font, text_alignment);
  } else {
    nk_draw_option(out, *state, style, *active, &label, &select, &cursor, str, len, font, text_alignment);
  }

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return (was_active != *active);
}

/*----------------------------------------------------------------
 *
 *                          CHECKBOX
 *
 * --------------------------------------------------------------*/
NK_API nk_bool
nk_check_text(struct nk_context *ctx, const char *text, int len, nk_bool active)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return active;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return active;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  nk_do_toggle(&ctx->last_widget_state, &win->buffer, bounds, &active, text, len, NK_TOGGLE_CHECK, &style->checkbox, in, style->font, NK_WIDGET_LEFT, NK_TEXT_LEFT);
  return active;
}

NK_API nk_bool
nk_check_text_align(struct nk_context *ctx, const char *text, int len, nk_bool active, nk_flags widget_alignment, nk_flags text_alignment)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return active;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return active;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  nk_do_toggle(&ctx->last_widget_state, &win->buffer, bounds, &active, text, len, NK_TOGGLE_CHECK, &style->checkbox, in, style->font, widget_alignment, text_alignment);
  return active;
}

NK_API unsigned int
nk_check_flags_text(struct nk_context *ctx, const char *text, int len, unsigned int flags, unsigned int value)
{
  int old_active;

  NK_ASSERT(ctx);
  NK_ASSERT(text);

  if (!ctx || !text) {
    return flags;
  }

  old_active = (int)((flags & value) & value);

  if (nk_check_text(ctx, text, len, old_active)) {
    flags |= value;
  } else {
    flags &= ~value;
  }
  return flags;
}

NK_API nk_bool
nk_checkbox_text(struct nk_context *ctx, const char *text, int len, nk_bool *active)
{
  int old_val;

  NK_ASSERT(ctx);
  NK_ASSERT(text);
  NK_ASSERT(active);

  if (!ctx || !text || !active) {
    return 0;
  }

  old_val = *active;
  *active = nk_check_text(ctx, text, len, *active);
  return old_val != *active;
}

NK_API nk_bool
nk_checkbox_text_align(struct nk_context *ctx, const char *text, int len, nk_bool *active, nk_flags widget_alignment, nk_flags text_alignment)
{
  int old_val;

  NK_ASSERT(ctx);
  NK_ASSERT(text);
  NK_ASSERT(active);

  if (!ctx || !text || !active) {
    return 0;
  }

  old_val = *active;
  *active = nk_check_text_align(ctx, text, len, *active, widget_alignment, text_alignment);
  return old_val != *active;
}

NK_API nk_bool
nk_checkbox_flags_text(struct nk_context *ctx, const char *text, int len, unsigned int *flags, unsigned int value)
{
  nk_bool active;

  NK_ASSERT(ctx);
  NK_ASSERT(text);
  NK_ASSERT(flags);

  if (!ctx || !text || !flags) {
    return 0;
  }

  active = (int)((*flags & value) & value);

  if (nk_checkbox_text(ctx, text, len, &active)) {
    if (active) {
      *flags |= value;
    } else {
      *flags &= ~value;
    }
    return 1;
  }
  return 0;
}

NK_API nk_bool
nk_check_label(struct nk_context *ctx, const char *label, nk_bool active)
{
  return nk_check_text(ctx, label, nk_strlen(label), active);
}

NK_API unsigned int
nk_check_flags_label(struct nk_context *ctx, const char *label, unsigned int flags, unsigned int value)
{
  return nk_check_flags_text(ctx, label, nk_strlen(label), flags, value);
}

NK_API nk_bool
nk_checkbox_label(struct nk_context *ctx, const char *label, nk_bool *active)
{
  return nk_checkbox_text(ctx, label, nk_strlen(label), active);
}

NK_API nk_bool
nk_checkbox_label_align(struct nk_context *ctx, const char *label, nk_bool *active, nk_flags widget_alignment, nk_flags text_alignment)
{
  return nk_checkbox_text_align(ctx, label, nk_strlen(label), active, widget_alignment, text_alignment);
}

NK_API nk_bool
nk_checkbox_flags_label(struct nk_context *ctx, const char *label, unsigned int *flags, unsigned int value)
{
  return nk_checkbox_flags_text(ctx, label, nk_strlen(label), flags, value);
}

/*----------------------------------------------------------------
 *
 *                          OPTION
 *
 * --------------------------------------------------------------*/
NK_API nk_bool
nk_option_text(struct nk_context *ctx, const char *text, int len, nk_bool is_active)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return is_active;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return (int)state;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  nk_do_toggle(&ctx->last_widget_state, &win->buffer, bounds, &is_active, text, len, NK_TOGGLE_OPTION, &style->option, in, style->font, NK_WIDGET_LEFT, NK_TEXT_LEFT);
  return is_active;
}

NK_API nk_bool
nk_option_text_align(struct nk_context *ctx, const char *text, int len, nk_bool is_active, nk_flags widget_alignment, nk_flags text_alignment)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return is_active;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return (int)state;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  nk_do_toggle(&ctx->last_widget_state, &win->buffer, bounds, &is_active, text, len, NK_TOGGLE_OPTION, &style->option, in, style->font, widget_alignment, text_alignment);
  return is_active;
}

NK_API nk_bool
nk_radio_text(struct nk_context *ctx, const char *text, int len, nk_bool *active)
{
  int old_value;

  NK_ASSERT(ctx);
  NK_ASSERT(text);
  NK_ASSERT(active);

  if (!ctx || !text || !active) {
    return 0;
  }

  old_value = *active;
  *active   = nk_option_text(ctx, text, len, old_value);
  return old_value != *active;
}

NK_API nk_bool
nk_radio_text_align(struct nk_context *ctx, const char *text, int len, nk_bool *active, nk_flags widget_alignment, nk_flags text_alignment)
{
  int old_value;

  NK_ASSERT(ctx);
  NK_ASSERT(text);
  NK_ASSERT(active);

  if (!ctx || !text || !active) {
    return 0;
  }

  old_value = *active;
  *active   = nk_option_text_align(ctx, text, len, old_value, widget_alignment, text_alignment);
  return old_value != *active;
}

NK_API nk_bool
nk_option_label(struct nk_context *ctx, const char *label, nk_bool active)
{
  return nk_option_text(ctx, label, nk_strlen(label), active);
}

NK_API nk_bool
nk_option_label_align(struct nk_context *ctx, const char *label, nk_bool active, nk_flags widget_alignment, nk_flags text_alignment)
{
  return nk_option_text_align(ctx, label, nk_strlen(label), active, widget_alignment, text_alignment);
}

NK_API nk_bool
nk_radio_label(struct nk_context *ctx, const char *label, nk_bool *active)
{
  return nk_radio_text(ctx, label, nk_strlen(label), active);
}

NK_API nk_bool
nk_radio_label_align(struct nk_context *ctx, const char *label, nk_bool *active, nk_flags widget_alignment, nk_flags text_alignment)
{
  return nk_radio_text_align(ctx, label, nk_strlen(label), active, widget_alignment, text_alignment);
}

/* ===============================================================
 *
 *                              SELECTABLE
 *
 * ===============================================================*/
NK_LIB void
nk_draw_selectable(struct nk_command_buffer         *out,
                   nk_flags                          state,
                   const struct nk_style_selectable *style,
                   nk_bool                           active,
                   const struct nk_rect             *bounds,
                   const struct nk_rect             *icon,
                   const struct nk_image            *img,
                   enum nk_symbol_type               sym,
                   const char                       *string,
                   int                               len,
                   nk_flags                          align,
                   const struct nk_user_font        *font)
{
  const struct nk_style_item *background;
  struct nk_text              text;

  text.padding = style->padding;

  /* select correct colors/images */
  if (!active) {
    if (state & NK_WIDGET_STATE_ACTIVED) {
      background = &style->pressed;
      text.text  = style->text_pressed;
    } else if (state & NK_WIDGET_STATE_HOVER) {
      background = &style->hover;
      text.text  = style->text_hover;
    } else {
      background = &style->normal;
      text.text  = style->text_normal;
    }
  } else {
    if (state & NK_WIDGET_STATE_ACTIVED) {
      background = &style->pressed_active;
      text.text  = style->text_pressed_active;
    } else if (state & NK_WIDGET_STATE_HOVER) {
      background = &style->hover_active;
      text.text  = style->text_hover_active;
    } else {
      background = &style->normal_active;
      text.text  = style->text_normal_active;
    }
  }

  text.text = nk_rgb_factor(text.text, style->color_factor);

  /* draw selectable background and text */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      text.background = background->data.color;
      nk_fill_rect(out, *bounds, style->rounding, background->data.color);
      break;
  }

  if (icon) {
    if (img) {
      nk_draw_image(out, *icon, img, nk_rgb_factor(nk_white, style->color_factor));
    } else {
      nk_draw_symbol(out, sym, *icon, text.background, text.text, 1, font);
    }
  }

  nk_widget_text(out, *bounds, string, len, &text, align, font);
}

NK_LIB nk_bool
nk_do_selectable(nk_flags                         *state,
                 struct nk_command_buffer         *out,
                 struct nk_rect                    bounds,
                 const char                       *str,
                 int                               len,
                 nk_flags                          align,
                 nk_bool                          *value,
                 const struct nk_style_selectable *style,
                 const struct nk_input            *in,
                 const struct nk_user_font        *font)
{
  int            old_value;
  struct nk_rect touch;

  NK_ASSERT(state);
  NK_ASSERT(out);
  NK_ASSERT(str);
  NK_ASSERT(len);
  NK_ASSERT(value);
  NK_ASSERT(style);
  NK_ASSERT(font);

  if (!state || !out || !str || !len || !value || !style || !font) {
    return 0;
  }

  old_value = *value;

  /* remove padding */
  touch.x = bounds.x - style->touch_padding.x;
  touch.y = bounds.y - style->touch_padding.y;
  touch.w = bounds.w + style->touch_padding.x * 2;
  touch.h = bounds.h + style->touch_padding.y * 2;

  /* update button */
  nk_nav_stop(out->context, bounds, 0);

  if (nk_button_behavior(state, touch, in, NK_BUTTON_DEFAULT)) {
    *value = !(*value);
  }

  /* draw selectable */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_selectable(out, *state, style, *value, &bounds, 0, 0, NK_SYMBOL_NONE, str, len, align, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return old_value != *value;
}

NK_LIB nk_bool
nk_do_selectable_image(nk_flags                         *state,
                       struct nk_command_buffer         *out,
                       struct nk_rect                    bounds,
                       const char                       *str,
                       int                               len,
                       nk_flags                          align,
                       nk_bool                          *value,
                       const struct nk_image            *img,
                       const struct nk_style_selectable *style,
                       const struct nk_input            *in,
                       const struct nk_user_font        *font)
{
  nk_bool        old_value;
  struct nk_rect touch;
  struct nk_rect icon;

  NK_ASSERT(state);
  NK_ASSERT(out);
  NK_ASSERT(str);
  NK_ASSERT(len);
  NK_ASSERT(value);
  NK_ASSERT(style);
  NK_ASSERT(font);

  if (!state || !out || !str || !len || !value || !style || !font) {
    return 0;
  }

  old_value = *value;

  /* toggle behavior */
  touch.x = bounds.x - style->touch_padding.x;
  touch.y = bounds.y - style->touch_padding.y;
  touch.w = bounds.w + style->touch_padding.x * 2;
  touch.h = bounds.h + style->touch_padding.y * 2;
  nk_nav_stop(out->context, bounds, 0);

  if (nk_button_behavior(state, touch, in, NK_BUTTON_DEFAULT)) {
    *value = !(*value);
  }

  icon.y = bounds.y + style->padding.y;
  icon.w = icon.h = bounds.h - 2 * style->padding.y;

  if (align & NK_TEXT_ALIGN_LEFT) {
    icon.x = (bounds.x + bounds.w) - (2 * style->padding.x + icon.w);
    icon.x = NK_MAX(icon.x, 0);
  } else {
    icon.x = bounds.x + 2 * style->padding.x;
  }

  icon.x += style->image_padding.x;
  icon.y += style->image_padding.y;
  icon.w -= 2 * style->image_padding.x;
  icon.h -= 2 * style->image_padding.y;

  /* draw selectable */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_selectable(out, *state, style, *value, &bounds, &icon, img, NK_SYMBOL_NONE, str, len, align, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return old_value != *value;
}

NK_LIB nk_bool
nk_do_selectable_symbol(nk_flags                         *state,
                        struct nk_command_buffer         *out,
                        struct nk_rect                    bounds,
                        const char                       *str,
                        int                               len,
                        nk_flags                          align,
                        nk_bool                          *value,
                        enum nk_symbol_type               sym,
                        const struct nk_style_selectable *style,
                        const struct nk_input            *in,
                        const struct nk_user_font        *font)
{
  int            old_value;
  struct nk_rect touch;
  struct nk_rect icon;

  NK_ASSERT(state);
  NK_ASSERT(out);
  NK_ASSERT(str);
  NK_ASSERT(len);
  NK_ASSERT(value);
  NK_ASSERT(style);
  NK_ASSERT(font);

  if (!state || !out || !str || !len || !value || !style || !font) {
    return 0;
  }

  old_value = *value;

  /* toggle behavior */
  touch.x = bounds.x - style->touch_padding.x;
  touch.y = bounds.y - style->touch_padding.y;
  touch.w = bounds.w + style->touch_padding.x * 2;
  touch.h = bounds.h + style->touch_padding.y * 2;
  nk_nav_stop(out->context, bounds, 0);

  if (nk_button_behavior(state, touch, in, NK_BUTTON_DEFAULT)) {
    *value = !(*value);
  }

  icon.y = bounds.y + style->padding.y;
  icon.w = icon.h = bounds.h - 2 * style->padding.y;

  if (align & NK_TEXT_ALIGN_LEFT) {
    icon.x = (bounds.x + bounds.w) - (2 * style->padding.x + icon.w);
    icon.x = NK_MAX(icon.x, 0);
  } else {
    icon.x = bounds.x + 2 * style->padding.x;
  }

  icon.x += style->image_padding.x;
  icon.y += style->image_padding.y;
  icon.w -= 2 * style->image_padding.x;
  icon.h -= 2 * style->image_padding.y;

  /* draw selectable */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_selectable(out, *state, style, *value, &bounds, &icon, 0, sym, str, len, align, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return old_value != *value;
}

NK_API nk_bool
nk_selectable_text(struct nk_context *ctx, const char *str, int len, nk_flags align, nk_bool *value)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  enum nk_widget_layout_states state;
  struct nk_rect               bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(value);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !value) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;
  style  = &ctx->style;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_selectable(&ctx->last_widget_state, &win->buffer, bounds, str, len, align, value, &style->selectable, in, style->font);
}

NK_API nk_bool
nk_selectable_image_text(struct nk_context *ctx, struct nk_image img, const char *str, int len, nk_flags align, nk_bool *value)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  enum nk_widget_layout_states state;
  struct nk_rect               bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(value);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !value) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;
  style  = &ctx->style;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_selectable_image(&ctx->last_widget_state, &win->buffer, bounds, str, len, align, value, &img, &style->selectable, in, style->font);
}

NK_API nk_bool
nk_selectable_symbol_text(struct nk_context *ctx, enum nk_symbol_type sym, const char *str, int len, nk_flags align, nk_bool *value)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_input *in;
  const struct nk_style *style;

  enum nk_widget_layout_states state;
  struct nk_rect               bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(value);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !value) {
    return 0;
  }

  win    = ctx->current;
  layout = win->layout;
  style  = &ctx->style;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_selectable_symbol(&ctx->last_widget_state, &win->buffer, bounds, str, len, align, value, sym, &style->selectable, in, style->font);
}

NK_API nk_bool
nk_selectable_symbol_label(struct nk_context *ctx, enum nk_symbol_type sym, const char *title, nk_flags align, nk_bool *value)
{
  return nk_selectable_symbol_text(ctx, sym, title, nk_strlen(title), align, value);
}

NK_API nk_bool
nk_select_text(struct nk_context *ctx, const char *str, int len, nk_flags align, nk_bool value)
{
  nk_selectable_text(ctx, str, len, align, &value);
  return value;
}

NK_API nk_bool
nk_selectable_label(struct nk_context *ctx, const char *str, nk_flags align, nk_bool *value)
{
  return nk_selectable_text(ctx, str, nk_strlen(str), align, value);
}

NK_API nk_bool
nk_selectable_image_label(struct nk_context *ctx, struct nk_image img, const char *str, nk_flags align, nk_bool *value)
{
  return nk_selectable_image_text(ctx, img, str, nk_strlen(str), align, value);
}

NK_API nk_bool
nk_select_label(struct nk_context *ctx, const char *str, nk_flags align, nk_bool value)
{
  nk_selectable_text(ctx, str, nk_strlen(str), align, &value);
  return value;
}

NK_API nk_bool
nk_select_image_label(struct nk_context *ctx, struct nk_image img, const char *str, nk_flags align, nk_bool value)
{
  nk_selectable_image_text(ctx, img, str, nk_strlen(str), align, &value);
  return value;
}

NK_API nk_bool
nk_select_image_text(struct nk_context *ctx, struct nk_image img, const char *str, int len, nk_flags align, nk_bool value)
{
  nk_selectable_image_text(ctx, img, str, len, align, &value);
  return value;
}

NK_API nk_bool
nk_select_symbol_text(struct nk_context *ctx, enum nk_symbol_type sym, const char *title, int title_len, nk_flags align, nk_bool value)
{
  nk_selectable_symbol_text(ctx, sym, title, title_len, align, &value);
  return value;
}

NK_API nk_bool
nk_select_symbol_label(struct nk_context *ctx, enum nk_symbol_type sym, const char *title, nk_flags align, nk_bool value)
{
  return nk_select_symbol_text(ctx, sym, title, nk_strlen(title), align, value);
}

/* ===============================================================
 *
 *                              SLIDER
 *
 * ===============================================================*/
NK_LIB float
nk_slider_behavior(nk_flags        *state,
                   struct nk_rect  *logical_cursor,
                   struct nk_rect  *visual_cursor,
                   struct nk_input *in,
                   struct nk_rect   bounds,
                   float            slider_min,
                   float            slider_max,
                   float            slider_value,
                   float            slider_step,
                   float            slider_steps)
{
  int left_mouse_down;
  int left_mouse_click_in_cursor;

  /* check if visual cursor is being dragged */
  nk_widget_state_reset(state);
  left_mouse_down            = in && in->mouse.buttons[NK_BUTTON_LEFT].down;
  left_mouse_click_in_cursor = in && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, *visual_cursor, nk_true);

  if (left_mouse_down && left_mouse_click_in_cursor) {
    float       ratio  = 0;
    const float d      = in->mouse.pos.x - (visual_cursor->x + visual_cursor->w * 0.5f);
    const float pxstep = bounds.w / slider_steps;

    /* only update value if the next slider step is reached */
    *state = NK_WIDGET_STATE_ACTIVE;

    if (NK_ABS(d) >= pxstep) {
      const float steps = (float)((int)(NK_ABS(d) / pxstep));

      slider_value                                   += (d > 0) ? (slider_step * steps) : -(slider_step * steps);
      slider_value                                    = NK_CLAMP(slider_min, slider_value, slider_max);
      ratio                                           = (slider_value - slider_min) / slider_step;
      logical_cursor->x                               = bounds.x + (logical_cursor->w * ratio);
      in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x = logical_cursor->x;
    }
  }

  /* slider widget state */
  if (nk_input_is_mouse_hovering_rect(in, bounds)) {
    *state = NK_WIDGET_STATE_HOVERED;
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, bounds)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, bounds)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return slider_value;
}

NK_LIB void
nk_draw_slider(struct nk_command_buffer     *out,
               nk_flags                      state,
               const struct nk_style_slider *style,
               const struct nk_rect         *bounds,
               const struct nk_rect         *visual_cursor,
               float                         min,
               float                         value,
               float                         max)
{
  struct nk_rect              fill;
  struct nk_rect              bar;
  const struct nk_style_item *background;

  /* select correct slider images/colors */
  struct nk_color             bar_color;
  const struct nk_style_item *cursor;

  NK_UNUSED(min);
  NK_UNUSED(max);
  NK_UNUSED(value);

  if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
    bar_color  = style->bar_active;
    cursor     = &style->cursor_active;
  } else if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    bar_color  = style->bar_hover;
    cursor     = &style->cursor_hover;
  } else {
    background = &style->normal;
    bar_color  = style->bar_normal;
    cursor     = &style->cursor_normal;
  }

  /* calculate slider background bar */
  bar.x = bounds->x;
  bar.y = (visual_cursor->y + visual_cursor->h / 2) - bounds->h / 12;
  bar.w = bounds->w;
  bar.h = bounds->h / 6;

  /* filled background bar style */
  fill.w = (visual_cursor->x + (visual_cursor->w / 2.0f)) - bar.x;
  fill.x = bar.x;
  fill.y = bar.y;
  fill.h = bar.h;

  /* draw background */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *bounds, style->rounding, nk_rgb_factor(background->data.color, style->color_factor));
      nk_stroke_rect(out, *bounds, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor));
      break;
  }

  /* draw slider bar */
  nk_fill_rect(out, bar, style->rounding, nk_rgb_factor(bar_color, style->color_factor));
  nk_fill_rect(out, fill, style->rounding, nk_rgb_factor(style->bar_filled, style->color_factor));

  /* draw cursor */
  if (cursor->type == NK_STYLE_ITEM_IMAGE) {
    nk_draw_image(out, *visual_cursor, &cursor->data.image, nk_rgb_factor(nk_white, style->color_factor));
  } else {
    nk_fill_circle(out, *visual_cursor, nk_rgb_factor(cursor->data.color, style->color_factor));
  }
}

NK_LIB float
nk_do_slider(nk_flags                     *state,
             struct nk_command_buffer     *out,
             struct nk_rect                bounds,
             float                         min,
             float                         val,
             float                         max,
             float                         step,
             const struct nk_style_slider *style,
             struct nk_input              *in,
             const struct nk_user_font    *font)
{
  float slider_range;
  float slider_min;
  float slider_max;
  float slider_value;
  float slider_steps;
  float cursor_offset;

  struct nk_rect visual_cursor;
  struct nk_rect logical_cursor;

  NK_ASSERT(style);
  NK_ASSERT(out);

  if (!out || !style) {
    return 0;
  }

  /* remove padding from slider bounds */
  bounds.x  = bounds.x + style->padding.x;
  bounds.y  = bounds.y + style->padding.y;
  bounds.h  = NK_MAX(bounds.h, 2 * style->padding.y);
  bounds.w  = NK_MAX(bounds.w, 2 * style->padding.x + style->cursor_size.x);
  bounds.w -= 2 * style->padding.x;
  bounds.h -= 2 * style->padding.y;

  /* optional buttons */
  if (style->show_buttons) {
    nk_flags       ws;
    struct nk_rect button;

    button.y = bounds.y;
    button.w = bounds.h;
    button.h = bounds.h;

    /* decrement button */
    button.x = bounds.x;

    if (nk_do_button_symbol(&ws, out, button, style->dec_symbol, NK_BUTTON_DEFAULT, &style->dec_button, in, font)) {
      val -= step;
    }

    /* increment button */
    button.x = (bounds.x + bounds.w) - button.w;

    if (nk_do_button_symbol(&ws, out, button, style->inc_symbol, NK_BUTTON_DEFAULT, &style->inc_button, in, font)) {
      val += step;
    }

    bounds.x = bounds.x + button.w + style->spacing.x;
    bounds.w = bounds.w - (2 * button.w + 2 * style->spacing.x);
  }

  /* remove one cursor size to support visual cursor */
  bounds.x += style->cursor_size.x * 0.5f;
  bounds.w -= style->cursor_size.x;

  /* make sure the provided values are correct */
  slider_max    = NK_MAX(min, max);
  slider_min    = NK_MIN(min, max);
  slider_value  = NK_CLAMP(slider_min, val, slider_max);
  slider_range  = slider_max - slider_min;
  slider_steps  = slider_range / step;
  cursor_offset = (slider_value - slider_min) / step;

  /* calculate cursor
  Basically you have two cursors. One for visual representation and interaction
  and one for updating the actual cursor value. */
  logical_cursor.h = bounds.h;
  logical_cursor.w = bounds.w / slider_steps;
  logical_cursor.x = bounds.x + (logical_cursor.w * cursor_offset);
  logical_cursor.y = bounds.y;

  visual_cursor.h = style->cursor_size.y;
  visual_cursor.w = style->cursor_size.x;
  visual_cursor.y = (bounds.y + bounds.h * 0.5f) - visual_cursor.h * 0.5f;
  visual_cursor.x = logical_cursor.x - visual_cursor.w * 0.5f;

  if (nk_nav_stop(out->context, bounds, NK_NAV_STOP_ADJUST | NK_NAV_STOP_NO_CLICK) && out->context->nav.adjust) {
    slider_value = NK_CLAMP(slider_min, slider_value + step * (float)out->context->nav.adjust, slider_max);
  }

  slider_value    = nk_slider_behavior(state, &logical_cursor, &visual_cursor, in, bounds, slider_min, slider_max, slider_value, step, slider_steps);
  visual_cursor.x = logical_cursor.x - visual_cursor.w * 0.5f;

  /* draw slider */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_slider(out, *state, style, &bounds, &visual_cursor, slider_min, slider_value, slider_max);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return slider_value;
}

NK_API nk_bool
nk_slider_float(struct nk_context *ctx, float min_value, float *value, float max_value, float value_step)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  struct nk_input       *in;
  const struct nk_style *style;

  int                          ret = 0;
  float                        old_value;
  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  NK_ASSERT(value);

  if (!ctx || !ctx->current || !ctx->current->layout || !value) {
    return ret;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return ret;
  }

  in = (/*state == NK_WIDGET_ROM || */ state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  old_value = *value;
  *value    = nk_do_slider(&ctx->last_widget_state, &win->buffer, bounds, min_value, old_value, max_value, value_step, &style->slider, in, style->font);
  return (old_value > *value || old_value < *value);
}

NK_API float
nk_slide_float(struct nk_context *ctx, float min, float val, float max, float step)
{
  nk_slider_float(ctx, min, &val, max, step);
  return val;
}

NK_API int
nk_slide_int(struct nk_context *ctx, int min, int val, int max, int step)
{
  float value = (float)val;

  nk_slider_float(ctx, (float)min, &value, (float)max, (float)step);
  return (int)value;
}

NK_API nk_bool
nk_slider_int(struct nk_context *ctx, int min, int *val, int max, int step)
{
  int   ret;
  float value = (float)*val;

  ret  = nk_slider_float(ctx, (float)min, &value, (float)max, (float)step);
  *val = (int)value;
  return ret;
}

/* ===============================================================
 *
 *                               KNOB
 *
 * ===============================================================*/

NK_LIB float
nk_knob_behavior(nk_flags        *state,
                 struct nk_input *in,
                 struct nk_rect   bounds,
                 float            knob_min,
                 float            knob_max,
                 float            knob_value,
                 float            knob_step,
                 float            knob_steps,
                 enum nk_heading  zero_direction,
                 float            dead_zone_percent)
{
  struct nk_vec2 origin;
  float          angle = 0.0f;

  origin.x = bounds.x + (bounds.w / 2);
  origin.y = bounds.y + (bounds.h / 2);

  nk_widget_state_reset(state);

  /* handle click and drag input */
  if (in && in->mouse.buttons[NK_BUTTON_LEFT].down && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, bounds, nk_true)) {
    /* calculate angle from origin and rotate */
    const float direction_rads[4] = {
      NK_PI * 2.5f, /* 90  NK_UP */
      NK_PI * 2.0f, /* 0   NK_RIGHT */
      NK_PI * 1.5f, /* 270 NK_DOWN */
      NK_PI,        /* 180 NK_LEFT */
    };

    *state = NK_WIDGET_STATE_ACTIVE;

    angle  = NK_ATAN2(in->mouse.pos.y - origin.y, in->mouse.pos.x - origin.x) + direction_rads[zero_direction];
    angle -= (angle > NK_PI * 2) ? NK_PI * 3 : NK_PI;

    /* account for dead space applied when drawing */
    angle *= 1.0f / (1.0f - dead_zone_percent);
    angle  = NK_CLAMP(-NK_PI, angle, NK_PI);

    /* convert -pi -> pi range to 0.0 -> 1.0 */
    angle = (angle + NK_PI) / (NK_PI * 2);

    /* click to closest step */
    knob_value = knob_min + ((int)(angle * knob_steps + (knob_step / 2))) * knob_step;
    knob_value = NK_CLAMP(knob_min, knob_value, knob_max);
  }

  /* knob widget state */
  if (nk_input_is_mouse_hovering_rect(in, bounds)) {
    *state = NK_WIDGET_STATE_HOVERED;

    /* handle scroll and arrow inputs */
    if (in->mouse.scroll_delta.y > 0 || (in->keyboard.keys[NK_KEY_UP].down && in->keyboard.keys[NK_KEY_UP].clicked)) {
      knob_value += knob_step;
    }

    if (in->mouse.scroll_delta.y < 0 || (in->keyboard.keys[NK_KEY_DOWN].down && in->keyboard.keys[NK_KEY_DOWN].clicked)) {
      knob_value -= knob_step;
    }

    /* easiest way to disable scrolling of parent panels..knob eats scrolling */
    in->mouse.scroll_delta.y = 0;
    knob_value               = NK_CLAMP(knob_min, knob_value, knob_max);
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, bounds)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, bounds)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }

  return knob_value;
}

NK_LIB void
nk_draw_knob(struct nk_command_buffer   *out,
             nk_flags                    state,
             const struct nk_style_knob *style,
             const struct nk_rect       *bounds,
             float                       min,
             float                       value,
             float                       max,
             enum nk_heading             zero_direction,
             float                       dead_zone_percent)
{
  const struct nk_style_item *background;
  struct nk_color             knob_color, cursor;

  NK_UNUSED(min);
  NK_UNUSED(max);
  NK_UNUSED(value);

  if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
    knob_color = style->knob_active;
    cursor     = style->cursor_active;
  } else if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    knob_color = style->knob_hover;
    cursor     = style->cursor_hover;
  } else {
    background = &style->normal;
    knob_color = style->knob_normal;
    cursor     = style->cursor_normal;
  }

  /* draw background */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *bounds, 0, nk_rgb_factor(background->data.color, style->color_factor));
      nk_stroke_rect(out, *bounds, 0, style->border, nk_rgb_factor(style->border_color, style->color_factor));
      break;
  }

  /* draw knob */
  nk_fill_circle(out, *bounds, nk_rgb_factor(knob_color, style->color_factor));

  if (style->knob_border > 0) {
    struct nk_rect border_bounds = *bounds;

    border_bounds.x += style->knob_border / 2;
    border_bounds.y += style->knob_border / 2;
    border_bounds.w -= style->knob_border;
    border_bounds.h -= style->knob_border;
    nk_stroke_circle(out, border_bounds, style->knob_border, nk_rgb_factor(style->knob_border_color, style->color_factor));
  }

  { /* calculate cursor line cords */
    float          half_circle_size = (bounds->w / 2);
    float          angle            = (value - min) / (max - min);
    float          alive_zone       = 1.0f - dead_zone_percent;
    struct nk_vec2 cursor_start, cursor_end;
    const float    direction_rads[4] = {
      NK_PI * 1.5f, /* 90  NK_UP */
      0.0f,         /* 0   NK_RIGHT */
      NK_PI * 0.5f, /* 270 NK_DOWN */
      NK_PI,        /* 180 NK_LEFT */
    };

    /* calculate + apply dead zone */
    angle = (angle * alive_zone) + (dead_zone_percent / 2);

    /* percentage 0.0 -> 1.0 to radians, rads are 0.0 to (2*pi) NOT -pi to pi */
    angle *= NK_PI * 2;

    /* apply zero angle */
    angle += direction_rads[zero_direction];

    if (angle > NK_PI * 2) {
      angle -= NK_PI * 2;
    }

    cursor_start.x = bounds->x + half_circle_size + (angle > NK_PI);
    cursor_start.y = bounds->y + half_circle_size + (angle < NK_PI_HALF || angle > (NK_PI * 1.5f));

    cursor_end.x = cursor_start.x + (half_circle_size * NK_COS(angle));
    cursor_end.y = cursor_start.y + (half_circle_size * NK_SIN(angle));

    /* cut off half of the cursor */
    cursor_start.x = (cursor_start.x + cursor_end.x) / 2;
    cursor_start.y = (cursor_start.y + cursor_end.y) / 2;

    /* draw cursor */
    nk_stroke_line(out, cursor_start.x, cursor_start.y, cursor_end.x, cursor_end.y, 2, nk_rgb_factor(cursor, style->color_factor));
  }
}

NK_LIB float
nk_do_knob(nk_flags                   *state,
           struct nk_command_buffer   *out,
           struct nk_rect              bounds,
           float                       min,
           float                       val,
           float                       max,
           float                       step,
           enum nk_heading             zero_direction,
           float                       dead_zone_percent,
           const struct nk_style_knob *style,
           struct nk_input            *in)
{
  float knob_range;
  float knob_min;
  float knob_max;
  float knob_value;
  float knob_steps;

  NK_ASSERT(style);
  NK_ASSERT(out);

  if (!out || !style) {
    return 0;
  }

  /* remove padding from knob bounds */
  bounds.y  = bounds.y + style->padding.y;
  bounds.x  = bounds.x + style->padding.x;
  bounds.h  = NK_MAX(bounds.h, 2 * style->padding.y);
  bounds.w  = NK_MAX(bounds.w, 2 * style->padding.x);
  bounds.w -= 2 * style->padding.x;
  bounds.h -= 2 * style->padding.y;

  if (bounds.h < bounds.w) {
    bounds.x += (bounds.w - bounds.h) / 2;
    bounds.w  = bounds.h;
  }

  /* make sure the provided values are correct */
  knob_max   = NK_MAX(min, max);
  knob_min   = NK_MIN(min, max);
  knob_value = NK_CLAMP(knob_min, val, knob_max);
  knob_range = knob_max - knob_min;
  knob_steps = knob_range / step;

  if (nk_nav_stop(out->context, bounds, NK_NAV_STOP_ADJUST | NK_NAV_STOP_NO_CLICK) && out->context->nav.adjust) {
    knob_value = NK_CLAMP(knob_min, knob_value + step * (float)out->context->nav.adjust, knob_max);
  }

  knob_value = nk_knob_behavior(state, in, bounds, knob_min, knob_max, knob_value, step, knob_steps, zero_direction, dead_zone_percent);

  /* draw knob */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_knob(out, *state, style, &bounds, knob_min, knob_value, knob_max, zero_direction, dead_zone_percent);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return knob_value;
}

NK_API nk_bool
nk_knob_float(struct nk_context *ctx, float min_value, float *value, float max_value, float value_step, enum nk_heading zero_direction, float dead_zone_degrees)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  struct nk_input       *in;
  const struct nk_style *style;

  int                          ret = 0;
  float                        old_value;
  struct nk_rect               bounds;
  enum nk_widget_layout_states state;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  NK_ASSERT(value);
  NK_ASSERT(NK_BETWEEN(dead_zone_degrees, 0.0f, 360.0f));

  if (!ctx || !ctx->current || !ctx->current->layout || !value) {
    return ret;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;

  state = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return ret;
  }

  in = (state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;

  old_value = *value;
  *value    = nk_do_knob(&ctx->last_widget_state, &win->buffer, bounds, min_value, old_value, max_value, value_step, zero_direction, dead_zone_degrees / 360.0f, &style->knob, in);

  return (old_value > *value || old_value < *value);
}

NK_API nk_bool
nk_knob_int(struct nk_context *ctx, int min, int *val, int max, int step, enum nk_heading zero_direction, float dead_zone_degrees)
{
  int   ret;
  float value = (float)*val;

  ret  = nk_knob_float(ctx, (float)min, &value, (float)max, (float)step, zero_direction, dead_zone_degrees);
  *val = (int)value;
  return ret;
}

/* ===============================================================
 *
 *                          PROGRESS
 *
 * ===============================================================*/
NK_LIB nk_size
nk_progress_behavior(nk_flags *state, struct nk_input *in, struct nk_rect r, struct nk_rect cursor, nk_size max, nk_size value, nk_bool modifiable)
{
  int left_mouse_down            = 0;
  int left_mouse_click_in_cursor = 0;

  nk_widget_state_reset(state);

  if (!in || !modifiable) {
    return value;
  }

  left_mouse_down            = in && in->mouse.buttons[NK_BUTTON_LEFT].down;
  left_mouse_click_in_cursor = in && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, cursor, nk_true);

  if (nk_input_is_mouse_hovering_rect(in, r)) {
    *state = NK_WIDGET_STATE_HOVERED;
  }

  if (in && left_mouse_down && left_mouse_click_in_cursor) {
    if (left_mouse_down && left_mouse_click_in_cursor) {
      float ratio = NK_MAX(0, (float)(in->mouse.pos.x - cursor.x)) / (float)cursor.w;

      value                                           = (nk_size)NK_CLAMP(0, (float)max * ratio, (float)max);
      in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x = cursor.x + cursor.w / 2.0f;
      *state                                         |= NK_WIDGET_STATE_ACTIVE;
    }
  }

  /* set progressbar widget state */
  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, r)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, r)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return value;
}

NK_LIB void
nk_draw_progress(
  struct nk_command_buffer *out, nk_flags state, const struct nk_style_progress *style, const struct nk_rect *bounds, const struct nk_rect *scursor, nk_size value, nk_size max)
{
  const struct nk_style_item *background;
  const struct nk_style_item *cursor;

  NK_UNUSED(max);
  NK_UNUSED(value);

  /* select correct colors/images to draw */
  if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
    cursor     = &style->cursor_active;
  } else if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
  } else {
    background = &style->normal;
    cursor     = &style->cursor_normal;
  }

  /* draw background */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *bounds, style->rounding, nk_rgb_factor(background->data.color, style->color_factor));
      nk_stroke_rect(out, *bounds, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor));
      break;
  }

  /* draw cursor */
  switch (cursor->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *scursor, &cursor->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *scursor, &cursor->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *scursor, style->rounding, nk_rgb_factor(cursor->data.color, style->color_factor));
      nk_stroke_rect(out, *scursor, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor));
      break;
  }
}

NK_LIB nk_size
nk_do_progress(
  nk_flags *state, struct nk_command_buffer *out, struct nk_rect bounds, nk_size value, nk_size max, nk_bool modifiable, const struct nk_style_progress *style, struct nk_input *in)
{
  float          prog_scale;
  nk_size        prog_value;
  struct nk_rect cursor;

  NK_ASSERT(style);
  NK_ASSERT(out);

  if (!out || !style) {
    return 0;
  }

  /* calculate progressbar cursor */
  cursor.w   = NK_MAX(bounds.w, 2 * style->padding.x + 2 * style->border);
  cursor.h   = NK_MAX(bounds.h, 2 * style->padding.y + 2 * style->border);
  cursor     = nk_pad_rect(bounds, nk_vec2(style->padding.x + style->border, style->padding.y + style->border));
  prog_scale = (float)value / (float)max;

  /* update progressbar */
  prog_value = NK_MIN(value, max);

  if (modifiable && nk_nav_stop(out->context, bounds, NK_NAV_STOP_ADJUST | NK_NAV_STOP_NO_CLICK) && out->context->nav.adjust) {
    nk_size step = NK_MAX(max / 100, (nk_size)1) * (nk_size)NK_ABS(out->context->nav.adjust);

    prog_value = out->context->nav.adjust < 0 ? (prog_value > step ? prog_value - step : 0) : NK_MIN(prog_value + step, max);
  }

  prog_value = nk_progress_behavior(state, in, bounds, cursor, max, prog_value, modifiable);
  cursor.w   = cursor.w * prog_scale;

  /* draw progressbar */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_progress(out, *state, style, &bounds, &cursor, value, max);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return prog_value;
}

NK_API nk_bool
nk_progress(struct nk_context *ctx, nk_size *cur, nk_size max, nk_bool is_modifyable)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_style *style;
  struct nk_input       *in;

  struct nk_rect               bounds;
  enum nk_widget_layout_states state;
  nk_size                      old_value;

  NK_ASSERT(ctx);
  NK_ASSERT(cur);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !cur) {
    return 0;
  }

  win    = ctx->current;
  style  = &ctx->style;
  layout = win->layout;
  state  = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in        = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  old_value = *cur;
  *cur      = nk_do_progress(&ctx->last_widget_state, &win->buffer, bounds, *cur, max, is_modifyable, &style->progress, in);
  return (*cur != old_value);
}

NK_API nk_size
nk_prog(struct nk_context *ctx, nk_size cur, nk_size max, nk_bool modifyable)
{
  nk_progress(ctx, &cur, max, modifyable);
  return cur;
}

/* ===============================================================
 *
 *                              SCROLLBAR
 *
 * ===============================================================*/
NK_LIB float
nk_scrollbar_behavior(nk_flags             *state,
                      struct nk_input      *in,
                      int                   has_scrolling,
                      const struct nk_rect *scroll,
                      const struct nk_rect *cursor,
                      const struct nk_rect *empty0,
                      const struct nk_rect *empty1,
                      float                 scroll_offset,
                      float                 max_offset,
                      float                 scroll_step,
                      float                 cursor_travel,
                      enum nk_orientation   o)
{
  nk_flags     ws = 0;
  int          left_mouse_down;
  unsigned int left_mouse_clicked;
  int          left_mouse_click_in_cursor;
  float        scroll_delta;
  nk_bool      shift_down;

  nk_widget_state_reset(state);

  if (!in) {
    return scroll_offset;
  }

  left_mouse_down            = in->mouse.buttons[NK_BUTTON_LEFT].down;
  left_mouse_clicked         = in->mouse.buttons[NK_BUTTON_LEFT].clicked;
  left_mouse_click_in_cursor = nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, *cursor, nk_true);

  if (nk_input_is_mouse_hovering_rect(in, *scroll)) {
    *state = NK_WIDGET_STATE_HOVERED;
  }

  shift_down = nk_input_is_key_down(in, NK_KEY_SHIFT);

  if (o == NK_VERTICAL) {
    scroll_delta = shift_down ? 0.0f : in->mouse.scroll_delta.y;
  } else {
    scroll_delta = in->mouse.scroll_delta.x;

    if (scroll_delta == 0.0f && shift_down) {
      scroll_delta = in->mouse.scroll_delta.y;
    }
  }

  if (left_mouse_down && left_mouse_click_in_cursor && !left_mouse_clicked) {
    float pixel;
    float delta;
    float old_offset = scroll_offset;

    *state = NK_WIDGET_STATE_ACTIVE;

    if (o == NK_VERTICAL) {
      pixel = in->mouse.delta.y;
    } else {
      pixel = in->mouse.delta.x;
    }

    if (cursor_travel > 0.0f && max_offset > 0.0f) {
      delta         = (pixel / cursor_travel) * max_offset;
      scroll_offset = NK_CLAMP(0.0f, scroll_offset + delta, max_offset);
    }

    if (max_offset > 0.0f && cursor_travel > 0.0f) {
      float cursor_delta = ((scroll_offset - old_offset) / max_offset) * cursor_travel;

      if (o == NK_VERTICAL) {
        in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.y = cursor->y + cursor_delta + cursor->h * 0.5f;
      } else {
        in->mouse.buttons[NK_BUTTON_LEFT].clicked_pos.x = cursor->x + cursor_delta + cursor->w * 0.5f;
      }
    }
  } else if ((nk_input_is_key_pressed(in, NK_KEY_SCROLL_UP) && o == NK_VERTICAL && has_scrolling) || nk_button_behavior(&ws, *empty0, in, NK_BUTTON_DEFAULT)) {
    /* scroll page up by click on empty space or shortcut */
    if (o == NK_VERTICAL) {
      scroll_offset = NK_CLAMP(0, scroll_offset - scroll->h, max_offset);
    } else {
      scroll_offset = NK_CLAMP(0, scroll_offset - scroll->w, max_offset);
    }
  } else if ((nk_input_is_key_pressed(in, NK_KEY_SCROLL_DOWN) && o == NK_VERTICAL && has_scrolling) || nk_button_behavior(&ws, *empty1, in, NK_BUTTON_DEFAULT)) {
    /* scroll page down by click on empty space or shortcut */
    if (o == NK_VERTICAL) {
      scroll_offset = NK_CLAMP(0, scroll_offset + scroll->h, max_offset);
    } else {
      scroll_offset = NK_CLAMP(0, scroll_offset + scroll->w, max_offset);
    }
  } else if (has_scrolling) {
    if (scroll_delta != 0.0f && max_offset > 0.0f) {
      scroll_offset = NK_CLAMP(0.0f, scroll_offset + scroll_step * (-scroll_delta), max_offset);
    } else if (nk_input_is_key_pressed(in, NK_KEY_SCROLL_START)) {
      scroll_offset = 0.0f;
    } else if (nk_input_is_key_pressed(in, NK_KEY_SCROLL_END)) {
      scroll_offset = max_offset;
    }
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, *scroll)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, *scroll)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return scroll_offset;
}

NK_LIB void
nk_draw_scrollbar(struct nk_command_buffer *out, nk_flags state, const struct nk_style_scrollbar *style, const struct nk_rect *bounds, const struct nk_rect *scroll)
{
  const struct nk_style_item *background;
  const struct nk_style_item *cursor;

  /* select correct colors/images to draw */
  if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
    cursor     = &style->cursor_active;
  } else if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    cursor     = &style->cursor_hover;
  } else {
    background = &style->normal;
    cursor     = &style->cursor_normal;
  }

  /* draw background */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *bounds, &background->data.image, nk_white);
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_white);
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *bounds, style->rounding, background->data.color);
      nk_stroke_rect(out, *bounds, style->rounding, style->border, style->border_color);
      break;
  }

  /* draw cursor */
  switch (cursor->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(out, *scroll, &cursor->data.image, nk_white);
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(out, *scroll, &cursor->data.slice, nk_white);
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(out, *scroll, style->rounding_cursor, cursor->data.color);
      nk_stroke_rect(out, *scroll, style->rounding_cursor, style->border_cursor, style->cursor_border_color);
      break;
  }
}

NK_LIB float
nk_scrollbar_cursor_size(float track_size, float view_size, float target_size, float min_size)
{
  float size = 0.0f;

  if (target_size > 0.0f) {
    size = (view_size / target_size) * track_size;
  }

  size = NK_MAX(size, min_size);
  size = NK_CLAMP(0.0f, size, track_size);
  return size;
}

NK_LIB float
nk_scrollbar_cursor_pos(float track_pos, float track_size, float cursor_size, float offset, float max_offset)
{
  float travel = NK_MAX(track_size - cursor_size, 0.0f);

  if (max_offset <= 0.0f || travel <= 0.0f) {
    return track_pos;
  }

  offset = NK_CLAMP(0.0f, offset, max_offset);
  return track_pos + (offset / max_offset) * travel;
}

NK_LIB float
nk_do_scrollbarv(nk_flags                        *state,
                 struct nk_command_buffer        *out,
                 struct nk_rect                   scroll,
                 int                              has_scrolling,
                 float                            offset,
                 float                            target,
                 float                            step,
                 float                            button_pixel_inc,
                 const struct nk_style_scrollbar *style,
                 struct nk_input                 *in,
                 const struct nk_user_font       *font)
{
  struct nk_rect empty_north;
  struct nk_rect empty_south;
  struct nk_rect cursor;

  float track_y;
  float track_h;
  float max_offset;

  float scroll_step;
  float scroll_offset;

  NK_ASSERT(out);
  NK_ASSERT(style);
  NK_ASSERT(state);

  if (!out || !style) {
    return 0;
  }

  scroll.w = NK_MAX(scroll.w, 1);
  scroll.h = NK_MAX(scroll.h, 0);

  if (target <= scroll.h) {
    return 0;
  }

  /* optional scrollbar buttons */
  if (style->show_buttons) {
    nk_flags       ws;
    float          scroll_h;
    struct nk_rect button;

    button.x = scroll.x;
    button.w = scroll.w;
    button.h = scroll.w;

    scroll_h    = NK_MAX(scroll.h - 2 * button.h, 0);
    scroll_step = NK_MIN(step, button_pixel_inc);

    /* decrement button */
    button.y = scroll.y;

    if (nk_do_button_symbol(&ws, out, button, style->dec_symbol, NK_BUTTON_REPEATER, &style->dec_button, in, font)) {
      offset = offset - scroll_step;
    }

    /* increment button */
    button.y = scroll.y + scroll.h - button.h;

    if (nk_do_button_symbol(&ws, out, button, style->inc_symbol, NK_BUTTON_REPEATER, &style->inc_button, in, font)) {
      offset = offset + scroll_step;
    }

    scroll.y = scroll.y + button.h;
    scroll.h = scroll_h;
  }

  /* calculate scrollbar constants */
  track_y       = scroll.y + style->border + style->padding.y;
  track_h       = NK_MAX(scroll.h - 2.0f * (style->border + style->padding.y), 0.0f);
  max_offset    = NK_MAX(target - scroll.h, 0.0f);
  scroll_step   = NK_MIN(step, scroll.h);
  scroll_offset = NK_CLAMP(0, offset, max_offset);

  /* calculate scrollbar cursor bounds */
  cursor.h = nk_scrollbar_cursor_size(track_h, scroll.h, target, style->cursor_min_size);
  cursor.y = nk_scrollbar_cursor_pos(track_y, track_h, cursor.h, scroll_offset, max_offset);
  cursor.w = NK_MAX(scroll.w - 2.0f * (style->border + style->padding.x), 0.0f);
  cursor.x = scroll.x + style->border + style->padding.x;

  /* calculate empty space around cursor */
  empty_north.x = scroll.x;
  empty_north.y = scroll.y;
  empty_north.w = scroll.w;
  empty_north.h = NK_MAX(cursor.y - scroll.y, 0);

  empty_south.x = scroll.x;
  empty_south.y = cursor.y + cursor.h;
  empty_south.w = scroll.w;
  empty_south.h = NK_MAX((scroll.y + scroll.h) - (cursor.y + cursor.h), 0);

  /* update scrollbar */
  scroll_offset = nk_scrollbar_behavior(
    state, in, has_scrolling, &scroll, &cursor, &empty_north, &empty_south, scroll_offset, max_offset, scroll_step, NK_MAX(track_h - cursor.h, 0.0f), NK_VERTICAL);
  cursor.y = nk_scrollbar_cursor_pos(track_y, track_h, cursor.h, scroll_offset, max_offset);

  /* draw scrollbar */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_scrollbar(out, *state, style, &scroll, &cursor);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return scroll_offset;
}

NK_LIB float
nk_do_scrollbarh(nk_flags                        *state,
                 struct nk_command_buffer        *out,
                 struct nk_rect                   scroll,
                 int                              has_scrolling,
                 float                            offset,
                 float                            target,
                 float                            step,
                 float                            button_pixel_inc,
                 const struct nk_style_scrollbar *style,
                 struct nk_input                 *in,
                 const struct nk_user_font       *font)
{
  struct nk_rect cursor;
  struct nk_rect empty_west;
  struct nk_rect empty_east;

  float track_x;
  float track_w;
  float max_offset;

  float scroll_step;
  float scroll_offset;

  NK_ASSERT(out);
  NK_ASSERT(style);

  if (!out || !style) {
    return 0;
  }

  /* scrollbar background */
  scroll.h = NK_MAX(scroll.h, 1);
  scroll.w = NK_MAX(scroll.w, 2 * scroll.h);

  if (target <= scroll.w) {
    return 0;
  }

  /* optional scrollbar buttons */
  if (style->show_buttons) {
    nk_flags       ws;
    float          scroll_w;
    struct nk_rect button;

    button.y = scroll.y;
    button.w = scroll.h;
    button.h = scroll.h;

    scroll_w    = scroll.w - 2 * button.w;
    scroll_step = NK_MIN(step, button_pixel_inc);

    /* decrement button */
    button.x = scroll.x;

    if (nk_do_button_symbol(&ws, out, button, style->dec_symbol, NK_BUTTON_REPEATER, &style->dec_button, in, font)) {
      offset = offset - scroll_step;
    }

    /* increment button */
    button.x = scroll.x + scroll.w - button.w;

    if (nk_do_button_symbol(&ws, out, button, style->inc_symbol, NK_BUTTON_REPEATER, &style->inc_button, in, font)) {
      offset = offset + scroll_step;
    }

    scroll.x = scroll.x + button.w;
    scroll.w = scroll_w;
  }

  /* calculate scrollbar constants */
  track_x       = scroll.x + style->border + style->padding.x;
  track_w       = NK_MAX(scroll.w - 2.0f * (style->border + style->padding.x), 0.0f);
  max_offset    = NK_MAX(target - scroll.w, 0.0f);
  scroll_step   = NK_MIN(step, scroll.w);
  scroll_offset = NK_CLAMP(0.0f, offset, max_offset);

  /* calculate cursor bounds */
  cursor.w = nk_scrollbar_cursor_size(track_w, scroll.w, target, style->cursor_min_size);
  cursor.x = nk_scrollbar_cursor_pos(track_x, track_w, cursor.w, scroll_offset, max_offset);
  cursor.h = NK_MAX(scroll.h - 2.0f * (style->border + style->padding.y), 0.0f);
  cursor.y = scroll.y + style->border + style->padding.y;

  /* calculate empty space around cursor */
  empty_west.x = scroll.x;
  empty_west.y = scroll.y;
  empty_west.w = cursor.x - scroll.x;
  empty_west.h = scroll.h;

  empty_east.x = cursor.x + cursor.w;
  empty_east.y = scroll.y;
  empty_east.w = (scroll.x + scroll.w) - (cursor.x + cursor.w);
  empty_east.h = scroll.h;

  /* update scrollbar */
  scroll_offset = nk_scrollbar_behavior(
    state, in, has_scrolling, &scroll, &cursor, &empty_west, &empty_east, scroll_offset, max_offset, scroll_step, NK_MAX(track_w - cursor.w, 0.0f), NK_HORIZONTAL);
  cursor.x = nk_scrollbar_cursor_pos(track_x, track_w, cursor.w, scroll_offset, max_offset);

  /* draw scrollbar */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_scrollbar(out, *state, style, &scroll, &cursor);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }
  return scroll_offset;
}

/* ===============================================================
 *
 *                          TEXT EDITOR
 *
 * ===============================================================*/
/* stb_textedit.h - v1.8  - public domain - Sean Barrett */
struct nk_text_find {
  float x, y;               /* position of n'th character */
  float height;             /* height of line */
  int   first_char, length; /* first char of row, and length */
  int   prev_first;         /*_ first char of previous row */
};

struct nk_text_edit_row {
  float x0, x1;
  /* starting x location, end x location (allows for align=right, etc) */
  float baseline_y_delta;
  /* position of baseline relative to previous row's baseline*/
  float ymin, ymax;
  /* height of row above and below baseline */
  int   num_chars;
};

/* forward declarations */
NK_INTERN void
nk_textedit_makeundo_delete(struct nk_text_edit *, int, int);
NK_INTERN void
nk_textedit_makeundo_insert(struct nk_text_edit *, int, int);
NK_INTERN void
nk_textedit_makeundo_replace(struct nk_text_edit *, int, int, int);

#define NK_TEXT_HAS_SELECTION(s) ((s)->select_start != (s)->select_end)

NK_INTERN int
nk_textedit_byte_offset(const struct nk_text_edit *edit, int rune_offset)
{
  const char *text;
  nk_rune     rune;
  int         len;

  text = nk_str_get_const(&edit->string);

  if (rune_offset >= edit->string.len) {
    return nk_str_len_char(&edit->string);
  }
  return (int)(nk_str_at_const(&edit->string, rune_offset, &rune, &len) - text);
}

NK_INTERN int
nk_textedit_rune_offset(const struct nk_text_edit *edit, int byte_offset)
{
  return nk_utf_len(nk_str_get_const(&edit->string), byte_offset);
}

NK_INTERN struct nk_text_shape *
nk_textedit_line_shape(struct nk_text_edit *edit, int line_start, const struct nk_user_font *font)
{
  const char *text;
  const char *begin;
  const char *line_end;
  const char *text_end;

  text = nk_str_get_const(&edit->string);

  if (!text) {
    return NULL;
  }

  begin    = text + nk_textedit_byte_offset(edit, line_start);
  line_end = begin;
  text_end = text + nk_str_len_char(&edit->string);

  while (line_end < text_end && *line_end != '\n') {
    line_end += 1;
  }
  return nk_text_shape_build_default(edit->context, font, edit->context->style.font_size, begin, (int)(line_end - begin));
}

NK_INTERN void
nk_textedit_layout_row(struct nk_text_edit_row *r, struct nk_text_edit *edit, int line_start_id, float row_height, const struct nk_user_font *font)
{
  int            l;
  int            glyphs = 0;
  nk_rune        unicode;
  const char    *remaining;
  int            len;
  const char    *end;
  const char    *text;
  struct nk_vec2 size;

  len = nk_str_len_char(&edit->string);
  end = nk_str_get_const(&edit->string);

  r->x0               = 0.0f;
  r->x1               = 0.0f;
  r->baseline_y_delta = row_height;
  r->ymin             = 0.0f;
  r->ymax             = row_height;
  r->num_chars        = 0;

  if (!end || line_start_id >= edit->string.len) {
    return;
  }

  end += len;
  text = nk_str_at_const(&edit->string, line_start_id, &unicode, &l);
  size = nk_text_calculate_text_bounds(edit->context, font, text, (int)(end - text), row_height,
                                       &remaining, 0, &glyphs, NK_STOP_ON_NEW_LINE);

  r->x1        = size.x;
  r->num_chars = glyphs;
}

NK_INTERN int
nk_textedit_locate_coord(struct nk_text_edit *edit, float x, float y, const struct nk_user_font *font,
                         float row_height, enum nk_text_affinity *affinity)
{
  struct nk_text_edit_row r;
  int                     n      = edit->string.len;
  float                   base_y = 0;
  int                     i      = 0;

  r.x0        = r.x1 = 0;
  r.ymin      = r.ymax = 0;
  r.num_chars = 0;

  /* search rows to find one that straddles 'y' */
  while (i < n) {
    nk_textedit_layout_row(&r, edit, i, row_height, font);

    if (r.num_chars <= 0) {
      *affinity = NK_TEXT_AFFINITY_TRAILING;
      return n;
    }

    if (i == 0 && y < base_y + r.ymin) {
      *affinity = NK_TEXT_AFFINITY_LEADING;
      return 0;
    }

    if (y < base_y + r.ymax) {
      break;
    }

    i      += r.num_chars;
    base_y += r.baseline_y_delta;
  }

  /* below all text, return 'after' last character */
  if (i >= n) {
    *affinity = NK_TEXT_AFFINITY_TRAILING;
    return n;
  }

  /* check if it's before the beginning of the line */
  if (x < r.x0) {
    *affinity = NK_TEXT_AFFINITY_LEADING;
    return i;
  }

  if (x < r.x1) {
    struct nk_text_shape *shape = nk_textedit_line_shape(edit, i, font);

    if (shape) {
      struct nk_text_cursor cursor = nk_text_shape_hit_test(shape, x - r.x0);
      int line_byte_offset         = nk_textedit_byte_offset(edit, i);

      *affinity = cursor.affinity;
      return nk_textedit_rune_offset(edit, line_byte_offset + cursor.source_offset);
    }
  }

  *affinity = NK_TEXT_AFFINITY_TRAILING;

  if (nk_str_rune_at(&edit->string, i + r.num_chars - 1) == '\n') {
    return i + r.num_chars - 1;
  } else {
    return i + r.num_chars;
  }
}

NK_LIB void
nk_textedit_click(struct nk_text_edit *state, float x, float y, const struct nk_user_font *font, float row_height)
{
  /* API click: on mouse down, move the cursor to the clicked location,
   * and reset the selection */
  enum nk_text_affinity affinity;

  state->cursor                = nk_textedit_locate_coord(state, x, y, font, row_height, &affinity);
  state->cursor_affinity       = (unsigned char)affinity;
  state->select_start          = state->cursor;
  state->select_end            = state->cursor;
  state->select_start_affinity = state->cursor_affinity;
  state->has_preferred_x       = 0;
}

NK_LIB void
nk_textedit_drag(struct nk_text_edit *state, float x, float y, const struct nk_user_font *font, float row_height)
{
  /* API drag: on mouse drag, move the cursor and selection endpoint
   * to the clicked location */
  enum nk_text_affinity affinity;
  int p = nk_textedit_locate_coord(state, x, y, font, row_height, &affinity);

  if (state->select_start == state->select_end) {
    state->select_start          = state->cursor;
    state->select_start_affinity = state->cursor_affinity;
  }

  state->cursor          = state->select_end = p;
  state->cursor_affinity = (unsigned char)affinity;
}

NK_INTERN void
nk_textedit_find_charpos(struct nk_text_find *find, struct nk_text_edit *state, int n, int single_line,
                         const struct nk_user_font *font, float row_height, enum nk_text_affinity affinity)
{
  struct nk_text_edit_row r;
  int                     previous = 0;
  int                     first    = 0;

  nk_zero_struct(r);
  find->y = 0.0f;

  while (1) {
    nk_bool ends_with_newline;

    nk_textedit_layout_row(&r, state, first, row_height, font);
    ends_with_newline = r.num_chars > 0 && nk_str_rune_at(&state->string, first + r.num_chars - 1) == '\n';

    if (single_line || n < first + r.num_chars ||
        (n == first + r.num_chars && !ends_with_newline) || !r.num_chars) {
      break;
    }

    previous = first;
    first   += r.num_chars;
    find->y += r.baseline_y_delta;
  }

  find->first_char = first;
  find->length     = r.num_chars;
  find->height     = r.ymax - r.ymin;
  find->prev_first = previous;

  {
    struct nk_text_shape *shape            = nk_textedit_line_shape(state, first, font);
    int                   line_byte_offset = nk_textedit_byte_offset(state, first);
    int                   byte_offset      = nk_textedit_byte_offset(state, n) - line_byte_offset;

    find->x = shape ? nk_text_shape_cursor_at(shape, byte_offset, affinity).x : r.x0;
  }
}

NK_INTERN void
nk_textedit_clamp(struct nk_text_edit *state)
{
  /* make the selection/cursor state valid if client altered the string */
  int n = state->string.len;

  if (NK_TEXT_HAS_SELECTION(state)) {
    if (state->select_start > n) {
      state->select_start = n;
    }

    if (state->select_end > n) {
      state->select_end = n;
    }

    /* if clamping forced them to be equal, move the cursor to match */
    if (state->select_start == state->select_end) {
      state->cursor = state->select_start;
    }
  }

  if (state->cursor > n) {
    state->cursor = n;
  }
}

NK_API void
nk_textedit_delete(struct nk_text_edit *state, int where, int len)
{
  /* delete characters while updating undo */
  nk_textedit_makeundo_delete(state, where, len);
  nk_str_delete_runes(&state->string, where, len);
  state->has_preferred_x = 0;
}

NK_API void
nk_textedit_delete_selection(struct nk_text_edit *state)
{
  /* delete the section */
  nk_textedit_clamp(state);

  if (NK_TEXT_HAS_SELECTION(state)) {
    if (state->select_start < state->select_end) {
      nk_textedit_delete(state, state->select_start, state->select_end - state->select_start);
      state->select_end      = state->cursor = state->select_start;
      state->cursor_affinity = state->select_start_affinity;
    } else {
      nk_textedit_delete(state, state->select_end, state->select_start - state->select_end);
      state->select_start = state->cursor = state->select_end;
    }

    state->select_start_affinity = state->cursor_affinity;
    state->has_preferred_x       = 0;
  }
}

NK_INTERN void
nk_textedit_sortselection(struct nk_text_edit *state)
{
  /* canonicalize the selection so start <= end */
  if (state->select_end < state->select_start) {
    int           temp          = state->select_end;
    unsigned char temp_affinity = state->cursor_affinity;

    state->select_end            = state->select_start;
    state->cursor_affinity       = state->select_start_affinity;
    state->select_start          = temp;
    state->select_start_affinity = temp_affinity;
  }
}

NK_INTERN void
nk_textedit_move_to_first(struct nk_text_edit *state)
{
  /* move cursor to first character of selection */
  if (NK_TEXT_HAS_SELECTION(state)) {
    nk_textedit_sortselection(state);
    state->cursor          = state->select_start;
    state->cursor_affinity = state->select_start_affinity;
    state->select_end      = state->select_start;
    state->has_preferred_x = 0;
  }
}

NK_INTERN void
nk_textedit_move_to_last(struct nk_text_edit *state)
{
  /* move cursor to last character of selection */
  if (NK_TEXT_HAS_SELECTION(state)) {
    nk_textedit_sortselection(state);
    nk_textedit_clamp(state);
    state->cursor                = state->select_end;
    state->select_start          = state->select_end;
    state->select_start_affinity = state->cursor_affinity;
    state->has_preferred_x       = 0;
  }
}

NK_INTERN int
nk_is_word_boundary(struct nk_text_edit *state, int idx)
{
  int     len;
  nk_rune c;

  if (idx < 0) {
    return 1;
  }

  if (!nk_str_at_rune(&state->string, idx, &c, &len)) {
    return 1;
  }
#ifndef NK_IS_WORD_BOUNDARY
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' || c == 0x3000);
#else
  return NK_IS_WORD_BOUNDARY(c);
#endif
}

NK_INTERN int
nk_textedit_move_to_word_previous_fallback(struct nk_text_edit *state)
{
  int c = state->cursor - 1;

  if (c > 0) {
    if (nk_is_word_boundary(state, c)) {
      while (c > 0 && nk_is_word_boundary(state, --c))
        ;
    }

    while (!nk_is_word_boundary(state, --c))
      ;

    c++;
  } else {
    return 0;
  }

  return c;
}

NK_INTERN int
nk_textedit_move_to_word_next_fallback(struct nk_text_edit *state)
{
  const int len = state->string.len;
  int       c   = state->cursor;

  if (c < len) {
    if (!nk_is_word_boundary(state, c)) {
      while (c < len && !nk_is_word_boundary(state, ++c))
        ;
    }

    while (c < len && nk_is_word_boundary(state, ++c))
      ;
  } else {
    return len;
  }

  return c;
}

NK_INTERN nk_bool
nk_textedit_space_at(const char *text, int len, int byte_offset)
{
  nk_rune rune = 0;

  nk_utf_decode(text + byte_offset, &rune, len - byte_offset);
  return rune == ' ' || rune == '\t' || rune == '\n' || rune == '\r' || rune == '\f' || rune == '\v' || rune == 0x3000 || rune == 0xA0;
}

NK_INTERN int
nk_textedit_move_to_boundary(struct nk_text_edit *state, const struct nk_user_font *font, nk_flags flags, nk_bool next)
{
  const char           *text = nk_str_get_const(&state->string);
  int                   len  = nk_str_len_char(&state->string);
  int                   byte_offset;
  struct nk_text_shape *shape;

  if (!len) {
    return 0;
  }

  shape = nk_text_shape_build_default(state->context, font, state->context->style.font_size, text, len);

  if (!shape) {
    if (flags == NK_TEXT_BOUNDARY_WORD) {
      return next ? nk_textedit_move_to_word_next_fallback(state) : nk_textedit_move_to_word_previous_fallback(state);
    }
    return NK_CLAMP(0, state->cursor + (next ? 1 : -1), state->string.len);
  }

  byte_offset = nk_textedit_byte_offset(state, state->cursor);

  do {
    byte_offset = next ? nk_text_shape_boundary_next(shape, byte_offset, flags)
                       : nk_text_shape_boundary_previous(shape, byte_offset, flags);
  } while (flags == NK_TEXT_BOUNDARY_WORD && byte_offset > 0 && byte_offset < len && nk_textedit_space_at(text, len, byte_offset));
  return nk_textedit_rune_offset(state, byte_offset);
}

NK_INTERN int
nk_textedit_move_visual(struct nk_text_edit *state, const struct nk_user_font *font, float row_height, int direction)
{
  struct nk_text_find   find;
  struct nk_text_shape *shape;
  struct nk_text_cursor cursor;
  int                   line_byte_offset;

  nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                           (enum nk_text_affinity)state->cursor_affinity);
  shape = nk_textedit_line_shape(state, find.first_char, font);

  if (!shape) {
    return nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_GRAPHEME, direction > 0);
  }

  line_byte_offset     = nk_textedit_byte_offset(state, find.first_char);
  cursor.source_offset = nk_textedit_byte_offset(state, state->cursor) - line_byte_offset;
  cursor.affinity      = (enum nk_text_affinity)state->cursor_affinity;

  {
    int old_offset = cursor.source_offset;

    cursor = nk_text_shape_cursor_move(shape, cursor, direction);

    if (cursor.source_offset == old_offset) {
      int next = NK_CLAMP(0, state->cursor + (direction > 0 ? 1 : -1), state->string.len);

      if (next != state->cursor && nk_str_rune_at(&state->string, direction > 0 ? state->cursor : next) == '\n') {
        state->cursor_affinity = direction > 0 ? NK_TEXT_AFFINITY_LEADING : NK_TEXT_AFFINITY_TRAILING;
        return next;
      }
      return state->cursor;
    }
  }

  state->cursor_affinity = (unsigned char)cursor.affinity;
  return nk_textedit_rune_offset(state, line_byte_offset + cursor.source_offset);
}

NK_INTERN void
nk_textedit_collapse_visual(struct nk_text_edit *state, const struct nk_user_font *font, float row_height, int direction)
{
  struct nk_text_find start;
  struct nk_text_find end;
  nk_bool             use_start;

  nk_textedit_find_charpos(&start, state, state->select_start, state->single_line, font, row_height,
                           (enum nk_text_affinity)state->select_start_affinity);
  nk_textedit_find_charpos(&end, state, state->select_end, state->single_line, font, row_height,
                           (enum nk_text_affinity)state->cursor_affinity);

  if (start.y == end.y) {
    use_start = direction < 0 ? start.x < end.x : start.x > end.x;
  } else {
    use_start = direction < 0 ? state->select_start < state->select_end : state->select_start > state->select_end;
  }

  if (use_start) {
    state->cursor          = state->select_start;
    state->cursor_affinity = state->select_start_affinity;
  } else {
    state->cursor = state->select_end;
  }

  state->select_start          = state->cursor;
  state->select_end            = state->cursor;
  state->select_start_affinity = state->cursor_affinity;
  state->has_preferred_x       = 0;
}

NK_INTERN void
nk_textedit_prep_selection_at_cursor(struct nk_text_edit *state)
{
  /* update selection and cursor to match each other */
  if (!NK_TEXT_HAS_SELECTION(state)) {
    state->select_start          = state->select_end = state->cursor;
    state->select_start_affinity = state->cursor_affinity;
  } else {
    state->cursor = state->select_end;
  }
}

NK_API nk_bool
nk_textedit_cut(struct nk_text_edit *state)
{
  /* API cut: delete selection */
  if (state->mode == NK_TEXT_EDIT_MODE_VIEW) {
    return 0;
  }

  if (NK_TEXT_HAS_SELECTION(state)) {
    nk_textedit_delete_selection(state); /* implicitly clamps */
    state->has_preferred_x = 0;
    return 1;
  }
  return 0;
}

NK_API nk_bool
nk_textedit_paste(struct nk_text_edit *state, char const *ctext, int len)
{
  /* API paste: replace existing selection with passed-in text, filtered like typed text */
  int     start;
  int     inserted = 0;
  int     offset   = 0;
  int     glyph_len;
  nk_rune unicode;

  if (state->mode == NK_TEXT_EDIT_MODE_VIEW || !ctext || len <= 0) {
    return 0;
  }

  /* if there's a selection, the paste should delete it */
  nk_textedit_clamp(state);
  nk_textedit_delete_selection(state);

  start     = state->cursor;
  glyph_len = nk_utf_decode(ctext, &unicode, len);

  while (glyph_len && offset < len) {
    nk_bool skip = unicode == '\r' || unicode == 127 || (unicode == '\n' && state->single_line) ||
                   (state->filter && !state->filter(state, unicode));

    if (!skip && nk_str_insert_text_utf8(&state->string, state->cursor, ctext + offset, 1)) {
      state->cursor += 1;
      inserted      += 1;
    }

    offset   += glyph_len;
    glyph_len = nk_utf_decode(ctext + offset, &unicode, len - offset);
  }

  if (!inserted) {
    return 0;
  }

  nk_textedit_makeundo_insert(state, start, inserted);
  state->cursor_affinity = NK_TEXT_AFFINITY_TRAILING;
  state->has_preferred_x = 0;
  return 1;
}

NK_API void
nk_textedit_text(struct nk_text_edit *state, const char *text, int total_len)
{
  nk_rune unicode;
  int     glyph_len;
  int     text_len = 0;

  NK_ASSERT(state);
  NK_ASSERT(text);

  if (!text || !total_len || state->mode == NK_TEXT_EDIT_MODE_VIEW) {
    return;
  }

  glyph_len = nk_utf_decode(text, &unicode, total_len);

  while ((text_len < total_len) && glyph_len) {
    /* don't insert a backward delete, just process the event */
    if (unicode == 127) {
      goto next;
    }

    /* can't add newline in single-line mode */
    if (unicode == '\n' && state->single_line) {
      goto next;
    }

    /* "\r\n" becomes "\n" */
    if (unicode == '\r') {
      goto next;
    }

    /* filter incoming text */
    if (state->filter && !state->filter(state, unicode)) {
      goto next;
    }

    if (!NK_TEXT_HAS_SELECTION(state) && state->cursor < state->string.len) {
      if (state->mode == NK_TEXT_EDIT_MODE_REPLACE) {
        nk_textedit_makeundo_replace(state, state->cursor, 1, 1);
        nk_str_delete_runes(&state->string, state->cursor, 1);
      }

      if (nk_str_insert_text_utf8(&state->string, state->cursor, text + text_len, 1)) {
        ++state->cursor;
        state->cursor_affinity = NK_TEXT_AFFINITY_TRAILING;
        state->has_preferred_x = 0;
      }
    } else {
      nk_textedit_delete_selection(state); /* implicitly clamps */

      if (nk_str_insert_text_utf8(&state->string, state->cursor, text + text_len, 1)) {
        nk_textedit_makeundo_insert(state, state->cursor, 1);
        state->cursor          = NK_MIN(state->cursor + 1, state->string.len);
        state->cursor_affinity = NK_TEXT_AFFINITY_TRAILING;
        state->has_preferred_x = 0;
      }
    }

  next:
    text_len += glyph_len;
    glyph_len = nk_utf_decode(text + text_len, &unicode, total_len - text_len);
  }
}

NK_LIB void
nk_textedit_key(struct nk_text_edit *state, enum nk_keys key, int shift_mod, const struct nk_user_font *font, float row_height)
{
retry:
  switch (key) {
    case NK_KEY_NONE:
    case NK_KEY_CTRL:
    case NK_KEY_ENTER:
    case NK_KEY_SHIFT:
    case NK_KEY_TAB:
    case NK_KEY_COPY:
    case NK_KEY_CUT:
    case NK_KEY_PASTE:
    case NK_KEY_MAX:
    default:
      break;

    case NK_KEY_TEXT_UNDO:
      nk_textedit_undo(state);
      state->has_preferred_x = 0;
      break;

    case NK_KEY_TEXT_REDO:
      nk_textedit_redo(state);
      state->has_preferred_x = 0;
      break;

    case NK_KEY_TEXT_SELECT_ALL:
      nk_textedit_select_all(state);
      state->has_preferred_x = 0;
      break;

    case NK_KEY_TEXT_INSERT_MODE:
      state->mode = NK_TEXT_EDIT_MODE_INSERT;
      break;

    case NK_KEY_TEXT_REPLACE_MODE:
      state->mode = NK_TEXT_EDIT_MODE_REPLACE;
      break;

    case NK_KEY_TEXT_RESET_MODE:
      state->mode = NK_TEXT_EDIT_MODE_VIEW;
      break;

    case NK_KEY_LEFT:
      if (shift_mod) {
        nk_textedit_clamp(state);
        nk_textedit_prep_selection_at_cursor(state);
        /* move selection left */
        state->select_end      = nk_textedit_move_visual(state, font, row_height, -1);
        state->cursor          = state->select_end;
        state->has_preferred_x = 0;
      } else {
        /* if currently there's a selection,
         * move cursor to start of selection */
        if (NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_collapse_visual(state, font, row_height, -1);
        } else {
          state->cursor = nk_textedit_move_visual(state, font, row_height, -1);
        }

        state->has_preferred_x = 0;
      }
      break;

    case NK_KEY_RIGHT:
      if (shift_mod) {
        nk_textedit_prep_selection_at_cursor(state);
        /* move selection right */
        state->select_end = nk_textedit_move_visual(state, font, row_height, 1);
        nk_textedit_clamp(state);
        state->cursor          = state->select_end;
        state->has_preferred_x = 0;
      } else {
        /* if currently there's a selection,
         * move cursor to end of selection */
        if (NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_collapse_visual(state, font, row_height, 1);
        } else {
          state->cursor = nk_textedit_move_visual(state, font, row_height, 1);
        }

        nk_textedit_clamp(state);
        state->has_preferred_x = 0;
      }
      break;

    case NK_KEY_TEXT_WORD_LEFT:
      if (shift_mod) {
        if (!NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_prep_selection_at_cursor(state);
        }

        state->cursor          = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_false);
        state->select_end      = state->cursor;
        state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
        nk_textedit_clamp(state);
      } else {
        if (NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_move_to_first(state);
        } else {
          state->cursor          = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_false);
          state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
          nk_textedit_clamp(state);
        }
      }
      break;

    case NK_KEY_TEXT_WORD_RIGHT:
      if (shift_mod) {
        if (!NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_prep_selection_at_cursor(state);
        }

        state->cursor          = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_true);
        state->select_end      = state->cursor;
        state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
        nk_textedit_clamp(state);
      } else {
        if (NK_TEXT_HAS_SELECTION(state)) {
          nk_textedit_move_to_last(state);
        } else {
          state->cursor          = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_true);
          state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
          nk_textedit_clamp(state);
        }
      }
      break;

    case NK_KEY_DOWN: {
      struct nk_text_find find;
      int                 sel = shift_mod;

      if (state->single_line) {
        /* on windows, up&down in single-line behave like left&right */
        key = NK_KEY_RIGHT;
        goto retry;
      }

      if (sel) {
        nk_textedit_prep_selection_at_cursor(state);
      } else if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_move_to_last(state);
      }

      /* compute current position of cursor point */
      nk_textedit_clamp(state);
      nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                               (enum nk_text_affinity)state->cursor_affinity);

      /* find the closest visual caret on the next row */
      if (find.length) {
        enum nk_text_affinity affinity;
        float goal_x = state->has_preferred_x ? state->preferred_x : find.x;

        state->cursor = nk_textedit_locate_coord(state, goal_x, find.y + find.height + 0.5f,
                                                 font, row_height, &affinity);
        state->cursor_affinity = (unsigned char)affinity;

        state->has_preferred_x = 1;
        state->preferred_x     = goal_x;

        if (sel) {
          state->select_end = state->cursor;
        }
      }
    } break;

    case NK_KEY_UP: {
      struct nk_text_find find;
      int                 sel = shift_mod;

      if (state->single_line) {
        /* on windows, up&down become left&right */
        key = NK_KEY_LEFT;
        goto retry;
      }

      if (sel) {
        nk_textedit_prep_selection_at_cursor(state);
      } else if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_move_to_first(state);
      }

      /* compute current position of cursor point */
      nk_textedit_clamp(state);
      nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                               (enum nk_text_affinity)state->cursor_affinity);

      /* can only go up if there's a previous row */
      if (find.prev_first != find.first_char) {
        enum nk_text_affinity affinity;
        float goal_x = state->has_preferred_x ? state->preferred_x : find.x;

        state->cursor = nk_textedit_locate_coord(state, goal_x, find.y - 0.5f,
                                                 font, row_height, &affinity);
        state->cursor_affinity = (unsigned char)affinity;

        state->has_preferred_x = 1;
        state->preferred_x     = goal_x;

        if (sel) {
          state->select_end = state->cursor;
        }
      }
    } break;

    case NK_KEY_DEL:
      if (state->mode == NK_TEXT_EDIT_MODE_VIEW) {
        break;
      }

      if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_delete_selection(state);
      } else {
        int n = state->string.len;

        if (state->cursor < n) {
          int next = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_GRAPHEME, nk_true);

          nk_textedit_delete(state, state->cursor, next - state->cursor);
        }
      }

      state->has_preferred_x = 0;
      break;

    case NK_KEY_BACKSPACE:
      if (state->mode == NK_TEXT_EDIT_MODE_VIEW) {
        break;
      }

      if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_delete_selection(state);
      } else {
        nk_textedit_clamp(state);

        if (state->cursor > 0) {
          int previous = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_GRAPHEME, nk_false);

          nk_textedit_delete(state, previous, state->cursor - previous);
          state->cursor          = previous;
          state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
        }
      }

      state->has_preferred_x = 0;
      break;

    case NK_KEY_TEXT_START:
      if (shift_mod) {
        nk_textedit_prep_selection_at_cursor(state);
        state->cursor          = state->select_end = 0;
        state->cursor_affinity = NK_TEXT_AFFINITY_LEADING;
        state->has_preferred_x = 0;
      } else {
        state->cursor                = state->select_start = state->select_end = 0;
        state->cursor_affinity       = NK_TEXT_AFFINITY_LEADING;
        state->select_start_affinity = NK_TEXT_AFFINITY_LEADING;
        state->has_preferred_x       = 0;
      }
      break;

    case NK_KEY_TEXT_END:
      if (shift_mod) {
        nk_textedit_prep_selection_at_cursor(state);
        state->cursor          = state->select_end = state->string.len;
        state->cursor_affinity = NK_TEXT_AFFINITY_TRAILING;
        state->has_preferred_x = 0;
      } else {
        state->cursor                = state->string.len;
        state->cursor_affinity       = NK_TEXT_AFFINITY_TRAILING;
        state->select_start          = state->select_end = state->cursor;
        state->select_start_affinity = state->cursor_affinity;
        state->has_preferred_x       = 0;
      }
      break;

    case NK_KEY_TEXT_LINE_START: {
      if (shift_mod) {
        struct nk_text_find find;

        nk_textedit_clamp(state);
        nk_textedit_prep_selection_at_cursor(state);
        nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                                 (enum nk_text_affinity)state->cursor_affinity);
        state->cursor          = state->select_end = find.first_char;
        state->cursor_affinity = NK_TEXT_AFFINITY_LEADING;
        state->has_preferred_x = 0;
      } else {
        struct nk_text_find find;

        nk_textedit_clamp(state);
        nk_textedit_move_to_first(state);
        nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                                 (enum nk_text_affinity)state->cursor_affinity);
        state->cursor                = find.first_char;
        state->cursor_affinity       = NK_TEXT_AFFINITY_LEADING;
        state->select_start          = state->select_end = state->cursor;
        state->select_start_affinity = state->cursor_affinity;
        state->has_preferred_x       = 0;
      }
    } break;

    case NK_KEY_TEXT_LINE_END: {
      if (shift_mod) {
        struct nk_text_find find;

        nk_textedit_clamp(state);
        nk_textedit_prep_selection_at_cursor(state);
        nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                                 (enum nk_text_affinity)state->cursor_affinity);
        state->has_preferred_x = 0;
        state->cursor          = find.first_char + find.length;

        if (find.length > 0 && nk_str_rune_at(&state->string, state->cursor - 1) == '\n') {
          --state->cursor;
        }

        state->select_end      = state->cursor;
        state->cursor_affinity = NK_TEXT_AFFINITY_TRAILING;
      } else {
        struct nk_text_find find;

        nk_textedit_clamp(state);
        nk_textedit_move_to_first(state);
        nk_textedit_find_charpos(&find, state, state->cursor, state->single_line, font, row_height,
                                 (enum nk_text_affinity)state->cursor_affinity);

        state->has_preferred_x = 0;
        state->cursor          = find.first_char + find.length;

        if (find.length > 0 && nk_str_rune_at(&state->string, state->cursor - 1) == '\n') {
          --state->cursor;
        }

        state->cursor_affinity       = NK_TEXT_AFFINITY_TRAILING;
        state->select_start          = state->select_end = state->cursor;
        state->select_start_affinity = state->cursor_affinity;
      }
    } break;

    case NK_KEY_TEXT_WORD_DELETE_LEFT: {
      if (state->mode == NK_TEXT_EDIT_MODE_VIEW) {
        break;
      }

      if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_delete_selection(state);
      } else {
        int old_cursor;

        nk_textedit_clamp(state);
        old_cursor = state->cursor;

        if (old_cursor > 0) {
          int new_cursor = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_false);

          if (new_cursor < old_cursor) {
            nk_textedit_delete(state, new_cursor, old_cursor - new_cursor);
            state->cursor          = new_cursor;
            state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
          }
        }
      }

      state->has_preferred_x = 0;
    } break;

    case NK_KEY_TEXT_WORD_DELETE_RIGHT: {
      if (state->mode == NK_TEXT_EDIT_MODE_VIEW) {
        break;
      }

      if (NK_TEXT_HAS_SELECTION(state)) {
        nk_textedit_delete_selection(state);
      } else {
        int old_cursor;
        int new_cursor;

        nk_textedit_clamp(state);
        old_cursor = state->cursor;

        new_cursor = nk_textedit_move_to_boundary(state, font, NK_TEXT_BOUNDARY_WORD, nk_true);
        nk_textedit_clamp(state);

        if (new_cursor > old_cursor) {
          nk_textedit_delete(state, old_cursor, new_cursor - old_cursor);
          state->cursor          = old_cursor;
          state->cursor_affinity = state->cursor ? NK_TEXT_AFFINITY_TRAILING : NK_TEXT_AFFINITY_LEADING;
        }
      }

      state->has_preferred_x = 0;
    } break;
  }
}

NK_INTERN void
nk_textedit_flush_redo(struct nk_text_undo_state *state)
{
  state->redo_point      = NK_TEXTEDIT_UNDOSTATECOUNT;
  state->redo_char_point = NK_TEXTEDIT_UNDOCHARCOUNT;
}

NK_INTERN void
nk_textedit_discard_undo(struct nk_text_undo_state *state)
{
  /* discard the oldest entry in the undo list */
  if (state->undo_point > 0) {
    /* if the 0th undo state has characters, clean those up */
    if (state->undo_rec[0].char_storage >= 0) {
      int n = state->undo_rec[0].insert_length, i;

      /* delete n characters from all other records */
      state->undo_char_point = (short)(state->undo_char_point - n);
      NK_MEMMOVE(state->undo_char, state->undo_char + n, (nk_size)state->undo_char_point * sizeof(nk_rune));

      for (i = 0; i < state->undo_point; ++i) {
        if (state->undo_rec[i].char_storage >= 0) {
          state->undo_rec[i].char_storage = (short)(state->undo_rec[i].char_storage - n);
        }
      }
    }

    --state->undo_point;
    NK_MEMMOVE(state->undo_rec, state->undo_rec + 1, (nk_size)((nk_size)state->undo_point * sizeof(state->undo_rec[0])));
  }
}

NK_INTERN void
nk_textedit_discard_redo(struct nk_text_undo_state *state)
{
  /*  discard the oldest entry in the redo list--it's bad if this
      ever happens, but because undo & redo have to store the actual
      characters in different cases, the redo character buffer can
      fill up even though the undo buffer didn't */
  nk_size num;
  int     k = NK_TEXTEDIT_UNDOSTATECOUNT - 1;

  if (state->redo_point <= k) {
    /* if the k'th undo state has characters, clean those up */
    if (state->undo_rec[k].char_storage >= 0) {
      int n = state->undo_rec[k].insert_length, i;

      /* delete n characters from all other records */
      state->redo_char_point = (short)(state->redo_char_point + n);
      num                    = (nk_size)(NK_TEXTEDIT_UNDOCHARCOUNT - state->redo_char_point);
      NK_MEMMOVE(state->undo_char + state->redo_char_point, state->undo_char + state->redo_char_point - n, num * sizeof(nk_rune));

      for (i = state->redo_point; i < k; ++i) {
        if (state->undo_rec[i].char_storage >= 0) {
          state->undo_rec[i].char_storage = (short)(state->undo_rec[i].char_storage + n);
        }
      }
    }

    ++state->redo_point;
    num = (nk_size)(NK_TEXTEDIT_UNDOSTATECOUNT - state->redo_point);

    if (num) {
      NK_MEMMOVE(state->undo_rec + state->redo_point - 1, state->undo_rec + state->redo_point, num * sizeof(state->undo_rec[0]));
    }
  }
}

NK_INTERN struct nk_text_undo_record *
nk_textedit_create_undo_record(struct nk_text_undo_state *state, int numchars)
{
  /* any time we create a new undo record, we discard redo*/
  nk_textedit_flush_redo(state);

  /* if we have no free records, we have to make room,
   * by sliding the existing records down */
  if (state->undo_point == NK_TEXTEDIT_UNDOSTATECOUNT) {
    nk_textedit_discard_undo(state);
  }

  /* if the characters to store won't possibly fit in the buffer,
   * we can't undo */
  if (numchars > NK_TEXTEDIT_UNDOCHARCOUNT) {
    state->undo_point      = 0;
    state->undo_char_point = 0;
    return 0;
  }

  /* if we don't have enough free characters in the buffer,
   * we have to make room */
  while (state->undo_char_point + numchars > NK_TEXTEDIT_UNDOCHARCOUNT) {
    nk_textedit_discard_undo(state);
  }
  return &state->undo_rec[state->undo_point++];
}

NK_INTERN nk_rune *
nk_textedit_createundo(struct nk_text_undo_state *state, int pos, int insert_len, int delete_len)
{
  struct nk_text_undo_record *r = nk_textedit_create_undo_record(state, insert_len);

  if (r == 0) {
    return 0;
  }

  r->where         = pos;
  r->insert_length = (short)insert_len;
  r->delete_length = (short)delete_len;

  if (insert_len == 0) {
    r->char_storage = -1;
    return 0;
  } else {
    r->char_storage        = state->undo_char_point;
    state->undo_char_point = (short)(state->undo_char_point + insert_len);
    return &state->undo_char[r->char_storage];
  }
}

NK_API void
nk_textedit_undo(struct nk_text_edit *state)
{
  struct nk_text_undo_state *s = &state->undo;
  struct nk_text_undo_record u, *r;

  if (s->undo_point == 0) {
    return;
  }

  /* we need to do two things: apply the undo record, and create a redo record */
  u               = s->undo_rec[s->undo_point - 1];
  r               = &s->undo_rec[s->redo_point - 1];
  r->char_storage = -1;

  r->insert_length = u.delete_length;
  r->delete_length = u.insert_length;
  r->where         = u.where;

  if (u.delete_length) {
    /*   if the undo record says to delete characters, then the redo record will
         need to re-insert the characters that get deleted, so we need to store
         them.
         there are three cases:
             - there's enough room to store the characters
             - characters stored for *redoing* don't leave room for redo
             - characters stored for *undoing* don't leave room for redo
         if the last is true, we have to bail */
    if (s->undo_char_point + u.delete_length >= NK_TEXTEDIT_UNDOCHARCOUNT) {
      /* the undo records take up too much character space; there's no space
       * to store the redo characters */
      r->insert_length = 0;
    } else {
      int i;

      /* there's definitely room to store the characters eventually */
      while (s->undo_char_point + u.delete_length > s->redo_char_point) {
        /* there's currently not enough room, so discard a redo record */
        nk_textedit_discard_redo(s);

        /* should never happen: */
        if (s->redo_point == NK_TEXTEDIT_UNDOSTATECOUNT) {
          return;
        }
      }

      r                  = &s->undo_rec[s->redo_point - 1];
      r->char_storage    = (short)(s->redo_char_point - u.delete_length);
      s->redo_char_point = (short)(s->redo_char_point - u.delete_length);

      /* now save the characters */
      for (i = 0; i < u.delete_length; ++i) {
        s->undo_char[r->char_storage + i] = nk_str_rune_at(&state->string, u.where + i);
      }
    }

    /* now we can carry out the deletion */
    nk_str_delete_runes(&state->string, u.where, u.delete_length);
  }

  /* check type of recorded action: */
  if (u.insert_length) {
    /* easy case: was a deletion, so we need to insert n characters */
    nk_str_insert_text_runes(&state->string, u.where, &s->undo_char[u.char_storage], u.insert_length);
    s->undo_char_point = (short)(s->undo_char_point - u.insert_length);
  }

  state->cursor = (short)(u.where + u.insert_length);

  s->undo_point--;
  s->redo_point--;
}

NK_API void
nk_textedit_redo(struct nk_text_edit *state)
{
  struct nk_text_undo_state  *s = &state->undo;
  struct nk_text_undo_record *u, r;

  if (s->redo_point == NK_TEXTEDIT_UNDOSTATECOUNT) {
    return;
  }

  /* we need to do two things: apply the redo record, and create an undo record */
  u = &s->undo_rec[s->undo_point];
  r = s->undo_rec[s->redo_point];

  /* we KNOW there must be room for the undo record, because the redo record
  was derived from an undo record */
  u->delete_length = r.insert_length;
  u->insert_length = r.delete_length;
  u->where         = r.where;
  u->char_storage  = -1;

  if (r.delete_length) {
    /* the redo record requires us to delete characters, so the undo record
    needs to store the characters */
    if (s->undo_char_point + u->insert_length > s->redo_char_point) {
      u->insert_length = 0;
      u->delete_length = 0;
    } else {
      int i;

      u->char_storage    = s->undo_char_point;
      s->undo_char_point = (short)(s->undo_char_point + u->insert_length);

      /* now save the characters */
      for (i = 0; i < u->insert_length; ++i) {
        s->undo_char[u->char_storage + i] = nk_str_rune_at(&state->string, u->where + i);
      }
    }

    nk_str_delete_runes(&state->string, r.where, r.delete_length);
  }

  if (r.insert_length) {
    /* easy case: need to insert n characters */
    nk_str_insert_text_runes(&state->string, r.where, &s->undo_char[r.char_storage], r.insert_length);
  }

  state->cursor = r.where + r.insert_length;

  s->undo_point++;
  s->redo_point++;
}

NK_INTERN void
nk_textedit_makeundo_insert(struct nk_text_edit *state, int where, int length)
{
  nk_textedit_createundo(&state->undo, where, 0, length);
}

NK_INTERN void
nk_textedit_makeundo_delete(struct nk_text_edit *state, int where, int length)
{
  int      i;
  nk_rune *p = nk_textedit_createundo(&state->undo, where, length, 0);

  if (p) {
    for (i = 0; i < length; ++i) {
      p[i] = nk_str_rune_at(&state->string, where + i);
    }
  }
}

NK_INTERN void
nk_textedit_makeundo_replace(struct nk_text_edit *state, int where, int old_length, int new_length)
{
  int      i;
  nk_rune *p = nk_textedit_createundo(&state->undo, where, old_length, new_length);

  if (p) {
    for (i = 0; i < old_length; ++i) {
      p[i] = nk_str_rune_at(&state->string, where + i);
    }
  }
}

NK_LIB void
nk_textedit_clear_state(struct nk_text_edit *state, enum nk_text_edit_type type, nk_plugin_filter filter)
{
  /* reset the state to default */
  state->undo.undo_point       = 0;
  state->undo.undo_char_point  = 0;
  state->undo.redo_point       = NK_TEXTEDIT_UNDOSTATECOUNT;
  state->undo.redo_char_point  = NK_TEXTEDIT_UNDOCHARCOUNT;
  state->select_end            = state->select_start = 0;
  state->cursor                = 0;
  state->has_preferred_x       = 0;
  state->preferred_x           = 0;
  state->cursor_at_end_of_line = 0;
  state->cursor_affinity       = NK_TEXT_AFFINITY_LEADING;
  state->select_start_affinity = NK_TEXT_AFFINITY_LEADING;
  state->initialized           = 1;
  state->single_line           = (unsigned char)(type == NK_TEXT_EDIT_SINGLE_LINE);
  state->mode                  = NK_TEXT_EDIT_MODE_VIEW;
  state->filter                = filter;
  state->scrollbar             = nk_vec2(0, 0);
  state->scrollbar_prev_max    = nk_vec2(0, 0);
  state->scrollbar_flags       = 0;
}

NK_API void
nk_textedit_init_fixed(struct nk_text_edit *state, void *memory, nk_size size)
{
  NK_ASSERT(state);
  NK_ASSERT(memory);

  if (!state || !memory || !size) {
    return;
  }

  NK_MEMSET(state, 0, sizeof(struct nk_text_edit));
  nk_textedit_clear_state(state, NK_TEXT_EDIT_SINGLE_LINE, 0);
  nk_str_init_fixed(&state->string, memory, size);
}

NK_API void
nk_textedit_init(struct nk_text_edit *state, const struct nk_allocator *alloc, nk_size size)
{
  NK_ASSERT(state);
  NK_ASSERT(alloc);

  if (!state || !alloc) {
    return;
  }

  NK_MEMSET(state, 0, sizeof(struct nk_text_edit));
  nk_textedit_clear_state(state, NK_TEXT_EDIT_SINGLE_LINE, 0);
  nk_str_init(&state->string, alloc, size);
}

#ifdef NK_INCLUDE_DEFAULT_ALLOCATOR
NK_API void
nk_textedit_init_default(struct nk_text_edit *state)
{
  NK_ASSERT(state);

  if (!state) {
    return;
  }

  NK_MEMSET(state, 0, sizeof(struct nk_text_edit));
  nk_textedit_clear_state(state, NK_TEXT_EDIT_SINGLE_LINE, 0);
  nk_str_init_default(&state->string);
}
#endif

NK_API void
nk_textedit_select_all(struct nk_text_edit *state)
{
  NK_ASSERT(state);
  state->select_start          = 0;
  state->select_end            = state->string.len;
  state->cursor                = state->select_end;
  state->select_start_affinity = NK_TEXT_AFFINITY_LEADING;
  state->cursor_affinity       = NK_TEXT_AFFINITY_TRAILING;
}

NK_API void
nk_textedit_free(struct nk_text_edit *state)
{
  NK_ASSERT(state);

  if (!state) {
    return;
  }

  nk_str_free(&state->string);
}

/* ===============================================================
 *
 *                          FILTER
 *
 * ===============================================================*/
NK_API nk_bool
nk_filter_default(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(unicode);
  NK_UNUSED(box);
  return nk_true;
}

NK_API nk_bool
nk_filter_ascii(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if (unicode > 128) {
    return nk_false;
  } else {
    return nk_true;
  }
}

NK_API nk_bool
nk_filter_float(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if ((unicode < '0' || unicode > '9') && unicode != '.' && unicode != '-') {
    return nk_false;
  } else {
    return nk_true;
  }
}

NK_API nk_bool
nk_filter_decimal(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if ((unicode < '0' || unicode > '9') && unicode != '-') {
    return nk_false;
  } else {
    return nk_true;
  }
}

NK_API nk_bool
nk_filter_hex(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if ((unicode < '0' || unicode > '9') && (unicode < 'a' || unicode > 'f') && (unicode < 'A' || unicode > 'F')) {
    return nk_false;
  } else {
    return nk_true;
  }
}

NK_API nk_bool
nk_filter_oct(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if (unicode < '0' || unicode > '7') {
    return nk_false;
  } else {
    return nk_true;
  }
}

NK_API nk_bool
nk_filter_binary(const struct nk_text_edit *box, nk_rune unicode)
{
  NK_UNUSED(box);

  if (unicode != '0' && unicode != '1') {
    return nk_false;
  } else {
    return nk_true;
  }
}

/* ===============================================================
 *
 *                          EDIT
 *
 * ===============================================================*/
NK_LIB void
nk_edit_draw_text(struct nk_command_buffer   *out,
                  const struct nk_style_edit *style,
                  float                       pos_x,
                  float                       pos_y,
                  float                       x_offset,
                  const char                 *text,
                  int                         byte_len,
                  float                       row_height,
                  const struct nk_user_font  *font,
                  struct nk_color             background,
                  struct nk_color             foreground,
                  nk_bool                     is_selected)
{
  NK_ASSERT(out);
  NK_ASSERT(font);
  NK_ASSERT(style);

  if (!text || !byte_len || !out || !style) {
    return;
  }

  {
    int         glyph_len   = 0;
    nk_rune     unicode     = 0;
    int         text_len    = 0;
    float       line_width  = 0;
    const char *line        = text;
    float       line_offset = 0;
    int         line_count  = 0;

    struct nk_text txt;

    foreground     = nk_rgb_factor(foreground, style->color_factor);
    background     = nk_rgb_factor(background, style->color_factor);
    txt.padding    = nk_vec2(0, 0);
    txt.background = background;
    txt.text       = foreground;

    glyph_len = nk_utf_decode(text + text_len, &unicode, byte_len - text_len);

    if (!glyph_len) {
      return;
    }

    while ((text_len < byte_len) && glyph_len) {
      if (unicode == '\n') {
        /* new line separator so draw previous line */
        struct nk_rect label;

        line_width = nk_text_width(out->context, font, out->context->style.font_size, line, (int)((text + text_len) - line));
        label.y    = pos_y + line_offset;
        label.h    = row_height;
        label.w    = line_width;
        label.x    = pos_x;

        if (!line_count) {
          label.x += x_offset;
        }

        if (is_selected) { /* selection needs to draw different background color */
          nk_fill_rect(out, label, 0, background);
        }

        nk_widget_text(out, label, line, (int)((text + text_len) - line), &txt, NK_TEXT_CENTERED, font);

        text_len++;
        line_count++;
        line_width   = 0;
        line         = text + text_len;
        line_offset += row_height;
        glyph_len    = nk_utf_decode(text + text_len, &unicode, (int)(byte_len - text_len));
        continue;
      }

      if (unicode == '\r') {
        text_len++;
        glyph_len = nk_utf_decode(text + text_len, &unicode, byte_len - text_len);
        continue;
      }

      text_len += glyph_len;
      glyph_len = nk_utf_decode(text + text_len, &unicode, byte_len - text_len);
      continue;
    }

    line_width = nk_text_width(out->context, font, out->context->style.font_size, line, (int)((text + text_len) - line));

    if (line_width > 0) {
      /* draw last line */
      struct nk_rect label;

      label.y = pos_y + line_offset;
      label.h = row_height;
      label.w = line_width;
      label.x = pos_x;

      if (!line_count) {
        label.x += x_offset;
      }

      if (is_selected) {
        nk_fill_rect(out, label, 0, background);
      }

      nk_widget_text(out, label, line, (int)((text + text_len) - line), &txt, NK_TEXT_LEFT, font);
    }
  }
}

NK_INTERN void
nk_edit_draw_text_selection(struct nk_command_buffer *out, const struct nk_style_edit *style,
                            float pos_x, float pos_y, struct nk_text_edit *edit, float row_height,
                            const struct nk_user_font *font, struct nk_color background, struct nk_color foreground,
                            struct nk_color selected_background, struct nk_color selected_foreground)
{
  const char *text            = nk_str_get_const(&edit->string);
  int         text_len        = nk_str_len_char(&edit->string);
  int         selection_begin = nk_textedit_byte_offset(edit, NK_MIN(edit->select_start, edit->select_end));
  int         selection_end   = nk_textedit_byte_offset(edit, NK_MAX(edit->select_start, edit->select_end));
  int         line_begin      = 0;
  int         line_idx        = 0;

  /* pass 1 draws selected text again in its own color */
  int same = selected_foreground.r == foreground.r && selected_foreground.g == foreground.g &&
             selected_foreground.b == foreground.b && selected_foreground.a == foreground.a;
  int pass;

  for (pass = 0; pass < (same ? 1 : 2); ++pass) {
    line_begin = 0;
    line_idx   = 0;

    while (line_begin < text_len && selection_begin != selection_end) {
      struct nk_text_selection_iterator iterator;
      struct nk_text_selection_span     span;
      struct nk_text_shape             *shape;
      int                               line_end = line_begin;
      int                               select_begin;
      int                               select_end;

      while (line_end < text_len && text[line_end] != '\n') {
        line_end += 1;
      }

      select_begin = NK_CLAMP(line_begin, selection_begin, line_end) - line_begin;
      select_end   = NK_CLAMP(line_begin, selection_end,   line_end) - line_begin;
      shape        = nk_text_shape_build_default(out->context, font, out->context->style.font_size,
                                                 text + line_begin, line_end - line_begin);

      if (shape && select_begin < select_end) {
        nk_text_selection_iterator_begin(&iterator, shape, select_begin, select_end);

        while (nk_text_selection_iterator_next(&iterator, &span)) {
          struct nk_rect selection = nk_rect(pos_x + span.x, pos_y + line_idx * row_height, span.width, row_height);

          if (pass == 0) {
            nk_fill_rect(out, selection, 0, selected_background);
          } else {
            struct nk_rect old_clip = out->clip;
            struct nk_rect span_clip;

            nk_unify(&span_clip, &old_clip, selection.x, selection.y, selection.x + selection.w, selection.y + selection.h);
            nk_push_scissor(out, span_clip);
            nk_draw_text(out, nk_rect(pos_x, selection.y, shape->advance.x, row_height),
                         text + line_begin, line_end - line_begin, font, selected_background, selected_foreground);
            nk_push_scissor(out, old_clip);
          }
        }
      }

      line_begin = line_end + 1;
      line_idx  += 1;
    }

    if (pass == 0) {
      nk_edit_draw_text(out, style, pos_x, pos_y, 0, text, text_len, row_height, font, background, foreground, nk_false);
    }
  }
}

NK_LIB nk_flags
nk_do_edit(nk_flags                   *state,
           struct nk_command_buffer   *out,
           struct nk_rect              bounds,
           nk_flags                    flags,
           nk_plugin_filter            filter,
           struct nk_text_edit        *edit,
           const struct nk_style_edit *style,
           struct nk_input            *in,
           const struct nk_user_font  *font)
{
  struct nk_rect area;
  nk_flags       ret = 0;
  float          row_height;
  char           prev_state    = 0;
  char           is_hovered    = 0;
  char           select_all    = 0;
  char           cursor_follow = 0;
  struct nk_rect old_clip;
  struct nk_rect clip;

  NK_ASSERT(state);
  NK_ASSERT(out);
  NK_ASSERT(style);

  if (!state || !out || !style) {
    return ret;
  }

  edit->context = out->context;

  /* visible text area calculation */
  area.x = bounds.x + style->padding.x + style->border;
  area.y = bounds.y + style->padding.y + style->border;
  area.w = bounds.w - (2.0f * style->padding.x + 2 * style->border);
  area.h = bounds.h - (2.0f * style->padding.y + 2 * style->border);

  if (flags & NK_EDIT_MULTILINE) {
    area.w = NK_MAX(0, area.w - style->scrollbar_size.x);
  }

  row_height = (flags & NK_EDIT_MULTILINE) ? out->context->style.font_size + style->row_padding : area.h;

  /* calculate clipping rectangle */
  old_clip = out->clip;
  nk_unify(&clip, &old_clip, area.x, area.y, area.x + area.w, area.y + area.h);

  /* update edit state */
  prev_state = (char)edit->active;

  if (in && in->mouse.buttons[NK_BUTTON_LEFT].clicked && in->mouse.buttons[NK_BUTTON_LEFT].down && !out->context->nav.hold) {
    edit->active = NK_INBOX(in->mouse.pos.x, in->mouse.pos.y, bounds.x, bounds.y, bounds.w, bounds.h);
  }

  nk_nav_stop(out->context, bounds, (edit->active ? NK_NAV_STOP_ACTIVE : 0) | ((flags & NK_EDIT_READ_ONLY) ? 0 : NK_NAV_STOP_TEXT));

  if (edit->active) {
    out->context->nav.editing = nk_true;
  }

  /* (de)activate text editor */
  if (!prev_state && edit->active) {
    const enum nk_text_edit_type type = (flags & NK_EDIT_MULTILINE) ? NK_TEXT_EDIT_MULTI_LINE : NK_TEXT_EDIT_SINGLE_LINE;
    /* keep scroll position when re-activating edit widget */
    struct nk_vec2               oldscrollbar          = edit->scrollbar;
    struct nk_vec2               oldscrollbar_prev_max = edit->scrollbar_prev_max;
    nk_uint                      oldscrollbar_flags    = edit->scrollbar_flags;

    nk_textedit_clear_state(edit, type, filter);

    edit->scrollbar          = oldscrollbar;
    edit->scrollbar_prev_max = oldscrollbar_prev_max;
    edit->scrollbar_flags    = oldscrollbar_flags;

    if (flags & NK_EDIT_AUTO_SELECT) {
      select_all = nk_true;
    }

    if (flags & NK_EDIT_GOTO_END_ON_ACTIVATE) {
      edit->cursor = edit->string.len;
      in           = 0;
    }
  } else if (!edit->active) {
    edit->mode = NK_TEXT_EDIT_MODE_VIEW;

    if (edit->single_line) {
      edit->scrollbar.x = 0;
    }
  }

  if (flags & NK_EDIT_READ_ONLY) {
    edit->mode = NK_TEXT_EDIT_MODE_VIEW;
  } else if (flags & NK_EDIT_ALWAYS_INSERT_MODE) {
    edit->mode = NK_TEXT_EDIT_MODE_INSERT;
  }

  ret = (edit->active) ? NK_EDIT_ACTIVE : NK_EDIT_INACTIVE;

  if (prev_state != edit->active) {
    ret |= (edit->active) ? NK_EDIT_ACTIVATED : NK_EDIT_DEACTIVATED;
  }

  /* handle user input */
  if (edit->active && in) {
    int         shift_mod = in->keyboard.keys[NK_KEY_SHIFT].down;
    const float mouse_x   = (in->mouse.pos.x - area.x) + edit->scrollbar.x;
    const float mouse_y   = (in->mouse.pos.y - area.y) + edit->scrollbar.y;

    /* mouse click handler */
    is_hovered = (char)nk_input_is_mouse_hovering_rect(in, area);

    if (select_all) {
      nk_textedit_select_all(edit);
    } else if (is_hovered && in->mouse.buttons[NK_BUTTON_DOUBLE].down && in->mouse.buttons[NK_BUTTON_DOUBLE].clicked) {
      nk_textedit_click(edit, mouse_x, mouse_y, font, row_height);
      nk_textedit_key(edit, NK_KEY_TEXT_WORD_LEFT, nk_false, font, row_height);
      nk_textedit_key(edit, NK_KEY_TEXT_WORD_RIGHT, nk_true, font, row_height);

      while (edit->select_end > edit->select_start && nk_str_rune_at(&edit->string, edit->select_end - 1) == ' ') {
        edit->select_end -= 1;
      }

      edit->cursor  = edit->select_end;
      cursor_follow = nk_true;
    } else if (is_hovered && in->mouse.buttons[NK_BUTTON_LEFT].down && in->mouse.buttons[NK_BUTTON_LEFT].clicked) {
      int anchor = edit->select_start != edit->select_end ? edit->select_start : edit->cursor;

      nk_textedit_click(edit, mouse_x, mouse_y, font, row_height);

      if (shift_mod && prev_state) {
        edit->select_start = anchor;
        edit->select_end   = edit->cursor;
      }

      cursor_follow = nk_true;
    } else if (is_hovered && in->mouse.buttons[NK_BUTTON_LEFT].down && nk_input_is_mouse_moved(in)) {
      nk_textedit_drag(edit, mouse_x, mouse_y, font, row_height);
      cursor_follow = nk_true;
    }

    {
      int i; /* keyboard input */
      int old_mode = edit->mode;

      for (i = 0; i < NK_KEY_MAX; ++i) {
        if (i == NK_KEY_ENTER || i == NK_KEY_TAB) {
          continue; /* special case */
        }

        if (nk_input_is_key_pressed(in, (enum nk_keys)i)) {
          int   old_cursor     = edit->cursor;
          int   old_length     = edit->string.len;
          short old_undo_point = edit->undo.undo_point;
          short old_redo_point = edit->undo.redo_point;

          nk_textedit_key(edit, (enum nk_keys)i, shift_mod, font, row_height);

          if ((i != NK_KEY_TEXT_SELECT_ALL) &&
              (edit->cursor          != old_cursor     ||
               edit->string.len      != old_length     ||
               edit->undo.undo_point != old_undo_point ||
               edit->undo.redo_point != old_redo_point)) {
            cursor_follow = nk_true;
          }
        }
      }

      if (old_mode != edit->mode) {
        in->keyboard.text_len = 0;
      }
    }

    /* text input */
    edit->filter = filter;

    if (in->keyboard.text_len) {
      nk_textedit_text(edit, in->keyboard.text, in->keyboard.text_len);
      cursor_follow         = nk_true;
      in->keyboard.text_len = 0;
    }

    /* enter key handler */
    if (nk_input_is_key_pressed(in, NK_KEY_ENTER) && prev_state) {
      cursor_follow = nk_true;

      if (flags & NK_EDIT_CTRL_ENTER_NEWLINE && shift_mod) {
        nk_textedit_text(edit, "\n", 1);
      } else if (flags & NK_EDIT_SIG_ENTER) {
        ret |= NK_EDIT_COMMITED;
      } else if (edit->single_line) {
        edit->active = nk_false;
        ret          = (ret & ~(nk_flags)(NK_EDIT_ACTIVE | NK_EDIT_ACTIVATED)) | NK_EDIT_INACTIVE | NK_EDIT_DEACTIVATED;
      } else {
        nk_textedit_text(edit, "\n", 1);
      }
    }

    /* cut & copy handler */
    {
      int copy = nk_input_is_key_pressed(in, NK_KEY_COPY);
      int cut  = nk_input_is_key_pressed(in, NK_KEY_CUT);

      if ((copy || cut) && (flags & NK_EDIT_CLIPBOARD)) {
        int begin = NK_MIN(edit->select_start, edit->select_end);
        int end   = NK_MAX(edit->select_start, edit->select_end);

        if (edit->clip.copy) {
          int         glyph_len;
          nk_rune     unicode;
          const char *text_begin, *text_end;

          text_begin = nk_str_at_const(&edit->string, begin, &unicode, &glyph_len);
          text_end   = nk_str_at_const(&edit->string, end, &unicode, &glyph_len);
          edit->clip.copy(edit->clip.userdata, text_begin, (int)(text_end - text_begin));
        }

        if (cut && !(flags & NK_EDIT_READ_ONLY)) {
          nk_textedit_cut(edit);
          cursor_follow = nk_true;
        }
      }
    }

    /* paste handler */
    {
      int paste = nk_input_is_key_pressed(in, NK_KEY_PASTE);

      if (paste && (flags & NK_EDIT_CLIPBOARD) && edit->clip.paste) {
        edit->clip.paste(edit->clip.userdata, edit);
        cursor_follow = nk_true;
      }
    }

    /* tab handler */
    {
      int tab = nk_input_is_key_pressed(in, NK_KEY_TAB);

      if (tab && (flags & NK_EDIT_ALLOW_TAB)) {
        nk_textedit_text(edit, "    ", 4);
        cursor_follow = nk_true;
      }
    }
  }

  /* set widget state */
  if (edit->active) {
    *state = NK_WIDGET_STATE_ACTIVE;
  } else {
    nk_widget_state_reset(state);
  }

  if (is_hovered) {
    *state |= NK_WIDGET_STATE_HOVERED;
  }

  /* DRAW EDIT */
  {
    const char *text = nk_str_get_const(&edit->string);
    int         len  = nk_str_len_char(&edit->string);

    { /* select background colors/images  */
      const struct nk_style_item *background;

      if (*state & NK_WIDGET_STATE_ACTIVED) {
        background = &style->active;
      } else if (*state & NK_WIDGET_STATE_HOVER) {
        background = &style->hover;
      } else {
        background = &style->normal;
      }

      /* draw background frame */
      switch (background->type) {
        case NK_STYLE_ITEM_IMAGE:
          nk_draw_image(out, bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
          break;

        case NK_STYLE_ITEM_NINE_SLICE:
          nk_draw_nine_slice(out, bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
          break;

        case NK_STYLE_ITEM_COLOR:
          nk_fill_rect(out, bounds, style->rounding, nk_rgb_factor(background->data.color, style->color_factor));
          nk_stroke_rect(out, bounds, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor));
          break;
      }
    }

    area.w = NK_MAX(0, area.w - style->cursor_size);

    if (edit->active) {
      int            total_lines = 1;
      struct nk_vec2 text_size   = nk_vec2(0, 0);
      struct nk_vec2 cursor_pos  = nk_vec2(0, 0);

      if (text && len) {
        int line_begin = 0;
        int line_end;

        for (line_end = 0; line_end < len; ++line_end) {
          if (text[line_end] == '\n') {
            float width = nk_text_width(out->context, font, out->context->style.font_size,
                                        text + line_begin, line_end - line_begin);

            text_size.x  = NK_MAX(text_size.x, width);
            line_begin   = line_end + 1;
            total_lines += 1;
          }
        }

        text_size.x = NK_MAX(text_size.x, nk_text_width(out->context, font, out->context->style.font_size,
                                                        text + line_begin, len - line_begin));
        text_size.y = (float)total_lines * row_height;
      }

      {
        struct nk_text_find cursor_find;

        nk_textedit_find_charpos(&cursor_find, edit, edit->cursor, edit->single_line, font, row_height,
                                 (enum nk_text_affinity)edit->cursor_affinity);
        cursor_pos.x = cursor_find.x;
        cursor_pos.y = cursor_find.y;
      }

      {
        /* scrollbar */
        if (cursor_follow) {
          /* update scrollbar to follow cursor */
          if (!(flags & NK_EDIT_NO_HORIZONTAL_SCROLL)) {
            /* horizontal scroll */
            const float scroll_increment = area.w * 0.25f;

            if (cursor_pos.x < edit->scrollbar.x) {
              edit->scrollbar.x = (float)(int)NK_MAX(0.0f, cursor_pos.x - scroll_increment);
            }

            if (cursor_pos.x >= edit->scrollbar.x + area.w) {
              edit->scrollbar.x = (float)(int)NK_MAX(0.0f, cursor_pos.x - area.w + scroll_increment);
            }
          } else {
            edit->scrollbar.x = 0;
          }

          if (flags & NK_EDIT_MULTILINE) {
            /* vertical scroll: like horizontal, it only adjusts if the
             * cursor leaves the visible area, and then only just enough
             * to keep it visible */
            if (cursor_pos.y < edit->scrollbar.y) {
              edit->scrollbar.y = NK_MAX(0.0f, cursor_pos.y);
            }

            if (cursor_pos.y > edit->scrollbar.y + area.h - row_height) {
              edit->scrollbar.y = cursor_pos.y - area.h + row_height;
            }
          } else {
            edit->scrollbar.y = 0;
          }
        }

        /* scrollbar widget */
        if (flags & NK_EDIT_MULTILINE) {
          nk_flags       ws;
          struct nk_rect scroll;
          float          scroll_target;
          float          scroll_offset;
          float          scroll_step;
          float          scroll_inc;
          float          old_max_offset;
          float          new_max_offset;

          scroll   = area;
          scroll.x = (bounds.x + bounds.w - style->border) - style->scrollbar_size.x;
          scroll.w = style->scrollbar_size.x;

          scroll_offset = edit->scrollbar.y;
          scroll_step   = scroll.h * 0.10f;
          scroll_inc    = scroll.h * 0.01f;
          scroll_target = text_size.y;

          old_max_offset = edit->scrollbar_prev_max.y;
          new_max_offset = NK_MAX(scroll_target - scroll.h, 0.0f);

          if ((edit->scrollbar_flags & NK_SCROLL_HAS_PREV_MAX_Y) && scroll_offset >= NK_MAX(old_max_offset - 1.0f, 0.0f) && new_max_offset > old_max_offset) {
            scroll_offset = new_max_offset;
          }

          edit->scrollbar.y          = nk_do_scrollbarv(&ws, out, scroll, is_hovered, scroll_offset, scroll_target, scroll_step, scroll_inc, &style->scrollbar, in, font);
          edit->scrollbar_prev_max.y = new_max_offset;
          edit->scrollbar_flags     |= NK_SCROLL_HAS_PREV_MAX_Y;

          /* Eat mouse scroll if we're active */
          if (in && is_hovered && in->mouse.scroll_delta.y) {
            in->mouse.scroll_delta.y = 0;
          }
        }
      }

      /* draw text */
      {
        struct nk_color             background_color;
        struct nk_color             text_color;
        struct nk_color             sel_background_color;
        struct nk_color             sel_text_color;
        struct nk_color             cursor_color;
        const struct nk_style_item *background;

        nk_push_scissor(out, clip);

        /* select correct colors to draw */
        if (*state & NK_WIDGET_STATE_ACTIVED) {
          background           = &style->active;
          text_color           = style->text_active;
          sel_text_color       = style->selected_text_hover;
          sel_background_color = style->selected_hover;
          cursor_color         = style->cursor_hover;
        } else if (*state & NK_WIDGET_STATE_HOVER) {
          background           = &style->hover;
          text_color           = style->text_hover;
          sel_text_color       = style->selected_text_hover;
          sel_background_color = style->selected_hover;
          cursor_color         = style->cursor_hover;
        } else {
          background           = &style->normal;
          text_color           = style->text_normal;
          sel_text_color       = style->selected_text_normal;
          sel_background_color = style->selected_normal;
          cursor_color         = style->cursor_normal;
        }

        if (background->type == NK_STYLE_ITEM_IMAGE) {
          background_color = nk_rgba(0, 0, 0, 0);
        } else {
          background_color = background->data.color;
        }

        cursor_color         = nk_rgb_factor(cursor_color, style->color_factor);
        sel_text_color       = nk_rgb_factor(sel_text_color, style->color_factor);
        sel_background_color = nk_rgb_factor(sel_background_color, style->color_factor);

        nk_edit_draw_text_selection(out, style, area.x - edit->scrollbar.x, area.y - edit->scrollbar.y,
                                    edit, row_height, font, background_color, text_color,
                                    sel_background_color, sel_text_color);

        /* blinks: 0.8 s on, 0.4 s off */
        {
          struct nk_context *ctx   = out->context;
          float              blink = 0;

          if (cursor_follow || prev_state != edit->active) {
            ctx->edit_blink = 0;
          }

          blink          = ctx->edit_blink - (float)(int)(ctx->edit_blink / 1.2f) * 1.2f;
          ctx->animating = nk_true;

          if (edit->select_start == edit->select_end && blink <= 0.8f) {
            struct nk_rect cursor;

            cursor.w = NK_MAX(style->cursor_size, 1.0f);
            cursor.h = ctx->style.font_size;
            cursor.x = (float)(int)(area.x + cursor_pos.x - edit->scrollbar.x);
            cursor.y = area.y + cursor_pos.y + row_height / 2.0f - cursor.h / 2.0f - edit->scrollbar.y;
            nk_fill_rect(out, cursor, 0, cursor_color);
          }
        }
      }
    } else {
      /* not active so just draw text */
      int         l     = nk_str_len_char(&edit->string);
      const char *begin = nk_str_get_const(&edit->string);

      const struct nk_style_item *background;
      struct nk_color             background_color;
      struct nk_color             text_color;

      nk_push_scissor(out, clip);

      if (*state & NK_WIDGET_STATE_ACTIVED) {
        background = &style->active;
        text_color = style->text_active;
      } else if (*state & NK_WIDGET_STATE_HOVER) {
        background = &style->hover;
        text_color = style->text_hover;
      } else {
        background = &style->normal;
        text_color = style->text_normal;
      }

      if (background->type == NK_STYLE_ITEM_IMAGE) {
        background_color = nk_rgba(0, 0, 0, 0);
      } else {
        background_color = background->data.color;
      }

      nk_edit_draw_text(out, style, area.x - edit->scrollbar.x, area.y - edit->scrollbar.y, 0, begin, l, row_height, font, background_color, text_color, nk_false);
    }

    nk_push_scissor(out, old_clip);
  }
  return ret;
}

NK_API void
nk_edit_focus(struct nk_context *ctx, nk_flags flags)
{
  nk_hash           hash;
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win              = ctx->current;
  hash             = win->edit.seq;
  win->edit.active = nk_true;
  win->edit.name   = hash;

  if (flags & NK_EDIT_ALWAYS_INSERT_MODE) {
    win->edit.mode = NK_TEXT_EDIT_MODE_INSERT;
  }
}

NK_API void
nk_edit_unfocus(struct nk_context *ctx)
{
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win              = ctx->current;
  win->edit.active = nk_false;
  win->edit.name   = 0;
}

NK_API nk_flags
nk_edit_string(struct nk_context *ctx, nk_flags flags, char *memory, int *len, int max, nk_plugin_filter filter)
{
  nk_hash              hash;
  nk_flags             state;
  struct nk_text_edit *edit;
  struct nk_window    *win;

  NK_ASSERT(ctx);
  NK_ASSERT(memory);
  NK_ASSERT(len);

  if (!ctx || !memory || !len) {
    return 0;
  }

  filter = (!filter) ? nk_filter_default : filter;
  win    = ctx->current;
  hash   = win->edit.seq;
  edit   = &ctx->text_edit;
  nk_textedit_clear_state(&ctx->text_edit, (flags & NK_EDIT_MULTILINE) ? NK_TEXT_EDIT_MULTI_LINE : NK_TEXT_EDIT_SINGLE_LINE, filter);

  if (win->edit.active && hash == win->edit.name) {
    if (flags & NK_EDIT_NO_CURSOR) {
      edit->cursor = nk_utf_len(memory, *len);
    } else {
      edit->cursor = win->edit.cursor;
    }

    if (!(flags & NK_EDIT_SELECTABLE)) {
      edit->select_start = win->edit.cursor;
      edit->select_end   = win->edit.cursor;
    } else {
      edit->select_start = win->edit.sel_start;
      edit->select_end   = win->edit.sel_end;
    }

    edit->mode                 = win->edit.mode;
    edit->scrollbar.x          = (float)win->edit.scrollbar.x;
    edit->scrollbar.y          = (float)win->edit.scrollbar.y;
    edit->scrollbar_prev_max.x = (float)win->edit.scrollbar.prev_max_x;
    edit->scrollbar_prev_max.y = (float)win->edit.scrollbar.prev_max_y;
    edit->scrollbar_flags      = win->edit.scrollbar.flags;
    edit->active               = nk_true;
  } else {
    edit->active = nk_false;
  }

  max  = NK_MAX(1, max);
  *len = NK_MIN(*len, max - 1);
  nk_str_init_fixed(&edit->string, memory, (nk_size)max);
  edit->string.buffer.allocated = (nk_size)*len;
  edit->string.len              = nk_utf_len(memory, *len);
  state                         = nk_edit_buffer(ctx, flags, edit, filter);
  *len                          = (int)edit->string.buffer.allocated;

  if (edit->active) {
    win->edit.cursor               = edit->cursor;
    win->edit.sel_start            = edit->select_start;
    win->edit.sel_end              = edit->select_end;
    win->edit.mode                 = edit->mode;
    win->edit.scrollbar.x          = (nk_uint)edit->scrollbar.x;
    win->edit.scrollbar.y          = (nk_uint)edit->scrollbar.y;
    win->edit.scrollbar.prev_max_x = (nk_uint)edit->scrollbar_prev_max.x;
    win->edit.scrollbar.prev_max_y = (nk_uint)edit->scrollbar_prev_max.y;
    win->edit.scrollbar.flags      = edit->scrollbar_flags;
  }
  return state;
}

struct nk_tab_stop_info {
  nk_hash id;
  nk_hash prev;
};

/* true when the focus moves onto it now */
NK_LIB nk_bool
nk_tab_stop(struct nk_context *ctx, struct nk_window *win, enum nk_widget_layout_states state, struct nk_rect bounds,
            struct nk_tab_stop_info *out)
{
  struct nk_input *in     = &ctx->input;
  struct nk_panel *layout = win->layout;
  nk_bool          focus  = nk_false;
  nk_bool          target;

  out->id          = nk_murmur_hash(&layout->tab_seq, sizeof(layout->tab_seq), layout->text_selection_scope);
  out->prev        = win->tab_last;
  layout->tab_seq += 1;

  if (!out->id) {
    out->id = 1;
  }

  if (ctx->nav.enabled) {
    return nk_false;
  }

  if ((layout->flags & NK_WINDOW_ROM) || (win->flags & NK_WINDOW_NO_INPUT)) {
    return nk_false;
  }

  if (!win->tab_next && !win->tab_target && !win->edit.active && !win->property.active &&
      (nk_input_is_key_pressed(in, NK_KEY_FOCUS_NEXT) || nk_input_is_key_pressed(in, NK_KEY_FOCUS_PREV))) {
    nk_bool back = nk_input_is_key_pressed(in, NK_KEY_FOCUS_PREV);

    in->keyboard.keys[NK_KEY_FOCUS_NEXT].clicked = 0;
    in->keyboard.keys[NK_KEY_FOCUS_PREV].clicked = 0;

    if (back && win->tab_frame_last) {
      win->tab_target     = win->tab_frame_last;
      win->tab_target_new = nk_true;
    } else {
      win->tab_next = nk_true;
    }
  }

  target = win->tab_target == out->id;

  if (target) {
    win->tab_target_seen = nk_true;
  }

  if (target || win->tab_next) {
    if (state == NK_WIDGET_DISABLED) {
      if (target) {
        win->tab_target     = out->prev;
        win->tab_target_new = nk_true;
        win->tab_next       = !out->prev;
      }
    } else if (state == NK_WIDGET_INVALID) {
      if (layout->offset_y) {
        float top    = layout->clip.y;
        float bottom = layout->clip.y + layout->clip.h;

        if (bounds.y < top) {
          float delta = top - bounds.y;

          *layout->offset_y = (*layout->offset_y > (nk_uint)delta) ? *layout->offset_y - (nk_uint)delta : 0;
        } else if (bounds.y + bounds.h > bottom) {
          *layout->offset_y += (nk_uint)(bounds.y + bounds.h - bottom);
        }
      }

      win->tab_next        = nk_false;
      win->tab_target      = out->id;
      win->tab_target_seen = nk_true;
      ctx->animating       = nk_true;
    } else {
      win->tab_next   = nk_false;
      win->tab_target = 0;
      focus           = nk_true;
    }
  }

  win->tab_last = out->id;
  return focus;
}

NK_LIB nk_bool
nk_tab_pressed(struct nk_window *win, struct nk_input *in, const struct nk_tab_stop_info *stop)
{
  nk_bool back;

  if (!in || !(nk_input_is_key_pressed(in, NK_KEY_FOCUS_NEXT) || nk_input_is_key_pressed(in, NK_KEY_FOCUS_PREV))) {
    return nk_false;
  }

  back                                         = nk_input_is_key_pressed(in, NK_KEY_FOCUS_PREV);
  in->keyboard.keys[NK_KEY_FOCUS_NEXT].clicked = 0;
  in->keyboard.keys[NK_KEY_FOCUS_PREV].clicked = 0;

  if (back) {
    win->tab_target     = stop->prev ? stop->prev : win->tab_frame_last;
    win->tab_target_new = nk_true;
    win->tab_next       = !win->tab_target;
  } else {
    win->tab_next = nk_true;
  }
  return nk_true;
}

NK_INTERN void
nk_edit_number_step(struct nk_text_edit *edit, int step)
{
  char        text[64];
  int         len      = (int)edit->string.buffer.allocated;
  int         decimals = 0, i;
  double      value, scale = 1;
  nk_uint     whole, part;
  char        digits[32];
  int         n = 0;
  const char *dot;

  if (len >= (int)sizeof(text) - 1) {
    return;
  }

  if (len) {
    NK_MEMCPY(text, nk_str_get_const(&edit->string), (nk_size)len);
  }

  text[len] = 0;
  dot       = text;

  while (*dot && *dot != '.') {
    dot++;
  }

  if (*dot) {
    decimals = len - (int)(dot - text) - 1;
  }

  for (i = 0; i < decimals; ++i) {
    scale *= 10;
  }

  value = (len ? nk_strtod(text, 0) : 0) + (double)step;

  {
    double a = (value < 0 ? -value : value) * scale + 0.5;

    whole = (nk_uint)(a / scale);
    part  = (nk_uint)(a - (double)whole * scale);
  }

  len = 0;

  if (value < 0 && (whole || part)) {
    text[len++] = '-';
  }

  do {
    digits[n++] = (char)('0' + whole % 10);
    whole /= 10;
  } while (whole && n < (int)sizeof(digits));

  while (n) {
    text[len++] = digits[--n];
  }

  if (decimals) {
    text[len++] = '.';

    for (i = decimals - 1; i >= 0; --i) {
      text[len + i] = (char)('0' + part % 10);
      part /= 10;
    }

    len += decimals;
  }

  nk_str_clear(&edit->string);
  nk_str_append_text_char(&edit->string, text, len);
}

NK_INTERN void
nk_osk_preview(struct nk_osk_state *osk, const struct nk_text_edit *edit, float width)
{
  const char *text   = nk_str_get_const(&edit->string);
  int         len    = (int)edit->string.buffer.allocated;
  int         cursor = nk_textedit_byte_offset((struct nk_text_edit *)edit, edit->cursor);
  int         begin  = cursor, end = cursor;
  int         room   = NK_OSK_PREVIEW_SIZE - 1;

  (void)width;

  if (!text) {
    osk->preview_len = osk->preview_cursor = 0;
    return;
  }

  cursor = NK_CLAMP(0, cursor, len);
  begin  = cursor;
  end    = cursor;

  while (begin > 0 && text[begin - 1] != '\n') {
    begin--;
  }

  while (end < len && text[end] != '\n') {
    end++;
  }

  if (end - begin > room) {
    begin = NK_MAX(begin, cursor - room * 3 / 4);
    end   = NK_MIN(end, begin + room);
  }

  while (begin < cursor && (text[begin] & 0xC0) == 0x80) {
    begin++;
  }

  while (end > cursor && end < len && (text[end] & 0xC0) == 0x80) {
    end--;
  }

  osk->preview_len    = end - begin;
  osk->preview_cursor = cursor - begin;
  NK_MEMCPY(osk->preview, text + begin, (nk_size)osk->preview_len);
}

NK_API nk_flags
nk_edit_buffer(struct nk_context *ctx, nk_flags flags, struct nk_text_edit *edit, nk_plugin_filter filter)
{
  struct nk_window *win;
  struct nk_style  *style;
  struct nk_input  *in;

  enum nk_widget_layout_states state;
  struct nk_rect               bounds;

  nk_flags      ret_flags = 0;
  unsigned char prev_state;
  nk_hash       hash;
  nk_bool       focus;
  struct nk_tab_stop_info tab_stop = {0, 0};

  /* make sure correct values */
  NK_ASSERT(ctx);
  NK_ASSERT(edit);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  state = nk_widget_nav(&bounds, ctx);
  focus = !(flags & NK_EDIT_READ_ONLY) && nk_tab_stop(ctx, win, state, bounds, &tab_stop);

  if (!state) {
    return state;
  } else if (state == NK_WIDGET_DISABLED) {
    flags |= NK_EDIT_READ_ONLY;
  }

  in = ((win->layout->flags & NK_WINDOW_ROM) || state == NK_WIDGET_DISABLED) ? 0 : &ctx->input;

  /* check if edit is currently hot item */
  hash = win->edit.seq++;

  ctx->nav.hold = ctx->nav.osk.open && win->name == ctx->nav.osk.window && hash == ctx->nav.osk.edit && state != NK_WIDGET_DISABLED;

  if (ctx->nav.hold) {
    in = &ctx->input;
  }

  if (focus) {
    nk_textedit_clear_state(edit, (flags & NK_EDIT_MULTILINE) ? NK_TEXT_EDIT_MULTI_LINE : NK_TEXT_EDIT_SINGLE_LINE, filter ? filter : nk_filter_default);
    nk_textedit_select_all(edit);
    win->edit.active     = nk_true;
    win->edit.name       = hash;
    win->edit.mode       = NK_TEXT_EDIT_MODE_INSERT;
    win->property.active = 0;
    ret_flags           |= NK_EDIT_ACTIVATED;
  }

  if (win->edit.active && hash == win->edit.name) {
    if (flags & NK_EDIT_NO_CURSOR) {
      edit->cursor = edit->string.len;
    }

    if (!(flags & NK_EDIT_SELECTABLE) && !focus) {
      edit->select_start = edit->cursor;
      edit->select_end   = edit->cursor;
    }

    if (flags & NK_EDIT_CLIPBOARD) {
      edit->clip = ctx->clip;
    }

    edit->active = (unsigned char)win->edit.active;
  } else {
    edit->active = nk_false;
  }

  edit->mode = win->edit.mode;

  if (edit->active && ctx->nav.cancel && !ctx->nav.hold) {
    struct nk_edit_revert *revert = &ctx->edit_revert;

    if (revert->valid && revert->window == win->name && revert->edit == hash && !(flags & NK_EDIT_READ_ONLY)) {
      nk_str_clear(&edit->string);
      nk_str_append_text_char(&edit->string, revert->text, revert->len);
      edit->cursor = edit->select_start = edit->select_end = 0;
    }

    edit->active     = nk_false;
    win->edit.active = nk_false;
    ctx->nav.cancel  = nk_false;
    ret_flags       |= NK_EDIT_DEACTIVATED;
  }

  if (edit->active && ctx->nav.leave && !ctx->nav.hold) {
    edit->active     = nk_false;
    win->edit.active = nk_false;
    ret_flags       |= NK_EDIT_DEACTIVATED;
  }

  if (ctx->nav.number_edit && ctx->nav.number_window == win->name && ctx->nav.number_edit == hash + 1) { /* + 1: never 0 */
    struct nk_input *kin = &ctx->input;
    int dir = nk_nav_pressed(kin, NK_KEY_NAV_RIGHT) || nk_nav_pressed(kin, NK_KEY_NAV_UP)   ? 1
            : nk_nav_pressed(kin, NK_KEY_NAV_LEFT) || nk_nav_pressed(kin, NK_KEY_NAV_DOWN) ? -1
                                                                                           : 0;

    if (!edit->active || kin->keyboard.text_len || (kin->mouse.buttons[NK_BUTTON_LEFT].clicked && !ctx->nav.active) ||
        nk_input_is_key_pressed(kin, NK_KEY_BACKSPACE) || nk_input_is_key_pressed(kin, NK_KEY_DEL)) {
      ctx->nav.number_edit = 0;
    } else if (dir) {
      nk_edit_number_step(edit, dir);
      nk_textedit_select_all(edit);
      edit->cursor                             = edit->string.len;
      kin->keyboard.keys[NK_KEY_LEFT].clicked  = 0;
      kin->keyboard.keys[NK_KEY_RIGHT].clicked = 0;
      kin->keyboard.keys[NK_KEY_UP].clicked    = 0;
      kin->keyboard.keys[NK_KEY_DOWN].clicked  = 0;
    }
  }

  filter     = (!filter) ? nk_filter_default : filter;
  prev_state = (unsigned char)edit->active;
  ret_flags |= nk_do_edit(&ctx->last_widget_state, &win->buffer, bounds, flags, filter, edit, &style->edit, in, style->font);

  if (ctx->nav.hold) {
    ctx->nav.osk.seen  = edit->active;
    ctx->nav.osk.field = bounds;
    ctx->nav.hold      = nk_false;
    nk_osk_preview(&ctx->nav.osk, edit, bounds.w);
  } else if ((ret_flags & NK_EDIT_ACTIVATED) && ctx->nav.active && !(flags & NK_EDIT_READ_ONLY)) {
    /* typing replaces a number but adds to text */
    if (filter == nk_filter_decimal || filter == nk_filter_float) {
      nk_textedit_select_all(edit);
    } else {
      edit->select_start = edit->select_end = edit->string.len;
    }

    edit->cursor = edit->string.len;

    if (filter == nk_filter_decimal || filter == nk_filter_float) {
      ctx->nav.number_window = win->name;
      ctx->nav.number_edit   = hash + 1;
    }

    ctx->nav.text_request = nk_true;
    ctx->nav.text_bounds  = bounds;
    ctx->nav.text_window  = win->name;
    ctx->nav.text_edit    = hash;

    if (ctx->nav.osk_on_activate) {
      ctx->nav.osk_on_activate = nk_false;
      nk_osk_open(ctx);
    }
  }

  if ((ret_flags & NK_EDIT_ACTIVATED) && edit->active) {
    struct nk_edit_revert *revert = &ctx->edit_revert;
    int                    bytes  = (int)edit->string.buffer.allocated;

    revert->window = win->name;
    revert->edit   = hash;
    revert->valid  = bytes <= NK_EDIT_REVERT_SIZE;
    revert->len    = revert->valid ? bytes : 0;

    if (revert->valid && bytes) {
      NK_MEMCPY(revert->text, nk_str_get_const(&edit->string), (nk_size)bytes);
    }
  }

  if (edit->active && !focus && !(flags & NK_EDIT_ALLOW_TAB) && nk_tab_pressed(win, in, &tab_stop)) {
    edit->active = nk_false;
    ret_flags   &= ~(nk_flags)NK_EDIT_ACTIVE;
    ret_flags   |= NK_EDIT_INACTIVE | NK_EDIT_DEACTIVATED;
  }

  if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    ctx->style.cursor_active = ctx->style.cursors[NK_CURSOR_TEXT];
  }

  if (edit->active && prev_state != edit->active) {
    /* current edit is now hot */
    win->edit.active = nk_true;
    win->edit.name   = hash;
  } else if (prev_state && !edit->active) {
    /* current edit is now cold */
    win->edit.active = nk_false;
  }
  return ret_flags;
}

NK_API nk_flags
nk_edit_string_zero_terminated(struct nk_context *ctx, nk_flags flags, char *buffer, int max, nk_plugin_filter filter)
{
  nk_flags result;
  int      len = nk_strlen(buffer);

  result = nk_edit_string(ctx, flags, buffer, &len, max, filter);
  buffer[NK_MIN(NK_MAX(max - 1, 0), len)] = '\0';
  return result;
}

/* ===============================================================
 *
 *                              PROPERTY
 *
 * ===============================================================*/
NK_LIB void
nk_drag_behavior(nk_flags *state, const struct nk_input *in, struct nk_rect drag, struct nk_property_variant *variant, float inc_per_pixel)
{
  int left_mouse_down            = in && in->mouse.buttons[NK_BUTTON_LEFT].down;
  int left_mouse_click_in_cursor = in && nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, drag, nk_true);

  nk_widget_state_reset(state);

  if (nk_input_is_mouse_hovering_rect(in, drag)) {
    *state = NK_WIDGET_STATE_HOVERED;
  }

  if (left_mouse_down && left_mouse_click_in_cursor) {
    float delta, pixels;

    pixels = in->mouse.delta.x;
    delta  = pixels * inc_per_pixel;

    switch (variant->kind) {
      default:
        break;

      case NK_PROPERTY_INT:
        variant->value.i = variant->value.i + (int)delta;
        variant->value.i = NK_CLAMP(variant->min_value.i, variant->value.i, variant->max_value.i);
        break;

      case NK_PROPERTY_FLOAT:
        variant->value.f = variant->value.f + (float)delta;
        variant->value.f = NK_CLAMP(variant->min_value.f, variant->value.f, variant->max_value.f);
        break;

      case NK_PROPERTY_DOUBLE:
        variant->value.d = variant->value.d + (double)delta;
        variant->value.d = NK_CLAMP(variant->min_value.d, variant->value.d, variant->max_value.d);
        break;
    }

    *state = NK_WIDGET_STATE_ACTIVE;
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, drag)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, drag)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
}

NK_LIB void
nk_property_behavior(nk_flags                   *ws,
                     const struct nk_input      *in,
                     struct nk_rect              property,
                     struct nk_rect              label,
                     struct nk_rect              edit,
                     struct nk_rect              empty,
                     int                        *state,
                     struct nk_property_variant *variant,
                     float                       inc_per_pixel)
{
  nk_widget_state_reset(ws);

  if (in && *state == NK_PROPERTY_DEFAULT) {
    if (nk_button_behavior(ws, edit, in, NK_BUTTON_DEFAULT)) {
      *state = NK_PROPERTY_EDIT;
    } else if (nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, label, nk_true)) {
      *state = NK_PROPERTY_DRAG;
    } else if (nk_input_is_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, empty, nk_true)) {
      *state = NK_PROPERTY_DRAG;
    }
  }

  if (*state == NK_PROPERTY_DRAG) {
    nk_drag_behavior(ws, in, property, variant, inc_per_pixel);

    if (!(*ws & NK_WIDGET_STATE_ACTIVED)) {
      *state = NK_PROPERTY_DEFAULT;
    }
  }
}

NK_LIB void
nk_draw_property(struct nk_command_buffer       *out,
                 const struct nk_style_property *style,
                 const struct nk_rect           *bounds,
                 const struct nk_rect           *label,
                 nk_flags                        state,
                 const char                     *name,
                 int                             len,
                 const struct nk_user_font      *font)
{
  struct nk_text              text;
  const struct nk_style_item *background;

  /* select correct background and text color */
  if (state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->active;
    text.text  = style->label_active;
  } else if (state & NK_WIDGET_STATE_HOVER) {
    background = &style->hover;
    text.text  = style->label_hover;
  } else {
    background = &style->normal;
    text.text  = style->label_normal;
  }

  text.text = nk_rgb_factor(text.text, style->color_factor);

  /* draw background */
  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(out, *bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(out, *bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      text.background = background->data.color;
      nk_fill_rect(out, *bounds, style->rounding, nk_rgb_factor(background->data.color, style->color_factor));
      nk_stroke_rect(out, *bounds, style->rounding, style->border, nk_rgb_factor(style->border_color, style->color_factor));
      break;
  }

  /* draw label */
  text.padding = nk_vec2(0, 0);

  if (name && name[0] != '#') {
    nk_widget_text(out, *label, name, len, &text, NK_TEXT_CENTERED, font);
  }
}

NK_INTERN void
nk_property_save(struct nk_property_variant *variant, char *buffer, int len)
{
  buffer[len] = '\0';

  switch (variant->kind) {
    default:
      break;

    case NK_PROPERTY_INT:
      variant->value.i = nk_strtoi(buffer, 0);
      variant->value.i = NK_CLAMP(variant->min_value.i, variant->value.i, variant->max_value.i);
      break;

    case NK_PROPERTY_FLOAT:
      nk_string_float_limit(buffer, NK_MAX_FLOAT_PRECISION);
      variant->value.f = nk_strtof(buffer, 0);
      variant->value.f = NK_CLAMP(variant->min_value.f, variant->value.f, variant->max_value.f);
      break;

    case NK_PROPERTY_DOUBLE:
      nk_string_float_limit(buffer, NK_MAX_FLOAT_PRECISION);
      variant->value.d = NK_STRTOD(buffer, 0);
      variant->value.d = NK_CLAMP(variant->min_value.d, variant->value.d, variant->max_value.d);
      break;
  }
}

#define NK_BUTTON_REPEAT_DELAY 0.3f
#define NK_BUTTON_REPEAT_RATE  0.05f

NK_LIB nk_bool
nk_property_button(nk_flags                     *ws,
                   struct nk_command_buffer     *out,
                   struct nk_rect                bounds,
                   enum nk_symbol_type           symbol,
                   const struct nk_style_button *style,
                   const struct nk_input        *in,
                   const struct nk_user_font    *font)
{
  struct nk_context *ctx = out->context;
  float              before;
  float              after;

  if (!nk_do_button_symbol(ws, out, bounds, symbol, NK_BUTTON_REPEATER, style, in, font)) {
    return nk_false;
  }

  ctx->animating = nk_true;

  if (nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT)) {
    ctx->button_repeat_time = 0;
    return nk_true;
  }

  before                  = ctx->button_repeat_time;
  after                   = before + ctx->delta_time_seconds;
  ctx->button_repeat_time = after;

  if (after < NK_BUTTON_REPEAT_DELAY) {
    return nk_false;
  }
  return before < NK_BUTTON_REPEAT_DELAY ||
         (int)((after - NK_BUTTON_REPEAT_DELAY) / NK_BUTTON_REPEAT_RATE) != (int)((before - NK_BUTTON_REPEAT_DELAY) / NK_BUTTON_REPEAT_RATE);
}

NK_LIB void
nk_do_property(nk_flags                       *ws,
               struct nk_command_buffer       *out,
               struct nk_rect                  property,
               const char                     *name,
               struct nk_property_variant     *variant,
               float                           inc_per_pixel,
               char                           *buffer,
               int                            *len,
               int                            *state,
               int                            *cursor,
               int                            *select_begin,
               int                            *select_end,
               const struct nk_style_property *style,
               enum nk_property_filter         filter,
               struct nk_input                *in,
               const struct nk_user_font      *font,
               struct nk_text_edit            *text_edit,
               enum nk_button_behavior         behavior)
{
  const nk_plugin_filter filters[] = {nk_filter_decimal, nk_filter_float};
  nk_bool                active, old;
  nk_bool                nav_focus = nk_false, nav_typed = nk_false;
  int                    num_len   = 0, name_len = 0;
  char                   string[NK_MAX_NUMBER_BUFFER];
  float                  size;

  char *dst = 0;
  int  *length;

  struct nk_rect left;
  struct nk_rect right;
  struct nk_rect label;
  struct nk_rect edit;
  struct nk_rect empty;

  NK_UNUSED(behavior);

  /* left decrement button */
  left.h = out->context->style.font_size / 2;
  left.w = left.h;
  left.x = property.x + style->border + style->padding.x;
  left.y = property.y + style->border + property.h / 2.0f - left.h / 2;

  /* text label */
  if (name && name[0] != '#') {
    name_len = nk_strlen(name);
  }

  size    = nk_text_width(out->context, font, out->context->style.font_size, name, name_len);
  label.x = left.x + left.w + style->padding.x;
  label.w = (float)size + 2 * style->padding.x;
  label.y = property.y + style->border + style->padding.y;
  label.h = property.h - (2 * style->border + 2 * style->padding.y);

  /* right increment button */
  right.y = left.y;
  right.w = left.w;
  right.h = left.h;
  right.x = property.x + property.w - (right.w + style->padding.x);

  /* edit */
  if (*state == NK_PROPERTY_EDIT) {
    size   = nk_text_width(out->context, font, out->context->style.font_size, buffer, *len);
    size  += style->edit.cursor_size;
    length = len;
    dst    = buffer;
  } else {
    switch (variant->kind) {
      default:
        break;

      case NK_PROPERTY_INT:
        nk_itoa(string, variant->value.i);
        num_len = nk_strlen(string);
        break;

      case NK_PROPERTY_FLOAT:
        NK_DTOA(string, (double)variant->value.f);
        num_len = nk_string_float_limit(string, NK_MAX_FLOAT_PRECISION);
        break;

      case NK_PROPERTY_DOUBLE:
        NK_DTOA(string, variant->value.d);
        num_len = nk_string_float_limit(string, NK_MAX_FLOAT_PRECISION);
        break;
    }

    size   = nk_text_width(out->context, font, out->context->style.font_size, string, num_len);
    dst    = string;
    length = &num_len;
  }

  edit.w = (float)size + 2 * style->padding.x;
  edit.w = NK_MIN(edit.w, right.x - (label.x + label.w));
  edit.x = right.x - (edit.w + style->padding.x);
  edit.y = property.y + style->border;
  edit.h = property.h - (2 * style->border);

  /* empty left space activator */
  empty.w = edit.x - (label.x + label.w);
  empty.x = label.x + label.w;
  empty.y = property.y;
  empty.h = property.h;

  nav_focus = nk_nav_stop(out->context, property, NK_NAV_STOP_ADJUST | (*state != NK_PROPERTY_DEFAULT ? NK_NAV_STOP_ACTIVE : 0));

  if (nav_focus && out->context->nav.adjust) {
    int a = out->context->nav.adjust;

    switch (variant->kind) {
      default:
        break;

      case NK_PROPERTY_INT:
        variant->value.i = NK_CLAMP(variant->min_value.i, variant->value.i + a * variant->step.i, variant->max_value.i);
        break;

      case NK_PROPERTY_FLOAT:
        variant->value.f = NK_CLAMP(variant->min_value.f, variant->value.f + (float)a * variant->step.f, variant->max_value.f);
        break;

      case NK_PROPERTY_DOUBLE:
        variant->value.d = NK_CLAMP(variant->min_value.d, variant->value.d + (double)a * variant->step.d, variant->max_value.d);
        break;
    }
  }

  if (out->context) {
    out->context->nav.suppress = nk_true;
  }

  if (*state == NK_PROPERTY_EDIT && out->context && out->context->nav.cancel) {
    *state                   = NK_PROPERTY_DEFAULT;
    out->context->nav.cancel = nk_false;
  }

  /* update property */
  old = (*state == NK_PROPERTY_EDIT);

  if (nav_focus && out->context->nav.captured && *state == NK_PROPERTY_DEFAULT && in && in->keyboard.text_len) {
    *state                     = NK_PROPERTY_EDIT;
    out->context->nav.captured = nk_false;
    nav_typed                  = nk_true;
  }

  nk_property_behavior(ws, in, property, label, edit, empty, state, variant, inc_per_pixel);

  /* draw property */
  if (style->draw_begin) {
    style->draw_begin(out, style->userdata);
  }

  nk_draw_property(out, style, &property, &label, *ws, name, name_len, font);

  if (style->draw_end) {
    style->draw_end(out, style->userdata);
  }

  /* execute right button  */
  if (nk_property_button(ws, out, left, style->sym_left, &style->dec_button, in, font)) {
    switch (variant->kind) {
      default:
        break;

      case NK_PROPERTY_INT:
        variant->value.i = NK_CLAMP(variant->min_value.i, variant->value.i - variant->step.i, variant->max_value.i);
        break;

      case NK_PROPERTY_FLOAT:
        variant->value.f = NK_CLAMP(variant->min_value.f, variant->value.f - variant->step.f, variant->max_value.f);
        break;

      case NK_PROPERTY_DOUBLE:
        variant->value.d = NK_CLAMP(variant->min_value.d, variant->value.d - variant->step.d, variant->max_value.d);
        break;
    }
  }

  /* execute left button  */
  if (nk_property_button(ws, out, right, style->sym_right, &style->inc_button, in, font)) {
    switch (variant->kind) {
      default:
        break;

      case NK_PROPERTY_INT:
        variant->value.i = NK_CLAMP(variant->min_value.i, variant->value.i + variant->step.i, variant->max_value.i);
        break;

      case NK_PROPERTY_FLOAT:
        variant->value.f = NK_CLAMP(variant->min_value.f, variant->value.f + variant->step.f, variant->max_value.f);
        break;

      case NK_PROPERTY_DOUBLE:
        variant->value.d = NK_CLAMP(variant->min_value.d, variant->value.d + variant->step.d, variant->max_value.d);
        break;
    }
  }

  if (!old && (*state == NK_PROPERTY_EDIT)) {
    /* property has been activated so setup buffer */
    NK_MEMCPY(buffer, dst, (nk_size)*length);
    *cursor = nk_utf_len(buffer, *length);
    *len    = *length;
    length  = len;
    dst     = buffer;
    active  = 0;

    if (nav_typed) {
      active        = 1;
      *select_begin = 0;
      *select_end   = *cursor;
    }
  } else {
    active = (*state == NK_PROPERTY_EDIT);
  }

  /* execute and run text edit field */
  nk_textedit_clear_state(text_edit, NK_TEXT_EDIT_SINGLE_LINE, filters[filter]);
  text_edit->active                    = (unsigned char)active;
  text_edit->string.len                = *length;
  text_edit->cursor                    = NK_CLAMP(0, *cursor, *length);
  text_edit->select_start              = NK_CLAMP(0, *select_begin, *length);
  text_edit->select_end                = NK_CLAMP(0, *select_end, *length);
  text_edit->string.buffer.allocated   = (nk_size)*length;
  text_edit->string.buffer.memory.size = NK_MAX_NUMBER_BUFFER;
  text_edit->string.buffer.memory.ptr  = dst;
  text_edit->string.buffer.size        = NK_MAX_NUMBER_BUFFER;
  text_edit->mode                      = NK_TEXT_EDIT_MODE_INSERT;
  nk_do_edit(ws, out, edit, NK_EDIT_ALWAYS_INSERT_MODE | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD | NK_EDIT_AUTO_SELECT, filters[filter], text_edit, &style->edit, (*state == NK_PROPERTY_EDIT) ? in : 0, font);

  *length       = text_edit->string.len;
  *cursor       = text_edit->cursor;
  *select_begin = text_edit->select_start;
  *select_end   = text_edit->select_end;

  if (text_edit->active && (nk_input_is_key_pressed(in, NK_KEY_ENTER) || nk_input_is_key_pressed(in, NK_KEY_FOCUS_NEXT) ||
                            nk_input_is_key_pressed(in, NK_KEY_FOCUS_PREV))) {
    text_edit->active = nk_false;
  }

  if (active && !text_edit->active) {
    /* property is now not active so convert edit text to value*/
    *state = NK_PROPERTY_DEFAULT;
    nk_property_save(variant, buffer, *len);
  }

  if (out->context) {
    out->context->nav.suppress = nk_false;
  }
}

NK_LIB struct nk_property_variant
nk_property_variant_int(int value, int min_value, int max_value, int step)
{
  struct nk_property_variant result;

  result.kind        = NK_PROPERTY_INT;
  result.value.i     = value;
  result.min_value.i = min_value;
  result.max_value.i = max_value;
  result.step.i      = step;
  return result;
}

NK_LIB struct nk_property_variant
nk_property_variant_float(float value, float min_value, float max_value, float step)
{
  struct nk_property_variant result;

  result.kind        = NK_PROPERTY_FLOAT;
  result.value.f     = value;
  result.min_value.f = min_value;
  result.max_value.f = max_value;
  result.step.f      = step;
  return result;
}

NK_LIB struct nk_property_variant
nk_property_variant_double(double value, double min_value, double max_value, double step)
{
  struct nk_property_variant result;

  result.kind        = NK_PROPERTY_DOUBLE;
  result.value.d     = value;
  result.min_value.d = min_value;
  result.max_value.d = max_value;
  result.step.d      = step;
  return result;
}

NK_LIB int
nk_property_text(const struct nk_property_variant *variant, char *out)
{
  switch (variant->kind) {
    default:
      out[0] = 0;
      return 0;

    case NK_PROPERTY_INT:
      nk_itoa(out, variant->value.i);
      return nk_strlen(out);

    case NK_PROPERTY_FLOAT:
      NK_DTOA(out, (double)variant->value.f);
      return nk_string_float_limit(out, NK_MAX_FLOAT_PRECISION);

    case NK_PROPERTY_DOUBLE:
      NK_DTOA(out, variant->value.d);
      return nk_string_float_limit(out, NK_MAX_FLOAT_PRECISION);
  }
}

NK_LIB void
nk_property(struct nk_context *ctx, const char *name, struct nk_property_variant *variant, float inc_per_pixel, const enum nk_property_filter filter)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  struct nk_input       *in;
  const struct nk_style *style;

  struct nk_rect               bounds;
  enum nk_widget_layout_states s;
  nk_bool                      hot;

  int    *state        = 0;
  nk_hash hash         = 0;
  char   *buffer       = 0;
  int    *len          = 0;
  int    *cursor       = 0;
  int    *select_begin = 0;
  int    *select_end   = 0;
  int     old_state;
  int     prev_state;
  nk_bool focus;
  struct nk_tab_stop_info tab_stop = {0, 0};

  char dummy_buffer[NK_MAX_NUMBER_BUFFER];
  int  dummy_state        = NK_PROPERTY_DEFAULT;
  int  dummy_length       = 0;
  int  dummy_cursor       = 0;
  int  dummy_select_begin = 0;
  int  dummy_select_end   = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  win    = ctx->current;
  layout = win->layout;
  style  = &ctx->style;
  s      = nk_widget_nav(&bounds, ctx);
  focus  = nk_tab_stop(ctx, win, s, bounds, &tab_stop);

  if (!s) {
    return;
  }

  /* calculate hash from name */
  if (name[0] == '#') {
    hash = nk_murmur_hash(name, (int)nk_strlen(name), win->property.seq++);
    name++; /* special number hash */
  } else {
    hash = nk_murmur_hash(name, (int)nk_strlen(name), 42);
  }

  /* check if property is previously hot */
  if (win->property.prev_state == NK_PROPERTY_EDIT && hash == win->property.prev_name) {
    nk_property_save(variant, win->property.prev_buffer, win->property.prev_length);
    win->property.prev_state = NK_PROPERTY_DEFAULT;
  }

  if (focus) {
    win->property.active       = 1;
    win->property.name         = hash;
    win->property.state        = NK_PROPERTY_EDIT;
    win->property.length       = nk_property_text(variant, win->property.buffer);
    win->property.cursor       = win->property.length;
    win->property.select_start = 0;
    win->property.select_end   = win->property.length;
    win->edit.active           = nk_true;
    win->edit.name             = (nk_hash)-1;
  }

  /* check if property is currently hot item */
  hot = win->property.active && hash == win->property.name;

  if (hot) {
    buffer       = win->property.buffer;
    len          = &win->property.length;
    cursor       = &win->property.cursor;
    state        = &win->property.state;
    select_begin = &win->property.select_start;
    select_end   = &win->property.select_end;
  } else {
    buffer       = dummy_buffer;
    len          = &dummy_length;
    cursor       = &dummy_cursor;
    state        = &dummy_state;
    select_begin = &dummy_select_begin;
    select_end   = &dummy_select_end;
  }

  /* execute property widget */
  old_state           = *state;
  prev_state          = win->property.state;
  ctx->text_edit.clip = ctx->clip;
  in                  = ((s == NK_WIDGET_ROM && !win->property.active) || layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED) ? 0 : &ctx->input;
  nk_do_property(&ctx->last_widget_state,
                 &win->buffer,
                 bounds,
                 name,
                 variant,
                 inc_per_pixel,
                 buffer,
                 len,
                 state,
                 cursor,
                 select_begin,
                 select_end,
                 &style->property,
                 filter,
                 in,
                 style->font,
                 &ctx->text_edit,
                 ctx->button_behavior);

  if (in && *state != NK_PROPERTY_DEFAULT && !hot) {
    /* another property was active */
    if (win->property.active /* && hash != win->property.name */) {
      win->property.prev_state  = prev_state;
      win->property.prev_name   = win->property.name;
      win->property.prev_length = win->property.length;
      NK_MEMCPY(win->property.prev_buffer, win->property.buffer, (nk_size)win->property.length);
    }

    /* current property is now hot */
    win->property.active = 1;
    NK_MEMCPY(win->property.buffer, buffer, (nk_size)*len);
    win->property.length       = *len;
    win->property.cursor       = *cursor;
    win->property.state        = *state;
    win->property.name         = hash;
    win->property.select_start = *select_begin;
    win->property.select_end   = *select_end;
    win->edit.active           = nk_true;
    win->edit.name             = (nk_hash)-1;

    if (*state == NK_PROPERTY_DRAG) {
      ctx->input.mouse.grab    = nk_true;
      ctx->input.mouse.grabbed = nk_true;
    }
  }

  if (hot && old_state == NK_PROPERTY_EDIT && *state == NK_PROPERTY_DEFAULT) {
    nk_tab_pressed(win, in, &tab_stop);
  }

  /* check if previously active property is now inactive */
  if (*state == NK_PROPERTY_DEFAULT && old_state != NK_PROPERTY_DEFAULT) {
    if (old_state == NK_PROPERTY_DRAG) {
      ctx->input.mouse.grab    = nk_false;
      ctx->input.mouse.grabbed = nk_false;
      ctx->input.mouse.ungrab  = nk_true;
    }

    win->property.select_start = 0;
    win->property.select_end   = 0;
    win->property.active       = 0;
    win->edit.active           = nk_false;
  }
}

NK_API nk_bool
nk_property_int(struct nk_context *ctx, const char *name, int min, int *val, int max, int step, float inc_per_pixel)
{
  struct nk_property_variant variant;
  nk_bool                    changed;

  NK_ASSERT(ctx);
  NK_ASSERT(name);
  NK_ASSERT(val);

  if (!ctx || !ctx->current || !name || !val) {
    return nk_false;
  }

  variant = nk_property_variant_int(*val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_INT);
  changed = variant.value.i != *val;
  *val    = variant.value.i;
  return changed;
}

NK_API nk_bool
nk_property_float(struct nk_context *ctx, const char *name, float min, float *val, float max, float step, float inc_per_pixel)
{
  struct nk_property_variant variant;
  nk_bool                    changed;

  NK_ASSERT(ctx);
  NK_ASSERT(name);
  NK_ASSERT(val);

  if (!ctx || !ctx->current || !name || !val) {
    return nk_false;
  }

  variant = nk_property_variant_float(*val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_FLOAT);
  changed = variant.value.f != *val;
  *val    = variant.value.f;
  return changed;
}

NK_API nk_bool
nk_property_double(struct nk_context *ctx, const char *name, double min, double *val, double max, double step, float inc_per_pixel)
{
  struct nk_property_variant variant;
  nk_bool                    changed;

  NK_ASSERT(ctx);
  NK_ASSERT(name);
  NK_ASSERT(val);

  if (!ctx || !ctx->current || !name || !val) {
    return nk_false;
  }

  variant = nk_property_variant_double(*val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_FLOAT);
  changed = variant.value.d != *val;
  *val    = variant.value.d;
  return changed;
}

NK_API int
nk_propertyi(struct nk_context *ctx, const char *name, int min, int val, int max, int step, float inc_per_pixel)
{
  struct nk_property_variant variant;

  NK_ASSERT(ctx);
  NK_ASSERT(name);

  if (!ctx || !ctx->current || !name) {
    return val;
  }

  variant = nk_property_variant_int(val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_INT);
  val = variant.value.i;
  return val;
}

NK_API float
nk_propertyf(struct nk_context *ctx, const char *name, float min, float val, float max, float step, float inc_per_pixel)
{
  struct nk_property_variant variant;

  NK_ASSERT(ctx);
  NK_ASSERT(name);

  if (!ctx || !ctx->current || !name) {
    return val;
  }

  variant = nk_property_variant_float(val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_FLOAT);
  val = variant.value.f;
  return val;
}

NK_API double
nk_propertyd(struct nk_context *ctx, const char *name, double min, double val, double max, double step, float inc_per_pixel)
{
  struct nk_property_variant variant;

  NK_ASSERT(ctx);
  NK_ASSERT(name);

  if (!ctx || !ctx->current || !name) {
    return val;
  }

  variant = nk_property_variant_double(val, min, max, step);
  nk_property(ctx, name, &variant, inc_per_pixel, NK_FILTER_FLOAT);
  val = variant.value.d;
  return val;
}

/* ==============================================================
 *
 *                          CHART
 *
 * ===============================================================*/
NK_API nk_bool
nk_chart_begin_colored(struct nk_context *ctx, enum nk_chart_type type, struct nk_color color, struct nk_color highlight, int count, float min_value, float max_value)
{
  struct nk_window            *win;
  struct nk_chart             *chart;
  const struct nk_style       *config;
  const struct nk_style_chart *style;

  const struct nk_style_item *background;
  struct nk_rect              bounds = {0, 0, 0, 0};

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  if (!nk_widget(&bounds, ctx)) {
    chart = &ctx->current->layout->chart;
    nk_zero(chart, sizeof(*chart));
    return 0;
  }

  win    = ctx->current;
  config = &ctx->style;
  chart  = &win->layout->chart;
  style  = &config->chart;

  /* setup basic generic chart  */
  nk_zero(chart, sizeof(*chart));
  chart->x = bounds.x + style->padding.x;
  chart->y = bounds.y + style->padding.y;
  chart->w = bounds.w - 2 * style->padding.x;
  chart->h = bounds.h - 2 * style->padding.y;
  chart->w = NK_MAX(chart->w, 2 * style->padding.x);
  chart->h = NK_MAX(chart->h, 2 * style->padding.y);

  /* add first slot into chart */
  {
    struct nk_chart_slot *slot = &chart->slots[chart->slot++];

    slot->type         = type;
    slot->count        = count;
    slot->color        = nk_rgb_factor(color, style->color_factor);
    slot->highlight    = highlight;
    slot->min          = NK_MIN(min_value, max_value);
    slot->max          = NK_MAX(min_value, max_value);
    slot->range        = slot->max - slot->min;
    slot->show_markers = style->show_markers;
  }

  /* draw chart background */
  background = &style->background;

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(&win->buffer, bounds, &background->data.image, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(&win->buffer, bounds, &background->data.slice, nk_rgb_factor(nk_white, style->color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(&win->buffer, bounds, style->rounding, nk_rgb_factor(style->border_color, style->color_factor));
      nk_fill_rect(&win->buffer, nk_shrink_rect(bounds, style->border), style->rounding, nk_rgb_factor(style->background.data.color, style->color_factor));
      break;
  }
  return 1;
}

NK_API nk_bool
nk_chart_begin(struct nk_context *ctx, const enum nk_chart_type type, int count, float min_value, float max_value)
{
  return nk_chart_begin_colored(ctx, type, ctx->style.chart.color, ctx->style.chart.selected_color, count, min_value, max_value);
}

NK_API void
nk_chart_add_slot_colored(struct nk_context *ctx, const enum nk_chart_type type, struct nk_color color, struct nk_color highlight, int count, float min_value, float max_value)
{
  const struct nk_style_chart *style;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  NK_ASSERT(ctx->current->layout->chart.slot < NK_CHART_MAX_SLOT);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return;
  }

  if (ctx->current->layout->chart.slot >= NK_CHART_MAX_SLOT) {
    return;
  }

  style = &ctx->style.chart;

  /* add another slot into the graph */
  {
    struct nk_chart      *chart = &ctx->current->layout->chart;
    struct nk_chart_slot *slot  = &chart->slots[chart->slot++];

    slot->type         = type;
    slot->count        = count;
    slot->color        = nk_rgb_factor(color, style->color_factor);
    slot->highlight    = highlight;
    slot->min          = NK_MIN(min_value, max_value);
    slot->max          = NK_MAX(min_value, max_value);
    slot->range        = slot->max - slot->min;
    slot->show_markers = style->show_markers;
  }
}

NK_API void
nk_chart_add_slot(struct nk_context *ctx, const enum nk_chart_type type, int count, float min_value, float max_value)
{
  nk_chart_add_slot_colored(ctx, type, ctx->style.chart.color, ctx->style.chart.selected_color, count, min_value, max_value);
}

NK_INTERN nk_flags
nk_chart_push_line(struct nk_context *ctx, struct nk_window *win, struct nk_chart *g, float value, int slot)
{
  struct nk_panel          *layout = win->layout;
  const struct nk_input    *i      = ctx->current->widgets_disabled ? 0 : &ctx->input;
  struct nk_command_buffer *out    = &win->buffer;

  nk_flags        ret = 0;
  struct nk_vec2  cur;
  struct nk_rect  bounds;
  struct nk_color color;
  float           step;
  float           range;
  float           ratio;

  NK_ASSERT(slot >= 0 && slot < NK_CHART_MAX_SLOT);
  step  = g->w / (float)g->slots[slot].count;
  range = g->slots[slot].max - g->slots[slot].min;
  ratio = (value - g->slots[slot].min) / range;

  if (g->slots[slot].index == 0) {
    /* first data point does not have a connection */
    g->slots[slot].last.x = g->x;
    g->slots[slot].last.y = (g->y + g->h) - ratio * (float)g->h;

    bounds.x = g->slots[slot].last.x - 2;
    bounds.y = g->slots[slot].last.y - 2;
    bounds.w = bounds.h = 4;

    color = g->slots[slot].color;

    if (!(layout->flags & NK_WINDOW_ROM) && i && NK_INBOX(i->mouse.pos.x, i->mouse.pos.y, g->slots[slot].last.x - 3, g->slots[slot].last.y - 3, 6, 6)) {
      ret   = nk_input_is_mouse_hovering_rect(i, bounds) ? NK_CHART_HOVERING : 0;
      ret  |= (i->mouse.buttons[NK_BUTTON_LEFT].down && i->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED : 0;
      color = g->slots[slot].highlight;
    }

    if (g->slots[slot].show_markers) {
      nk_fill_rect(out, bounds, 0, color);
    }

    g->slots[slot].index += 1;
    return ret;
  }

  /* draw a line between the last data point and the new one */
  color = g->slots[slot].color;
  cur.x = g->x + (float)(step * (float)g->slots[slot].index);
  cur.y = (g->y + g->h) - (ratio * (float)g->h);
  nk_stroke_line(out, g->slots[slot].last.x, g->slots[slot].last.y, cur.x, cur.y, 1.0f, color);

  bounds.x = cur.x - 3;
  bounds.y = cur.y - 3;
  bounds.w = bounds.h = 6;

  /* user selection of current data point */
  if (!(layout->flags & NK_WINDOW_ROM)) {
    if (nk_input_is_mouse_hovering_rect(i, bounds)) {
      ret   = NK_CHART_HOVERING;
      ret  |= (!i->mouse.buttons[NK_BUTTON_LEFT].down && i->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED : 0;
      color = g->slots[slot].highlight;
    }
  }

  if (g->slots[slot].show_markers) {
    nk_fill_rect(out, nk_rect(cur.x - 2, cur.y - 2, 4, 4), 0, color);
  }

  /* save current data point position */
  g->slots[slot].last.x = cur.x;
  g->slots[slot].last.y = cur.y;
  g->slots[slot].index += 1;
  return ret;
}

NK_INTERN nk_flags
nk_chart_push_column(const struct nk_context *ctx, struct nk_window *win, struct nk_chart *chart, float value, int slot)
{
  struct nk_command_buffer *out    = &win->buffer;
  const struct nk_input    *in     = ctx->current->widgets_disabled ? 0 : &ctx->input;
  struct nk_panel          *layout = win->layout;

  float           ratio;
  nk_flags        ret = 0;
  struct nk_color color;
  struct nk_rect  item = {0, 0, 0, 0};

  NK_ASSERT(slot >= 0 && slot < NK_CHART_MAX_SLOT);

  if (chart->slots[slot].index >= chart->slots[slot].count) {
    return nk_false;
  }

  if (chart->slots[slot].count) {
    float padding = (float)(chart->slots[slot].count - 1);

    item.w = (chart->w - padding) / (float)(chart->slots[slot].count);
  }

  /* calculate bounds of current bar chart entry */
  color  = chart->slots[slot].color;
  item.h = chart->h * NK_ABS((value / chart->slots[slot].range));

  if (value >= 0) {
    ratio  = (value + NK_ABS(chart->slots[slot].min)) / NK_ABS(chart->slots[slot].range);
    item.y = (chart->y + chart->h) - chart->h * ratio;
  } else {
    ratio  = (value - chart->slots[slot].max) / chart->slots[slot].range;
    item.y = chart->y + (chart->h * NK_ABS(ratio)) - item.h;
  }

  item.x = chart->x + ((float)chart->slots[slot].index * item.w);
  item.x = item.x + ((float)chart->slots[slot].index);

  /* user chart bar selection */
  if (!(layout->flags & NK_WINDOW_ROM) && in && NK_INBOX(in->mouse.pos.x, in->mouse.pos.y, item.x, item.y, item.w, item.h)) {
    ret   = NK_CHART_HOVERING;
    ret  |= (!in->mouse.buttons[NK_BUTTON_LEFT].down && in->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED : 0;
    color = chart->slots[slot].highlight;
  }

  nk_fill_rect(out, item, 0, color);
  chart->slots[slot].index += 1;
  return ret;
}

NK_API nk_flags
nk_chart_push_slot(struct nk_context *ctx, float value, int slot)
{
  nk_flags          flags;
  struct nk_window *win;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(slot >= 0 && slot < NK_CHART_MAX_SLOT);
  NK_ASSERT(slot < ctx->current->layout->chart.slot);

  if (!ctx || !ctx->current || slot >= NK_CHART_MAX_SLOT) {
    return nk_false;
  }

  if (slot >= ctx->current->layout->chart.slot) {
    return nk_false;
  }

  win = ctx->current;

  if (win->layout->chart.slot < slot) {
    return nk_false;
  }

  switch (win->layout->chart.slots[slot].type) {
    case NK_CHART_LINES:
      flags = nk_chart_push_line(ctx, win, &win->layout->chart, value, slot);
      break;

    case NK_CHART_COLUMN:
      flags = nk_chart_push_column(ctx, win, &win->layout->chart, value, slot);
      break;

    default:
    case NK_CHART_MAX:
      flags = 0;
  }
  return flags;
}

NK_API nk_flags
nk_chart_push(struct nk_context *ctx, float value)
{
  return nk_chart_push_slot(ctx, value, 0);
}

NK_API void
nk_chart_end(struct nk_context *ctx)
{
  struct nk_window *win;
  struct nk_chart  *chart;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  win   = ctx->current;
  chart = &win->layout->chart;
  NK_MEMSET(chart, 0, sizeof(*chart));
  return;
}

NK_API void
nk_plot(struct nk_context *ctx, enum nk_chart_type type, const float *values, int count, int offset)
{
  int   i = 0;
  float min_value;
  float max_value;

  NK_ASSERT(ctx);
  NK_ASSERT(values);

  if (!ctx || !values || !count) {
    return;
  }

  min_value = values[offset];
  max_value = values[offset];

  for (i = 0; i < count; ++i) {
    min_value = NK_MIN(values[i + offset], min_value);
    max_value = NK_MAX(values[i + offset], max_value);
  }

  if (nk_chart_begin(ctx, type, count, min_value, max_value)) {
    for (i = 0; i < count; ++i) {
      nk_chart_push(ctx, values[i + offset]);
    }

    nk_chart_end(ctx);
  }
}

NK_API void
nk_plot_function(struct nk_context *ctx, enum nk_chart_type type, void *userdata, float (*value_getter)(void *user, int index), int count, int offset)
{
  int   i = 0;
  float min_value;
  float max_value;

  NK_ASSERT(ctx);
  NK_ASSERT(value_getter);

  if (!ctx || !value_getter || !count) {
    return;
  }

  max_value = min_value = value_getter(userdata, offset);

  for (i = 0; i < count; ++i) {
    float value = value_getter(userdata, i + offset);

    min_value = NK_MIN(value, min_value);
    max_value = NK_MAX(value, max_value);
  }

  if (nk_chart_begin(ctx, type, count, min_value, max_value)) {
    for (i = 0; i < count; ++i) {
      nk_chart_push(ctx, value_getter(userdata, i + offset));
    }

    nk_chart_end(ctx);
  }
}

/* ==============================================================
 *
 *                          COLOR PICKER
 *
 * ===============================================================*/
NK_LIB nk_bool
nk_color_picker_behavior(nk_flags              *state,
                         const struct nk_rect  *bounds,
                         const struct nk_rect  *matrix,
                         const struct nk_rect  *hue_bar,
                         const struct nk_rect  *alpha_bar,
                         struct nk_colorf      *color,
                         const struct nk_input *in)
{
  float   hsva[4];
  nk_bool value_changed = 0;
  nk_bool hsv_changed   = 0;

  NK_ASSERT(state);
  NK_ASSERT(matrix);
  NK_ASSERT(hue_bar);
  NK_ASSERT(color);

  /* color matrix */
  nk_colorf_hsva_fv(hsva, *color);

  if (nk_button_behavior(state, *matrix, in, NK_BUTTON_REPEATER)) {
    hsva[1]       = NK_SATURATE((in->mouse.pos.x - matrix->x) / (matrix->w - 1));
    hsva[2]       = 1.0f - NK_SATURATE((in->mouse.pos.y - matrix->y) / (matrix->h - 1));
    value_changed = hsv_changed = 1;
  }

  /* hue bar */
  if (nk_button_behavior(state, *hue_bar, in, NK_BUTTON_REPEATER)) {
    hsva[0]       = NK_SATURATE((in->mouse.pos.y - hue_bar->y) / (hue_bar->h - 1));
    value_changed = hsv_changed = 1;
  }

  /* alpha bar */
  if (alpha_bar) {
    if (nk_button_behavior(state, *alpha_bar, in, NK_BUTTON_REPEATER)) {
      hsva[3]       = 1.0f - NK_SATURATE((in->mouse.pos.y - alpha_bar->y) / (alpha_bar->h - 1));
      value_changed = 1;
    }
  }

  nk_widget_state_reset(state);

  if (hsv_changed) {
    *color = nk_hsva_colorfv(hsva);
    *state = NK_WIDGET_STATE_ACTIVE;
  }

  if (value_changed) {
    color->a = hsva[3];
    *state   = NK_WIDGET_STATE_ACTIVE;
  }

  /* set color picker widget state */
  if (nk_input_is_mouse_hovering_rect(in, *bounds)) {
    *state = NK_WIDGET_STATE_HOVERED;
  }

  if (*state & NK_WIDGET_STATE_HOVER && !nk_input_is_mouse_prev_hovering_rect(in, *bounds)) {
    *state |= NK_WIDGET_STATE_ENTERED;
  } else if (nk_input_is_mouse_prev_hovering_rect(in, *bounds)) {
    *state |= NK_WIDGET_STATE_LEFT;
  }
  return value_changed;
}

NK_LIB void
nk_draw_color_picker(struct nk_command_buffer *o, const struct nk_rect *matrix, const struct nk_rect *hue_bar, const struct nk_rect *alpha_bar, struct nk_colorf col)
{
  NK_STORAGE const struct nk_color black       = {0, 0, 0, 255};
  NK_STORAGE const struct nk_color white       = {255, 255, 255, 255};
  NK_STORAGE const struct nk_color black_trans = {0, 0, 0, 0};

  const float     crosshair_size = 7.0f;
  struct nk_color temp;
  float           hsva[4];
  float           line_y;
  int             i;

  NK_ASSERT(o);
  NK_ASSERT(matrix);
  NK_ASSERT(hue_bar);

  /* draw hue bar */
  nk_colorf_hsva_fv(hsva, col);

  for (i = 0; i < 6; ++i) {
    NK_GLOBAL const struct nk_color hue_colors[] = {
      {255, 0, 0, 255}, {255, 255, 0, 255}, {0, 255, 0, 255}, {0, 255, 255, 255}, {0, 0, 255, 255}, {255, 0, 255, 255}, {255, 0, 0, 255}};

    nk_fill_rect_multi_color(o,
                             nk_rect(hue_bar->x, hue_bar->y + (float)i * (hue_bar->h / 6.0f) + 0.5f, hue_bar->w, (hue_bar->h / 6.0f) + 0.5f),
                             hue_colors[i],
                             hue_colors[i],
                             hue_colors[i + 1],
                             hue_colors[i + 1]);
  }

  line_y = (float)(int)(hue_bar->y + hsva[0] * matrix->h + 0.5f);
  nk_stroke_line(o, hue_bar->x - 1, line_y, hue_bar->x + hue_bar->w + 2, line_y, 1, nk_rgb(255, 255, 255));

  /* draw alpha bar */
  if (alpha_bar) {
    float alpha = NK_SATURATE(col.a);

    line_y = (float)(int)(alpha_bar->y + (1.0f - alpha) * matrix->h + 0.5f);

    nk_fill_rect_multi_color(o, *alpha_bar, white, white, black, black);
    nk_stroke_line(o, alpha_bar->x - 1, line_y, alpha_bar->x + alpha_bar->w + 2, line_y, 1, nk_rgb(255, 255, 255));
  }

  /* draw color matrix */
  temp = nk_hsv_f(hsva[0], 1.0f, 1.0f);
  nk_fill_rect_multi_color(o, *matrix, white, temp, temp, white);
  nk_fill_rect_multi_color(o, *matrix, black_trans, black_trans, black, black);

  /* draw cross-hair */
  {
    struct nk_vec2 p;
    float          S = hsva[1];
    float          V = hsva[2];

    p.x = (float)(int)(matrix->x + S * matrix->w);
    p.y = (float)(int)(matrix->y + (1.0f - V) * matrix->h);
    nk_stroke_line(o, p.x - crosshair_size, p.y, p.x - 2, p.y, 1.0f, white);
    nk_stroke_line(o, p.x + crosshair_size + 1, p.y, p.x + 3, p.y, 1.0f, white);
    nk_stroke_line(o, p.x, p.y + crosshair_size + 1, p.x, p.y + 3, 1.0f, white);
    nk_stroke_line(o, p.x, p.y - crosshair_size, p.x, p.y - 2, 1.0f, white);
  }
}

NK_LIB nk_bool
nk_do_color_picker(nk_flags                  *state,
                   struct nk_command_buffer  *out,
                   struct nk_colorf          *col,
                   enum nk_color_format       fmt,
                   struct nk_rect             bounds,
                   struct nk_vec2             padding,
                   const struct nk_input     *in,
                   const struct nk_user_font *font)
{
  int            ret = 0;
  struct nk_rect matrix;
  struct nk_rect hue_bar;
  struct nk_rect alpha_bar;
  float          bar_w;

  NK_ASSERT(out);
  NK_ASSERT(col);
  NK_ASSERT(state);
  NK_ASSERT(font);

  if (!out || !col || !state || !font) {
    return ret;
  }

  bar_w     = out->context->style.font_size;
  bounds.x += padding.x;
  bounds.y += padding.x;
  bounds.w -= 2 * padding.x;
  bounds.h -= 2 * padding.y;

  matrix.x = bounds.x;
  matrix.y = bounds.y;
  matrix.h = bounds.h;
  matrix.w = bounds.w - (3 * padding.x + 2 * bar_w);

  hue_bar.w = bar_w;
  hue_bar.y = bounds.y;
  hue_bar.h = matrix.h;
  hue_bar.x = matrix.x + matrix.w + padding.x;

  alpha_bar.x = hue_bar.x + hue_bar.w + padding.x;
  alpha_bar.y = bounds.y;
  alpha_bar.w = bar_w;
  alpha_bar.h = matrix.h;

  {
    struct nk_context *ctx = out->context;
    float              hsva[4];
    nk_bool            changed = nk_false;

    nk_colorf_hsva_fv(hsva, *col);

    if (nk_nav_stop(ctx, matrix, NK_NAV_STOP_CAPTURE) && (ctx->nav.adjust || ctx->nav.adjust_y)) {
      hsva[1] = NK_SATURATE(hsva[1] + 0.02f * (float)ctx->nav.adjust);
      hsva[2] = NK_SATURATE(hsva[2] + 0.02f * (float)ctx->nav.adjust_y);
      changed = nk_true;
    }

    if (nk_nav_stop(ctx, hue_bar, NK_NAV_STOP_ADJUST | NK_NAV_STOP_NO_CLICK) && ctx->nav.adjust) {
      hsva[0] = NK_SATURATE(hsva[0] + 0.01f * (float)ctx->nav.adjust);
      changed = nk_true;
    }

    if (fmt == NK_RGBA && nk_nav_stop(ctx, alpha_bar, NK_NAV_STOP_ADJUST | NK_NAV_STOP_NO_CLICK) && ctx->nav.adjust) {
      hsva[3] = NK_SATURATE(hsva[3] + 0.02f * (float)ctx->nav.adjust);
      changed = nk_true;
    }

    if (changed) {
      *col = nk_hsva_colorfv(hsva);
      ret  = 1;
    }
  }

  ret |= nk_color_picker_behavior(state, &bounds, &matrix, &hue_bar, (fmt == NK_RGBA) ? &alpha_bar : 0, col, in);
  nk_draw_color_picker(out, &matrix, &hue_bar, (fmt == NK_RGBA) ? &alpha_bar : 0, *col);
  return ret;
}

NK_API nk_bool
nk_color_pick(struct nk_context *ctx, struct nk_colorf *color, enum nk_color_format fmt)
{
  struct nk_window      *win;
  struct nk_panel       *layout;
  const struct nk_style *config;
  const struct nk_input *in;

  enum nk_widget_layout_states state;
  struct nk_rect               bounds;

  NK_ASSERT(ctx);
  NK_ASSERT(color);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !color) {
    return 0;
  }

  win    = ctx->current;
  config = &ctx->style;
  layout = win->layout;
  state  = nk_widget_nav(&bounds, ctx);

  if (!state) {
    return 0;
  }

  in = (state == NK_WIDGET_ROM || state == NK_WIDGET_DISABLED || layout->flags & NK_WINDOW_ROM) ? 0 : &ctx->input;
  return nk_do_color_picker(&ctx->last_widget_state, &win->buffer, color, fmt, bounds, nk_vec2(0, 0), in, config->font);
}

NK_API struct nk_colorf
nk_color_picker(struct nk_context *ctx, struct nk_colorf color, enum nk_color_format fmt)
{
  nk_color_pick(ctx, &color, fmt);
  return color;
}

/* ==============================================================
 *
 *                          COMBO
 *
 * ===============================================================*/
NK_INTERN nk_bool
nk_combo_begin(struct nk_context *ctx, struct nk_window *win, struct nk_vec2 size, nk_bool is_clicked, struct nk_rect header)
{
  struct nk_window *popup;
  int               is_open   = 0;
  int               is_active = 0;
  struct nk_rect    body;
  struct nk_rect    limits;
  nk_hash           hash;
  float             border;
  float             down_y;
  float             limits_right;
  float             limits_bottom;
  float             room_below;
  float             room_above;
  nk_bool           open_up;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  popup  = win->popup.win;
  body.x = header.x;
  body.w = size.x;
  body.h = size.y;

  border = ctx->style.window.combo_border;
  down_y = header.y + header.h - border;
  body.y = down_y;

  limits = ctx->window_maximize_bounds;

  if (limits.w > 0.0f && limits.h > 0.0f) {
    limits_right  = limits.x + limits.w;
    limits_bottom = limits.y + limits.h;

    body.w = NK_MIN(body.w, limits.w);
    body.x = NK_CLAMP(limits.x, body.x, limits_right - body.w);

    room_below = limits_bottom - down_y;
    room_above = header.y + border - limits.y;

    room_below = NK_MAX(room_below, 0.0f);
    room_above = NK_MAX(room_above, 0.0f);

    open_up = body.h > room_below && room_above > room_below;

    if (open_up) {
      body.h = NK_MIN(body.h, room_above);
      body.y = header.y - body.h + border;
    } else {
      body.h = NK_MIN(body.h, room_below);
      body.y = down_y;
    }

    body.h = NK_MAX(body.h, 1.0f);
  }

  hash      = win->popup.combo_count++;
  is_open   = (popup) ? nk_true : nk_false;
  is_active = (popup && (win->popup.name == hash) && win->popup.type == NK_PANEL_COMBO);

  if ((is_clicked && is_open && !is_active) || (is_open && !is_active) || (!is_open && !is_active && !is_clicked)) {
    return 0;
  }

  if (!nk_nonblock_begin(ctx, 0, body, (is_clicked && is_open) ? nk_rect(0, 0, 0, 0) : header, NK_PANEL_COMBO)) {
    return 0;
  }

  win->popup.type = NK_PANEL_COMBO;
  win->popup.name = hash;
  return 1;
}

NK_API nk_bool
nk_combo_begin_text(struct nk_context *ctx, const char *selected, int len, struct nk_vec2 size)
{
  const struct nk_input *in;
  struct nk_window      *win;
  struct nk_style       *style;

  enum nk_widget_layout_states s;
  int                          is_clicked = nk_false;
  struct nk_rect               header;
  const struct nk_style_item  *background;
  struct nk_text               text;

  NK_ASSERT(ctx);
  NK_ASSERT(selected);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout || !selected) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (s == NK_WIDGET_INVALID) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->combo.active;
    text.text  = style->combo.label_active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background = &style->combo.hover;
    text.text  = style->combo.label_hover;
  } else {
    background = &style->combo.normal;
    text.text  = style->combo.label_normal;
  }

  text.text = nk_rgb_factor(text.text, style->combo.color_factor);

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      text.background = background->data.color;
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    /* print currently selected text item */
    struct nk_rect label;
    struct nk_rect button;
    struct nk_rect content;
    int            draw_button_symbol;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* represents whether or not the combo's button symbol should be drawn */
    draw_button_symbol = sym != NK_SYMBOL_NONE;

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.x;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;

    /* draw selected label */
    text.padding = nk_vec2(0, 0);
    label.x      = header.x + style->combo.content_padding.x;
    label.y      = header.y + style->combo.content_padding.y;
    label.h      = header.h - 2 * style->combo.content_padding.y;

    if (draw_button_symbol) {
      label.w = button.x - (style->combo.content_padding.x + style->combo.spacing.x) - label.x;
    } else {
      label.w = header.w - 2 * style->combo.content_padding.x;
    }

    nk_widget_text(&win->buffer, label, selected, len, &text, NK_TEXT_LEFT, ctx->style.font);

    /* draw open/close button */
    if (draw_button_symbol) {
      nk_draw_button_symbol(&win->buffer, &button, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);
    }
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_label(struct nk_context *ctx, const char *selected, struct nk_vec2 size)
{
  return nk_combo_begin_text(ctx, selected, nk_strlen(selected), size);
}

NK_API nk_bool
nk_combo_begin_color(struct nk_context *ctx, struct nk_color color, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_style       *style;
  const struct nk_input *in;

  struct nk_rect               header;
  int                          is_clicked = nk_false;
  enum nk_widget_layout_states s;
  const struct nk_style_item  *background;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (s == NK_WIDGET_INVALID) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->combo.active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background = &style->combo.hover;
  } else {
    background = &style->combo.normal;
  }

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    struct nk_rect content;
    struct nk_rect button;
    struct nk_rect bounds;
    int            draw_button_symbol;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* represents whether or not the combo's button symbol should be drawn */
    draw_button_symbol = sym != NK_SYMBOL_NONE;

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.x;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;

    /* draw color */
    bounds.h = header.h - 4 * style->combo.content_padding.y;
    bounds.y = header.y + 2 * style->combo.content_padding.y;
    bounds.x = header.x + 2 * style->combo.content_padding.x;

    if (draw_button_symbol) {
      bounds.w = (button.x - (style->combo.content_padding.x + style->combo.spacing.x)) - bounds.x;
    } else {
      bounds.w = header.w - 4 * style->combo.content_padding.x;
    }

    nk_fill_rect(&win->buffer, bounds, 0, nk_rgb_factor(color, style->combo.color_factor));

    /* draw open/close button */
    if (draw_button_symbol) {
      nk_draw_button_symbol(&win->buffer, &button, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);
    }
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_symbol(struct nk_context *ctx, enum nk_symbol_type symbol, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_style       *style;
  const struct nk_input *in;

  struct nk_rect               header;
  int                          is_clicked = nk_false;
  enum nk_widget_layout_states s;
  const struct nk_style_item  *background;
  struct nk_color              sym_background;
  struct nk_color              symbol_color;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (s == NK_WIDGET_INVALID) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background   = &style->combo.active;
    symbol_color = style->combo.symbol_active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background   = &style->combo.hover;
    symbol_color = style->combo.symbol_hover;
  } else {
    background   = &style->combo.normal;
    symbol_color = style->combo.symbol_hover;
  }

  symbol_color = nk_rgb_factor(symbol_color, style->combo.color_factor);

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      sym_background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      sym_background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      sym_background = background->data.color;
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    struct nk_rect bounds = {0, 0, 0, 0};
    struct nk_rect content;
    struct nk_rect button;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.y;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;

    /* draw symbol */
    bounds.h = header.h - 2 * style->combo.content_padding.y;
    bounds.y = header.y + style->combo.content_padding.y;
    bounds.x = header.x + style->combo.content_padding.x;
    bounds.w = (button.x - style->combo.content_padding.y) - bounds.x;
    nk_draw_symbol(&win->buffer, symbol, bounds, sym_background, symbol_color, 1.0f, style->font);

    /* draw open/close button */
    nk_draw_button_symbol(&win->buffer, &bounds, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_symbol_text(struct nk_context *ctx, const char *selected, int len, enum nk_symbol_type symbol, struct nk_vec2 size)
{
  struct nk_window *win;
  struct nk_style  *style;
  struct nk_input  *in;

  struct nk_rect               header;
  int                          is_clicked = nk_false;
  enum nk_widget_layout_states s;
  const struct nk_style_item  *background;
  struct nk_color              symbol_color;
  struct nk_text               text;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (!s) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background   = &style->combo.active;
    symbol_color = style->combo.symbol_active;
    text.text    = style->combo.label_active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background   = &style->combo.hover;
    symbol_color = style->combo.symbol_hover;
    text.text    = style->combo.label_hover;
  } else {
    background   = &style->combo.normal;
    symbol_color = style->combo.symbol_normal;
    text.text    = style->combo.label_normal;
  }

  text.text    = nk_rgb_factor(text.text, style->combo.color_factor);
  symbol_color = nk_rgb_factor(symbol_color, style->combo.color_factor);

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      text.background = background->data.color;
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    struct nk_rect content;
    struct nk_rect button;
    struct nk_rect label;
    struct nk_rect image;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.x;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;
    nk_draw_button_symbol(&win->buffer, &button, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);

    /* draw symbol */
    image.x = header.x + style->combo.content_padding.x;
    image.y = header.y + style->combo.content_padding.y;
    image.h = header.h - 2 * style->combo.content_padding.y;
    image.w = image.h;
    nk_draw_symbol(&win->buffer, symbol, image, text.background, symbol_color, 1.0f, style->font);

    /* draw label */
    text.padding = nk_vec2(0, 0);
    label.x      = image.x + image.w + style->combo.spacing.x + style->combo.content_padding.x;
    label.y      = header.y + style->combo.content_padding.y;
    label.w      = (button.x - style->combo.content_padding.x) - label.x;
    label.h      = header.h - 2 * style->combo.content_padding.y;
    nk_widget_text(&win->buffer, label, selected, len, &text, NK_TEXT_LEFT, style->font);
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_image(struct nk_context *ctx, struct nk_image img, struct nk_vec2 size)
{
  struct nk_window      *win;
  struct nk_style       *style;
  const struct nk_input *in;

  struct nk_rect               header;
  int                          is_clicked = nk_false;
  enum nk_widget_layout_states s;
  const struct nk_style_item  *background;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (s == NK_WIDGET_INVALID) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->combo.active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background = &style->combo.hover;
  } else {
    background = &style->combo.normal;
  }

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    struct nk_rect bounds = {0, 0, 0, 0};
    struct nk_rect content;
    struct nk_rect button;
    int            draw_button_symbol;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* represents whether or not the combo's button symbol should be drawn */
    draw_button_symbol = sym != NK_SYMBOL_NONE;

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.y;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;

    /* draw image */
    bounds.h = header.h - 2 * style->combo.content_padding.y;
    bounds.y = header.y + style->combo.content_padding.y;
    bounds.x = header.x + style->combo.content_padding.x;

    if (draw_button_symbol) {
      bounds.w = (button.x - style->combo.content_padding.y) - bounds.x;
    } else {
      bounds.w = header.w - 2 * style->combo.content_padding.x;
    }

    nk_draw_image(&win->buffer, bounds, &img, nk_rgb_factor(nk_white, style->combo.color_factor));

    /* draw open/close button */
    if (draw_button_symbol) {
      nk_draw_button_symbol(&win->buffer, &bounds, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);
    }
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_image_text(struct nk_context *ctx, const char *selected, int len, struct nk_image img, struct nk_vec2 size)
{
  struct nk_window *win;
  struct nk_style  *style;
  struct nk_input  *in;

  struct nk_rect               header;
  int                          is_clicked = nk_false;
  enum nk_widget_layout_states s;
  const struct nk_style_item  *background;
  struct nk_text               text;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  win   = ctx->current;
  style = &ctx->style;
  s     = nk_widget_nav(&header, ctx);

  if (!s) {
    return 0;
  }

  in = (win->layout->flags & NK_WINDOW_ROM || s == NK_WIDGET_DISABLED || s == NK_WIDGET_ROM) ? 0 : &ctx->input;
  nk_nav_stop(ctx, header, 0);

  if (nk_button_behavior(&ctx->last_widget_state, header, in, NK_BUTTON_DEFAULT)) {
    is_clicked = nk_true;
  }

  /* draw combo box header background and border */
  if (ctx->last_widget_state & NK_WIDGET_STATE_ACTIVED) {
    background = &style->combo.active;
    text.text  = style->combo.label_active;
  } else if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
    background = &style->combo.hover;
    text.text  = style->combo.label_hover;
  } else {
    background = &style->combo.normal;
    text.text  = style->combo.label_normal;
  }

  text.text = nk_rgb_factor(text.text, style->combo.color_factor);

  switch (background->type) {
    case NK_STYLE_ITEM_IMAGE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_image(&win->buffer, header, &background->data.image, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_NINE_SLICE:
      text.background = nk_rgba(0, 0, 0, 0);
      nk_draw_nine_slice(&win->buffer, header, &background->data.slice, nk_rgb_factor(nk_white, style->combo.color_factor));
      break;

    case NK_STYLE_ITEM_COLOR:
      text.background = background->data.color;
      nk_fill_rect(&win->buffer, header, style->combo.rounding, nk_rgb_factor(background->data.color, style->combo.color_factor));
      nk_stroke_rect(&win->buffer, header, style->combo.rounding, style->combo.border, nk_rgb_factor(style->combo.border_color, style->combo.color_factor));
      break;
  }

  {
    struct nk_rect content;
    struct nk_rect button;
    struct nk_rect label;
    struct nk_rect image;
    int            draw_button_symbol;

    enum nk_symbol_type sym;

    if (ctx->last_widget_state & NK_WIDGET_STATE_HOVER) {
      sym = style->combo.sym_hover;
    } else if (is_clicked) {
      sym = style->combo.sym_active;
    } else {
      sym = style->combo.sym_normal;
    }

    /* represents whether or not the combo's button symbol should be drawn */
    draw_button_symbol = sym != NK_SYMBOL_NONE;

    /* calculate button */
    button.w = header.h - 2 * style->combo.button_padding.y;
    button.x = (header.x + header.w - header.h) - style->combo.button_padding.x;
    button.y = header.y + style->combo.button_padding.y;
    button.h = button.w;

    content.x = button.x + style->combo.button.padding.x;
    content.y = button.y + style->combo.button.padding.y;
    content.w = button.w - 2 * style->combo.button.padding.x;
    content.h = button.h - 2 * style->combo.button.padding.y;

    if (draw_button_symbol) {
      nk_draw_button_symbol(&win->buffer, &button, &content, ctx->last_widget_state, &ctx->style.combo.button, sym, style->font);
    }

    /* draw image */
    image.x = header.x + style->combo.content_padding.x;
    image.y = header.y + style->combo.content_padding.y;
    image.h = header.h - 2 * style->combo.content_padding.y;
    image.w = image.h;
    nk_draw_image(&win->buffer, image, &img, nk_rgb_factor(nk_white, style->combo.color_factor));

    /* draw label */
    text.padding = nk_vec2(0, 0);
    label.x      = image.x + image.w + style->combo.spacing.x + style->combo.content_padding.x;
    label.y      = header.y + style->combo.content_padding.y;
    label.h      = header.h - 2 * style->combo.content_padding.y;

    if (draw_button_symbol) {
      label.w = (button.x - style->combo.content_padding.x) - label.x;
    } else {
      label.w = (header.x + header.w - style->combo.content_padding.x) - label.x;
    }

    nk_widget_text(&win->buffer, label, selected, len, &text, NK_TEXT_LEFT, style->font);
  }
  return nk_combo_begin(ctx, win, size, is_clicked, header);
}

NK_API nk_bool
nk_combo_begin_symbol_label(struct nk_context *ctx, const char *selected, enum nk_symbol_type type, struct nk_vec2 size)
{
  return nk_combo_begin_symbol_text(ctx, selected, nk_strlen(selected), type, size);
}

NK_API nk_bool
nk_combo_begin_image_label(struct nk_context *ctx, const char *selected, struct nk_image img, struct nk_vec2 size)
{
  return nk_combo_begin_image_text(ctx, selected, nk_strlen(selected), img, size);
}

NK_API nk_bool
nk_combo_item_text(struct nk_context *ctx, const char *text, int len, nk_flags align)
{
  return nk_contextual_item_text(ctx, text, len, align);
}

NK_API nk_bool
nk_combo_item_label(struct nk_context *ctx, const char *label, nk_flags align)
{
  return nk_contextual_item_label(ctx, label, align);
}

NK_API nk_bool
nk_combo_item_image_text(struct nk_context *ctx, struct nk_image img, const char *text, int len, nk_flags alignment)
{
  return nk_contextual_item_image_text(ctx, img, text, len, alignment);
}

NK_API nk_bool
nk_combo_item_image_label(struct nk_context *ctx, struct nk_image img, const char *text, nk_flags alignment)
{
  return nk_contextual_item_image_label(ctx, img, text, alignment);
}

NK_API nk_bool
nk_combo_item_symbol_text(struct nk_context *ctx, enum nk_symbol_type sym, const char *text, int len, nk_flags alignment)
{
  return nk_contextual_item_symbol_text(ctx, sym, text, len, alignment);
}

NK_API nk_bool
nk_combo_item_symbol_label(struct nk_context *ctx, enum nk_symbol_type sym, const char *label, nk_flags alignment)
{
  return nk_contextual_item_symbol_label(ctx, sym, label, alignment);
}

NK_API void
nk_combo_end(struct nk_context *ctx)
{
  nk_contextual_end(ctx);
}

NK_API void
nk_combo_close(struct nk_context *ctx)
{
  nk_contextual_close(ctx);
}

NK_API int
nk_combo(struct nk_context *ctx, const char *const *items, int count, int selected, int item_height, struct nk_vec2 size)
{
  int            i = 0;
  int            max_height;
  struct nk_vec2 item_spacing;
  struct nk_vec2 window_padding;

  NK_ASSERT(ctx);
  NK_ASSERT(items);
  NK_ASSERT(ctx->current);

  if (!ctx || !items || !count) {
    return selected;
  }

  item_spacing   = ctx->style.window.spacing;
  window_padding = nk_panel_get_padding(&ctx->style, ctx->current->layout->type);
  max_height     = count * item_height + count * (int)item_spacing.y;
  max_height    += (int)item_spacing.y * 2 + (int)window_padding.y * 2;
  size.y         = NK_MIN(size.y, (float)max_height);

  if (nk_combo_begin_label(ctx, items[selected], size)) {
    nk_layout_row_dynamic(ctx, (float)item_height, 1);

    for (i = 0; i < count; ++i) {
      if (nk_combo_item_label(ctx, items[i], NK_TEXT_LEFT)) {
        selected = i;
      }
    }

    nk_combo_end(ctx);
  }
  return selected;
}

NK_API int
nk_combo_separator(struct nk_context *ctx, const char *items_separated_by_separator, int separator, int selected, int count, int item_height, struct nk_vec2 size)
{
  int            i;
  int            max_height;
  struct nk_vec2 item_spacing;
  struct nk_vec2 window_padding;
  const char    *current_item;
  const char    *iter;
  int            length = 0;

  NK_ASSERT(ctx);
  NK_ASSERT(items_separated_by_separator);

  if (!ctx || !items_separated_by_separator) {
    return selected;
  }

  /* calculate popup window */
  item_spacing   = ctx->style.window.spacing;
  window_padding = nk_panel_get_padding(&ctx->style, ctx->current->layout->type);
  max_height     = count * item_height + count * (int)item_spacing.y;
  max_height    += (int)item_spacing.y * 2 + (int)window_padding.y * 2;
  size.y         = NK_MIN(size.y, (float)max_height);

  /* find selected item */
  current_item = items_separated_by_separator;

  for (i = 0; i < count; ++i) {
    iter = current_item;

    while (*iter && *iter != separator) {
      iter++;
    }

    length = (int)(iter - current_item);

    if (i == selected) {
      break;
    }

    current_item = iter + 1;
  }

  if (nk_combo_begin_text(ctx, current_item, length, size)) {
    current_item = items_separated_by_separator;
    nk_layout_row_dynamic(ctx, (float)item_height, 1);

    for (i = 0; i < count; ++i) {
      iter = current_item;

      while (*iter && *iter != separator) {
        iter++;
      }

      length = (int)(iter - current_item);

      if (nk_combo_item_text(ctx, current_item, length, NK_TEXT_LEFT)) {
        selected = i;
      }

      current_item = current_item + length + 1;
    }

    nk_combo_end(ctx);
  }
  return selected;
}

NK_API int
nk_combo_string(struct nk_context *ctx, const char *items_separated_by_zeros, int selected, int count, int item_height, struct nk_vec2 size)
{
  return nk_combo_separator(ctx, items_separated_by_zeros, '\0', selected, count, item_height, size);
}

NK_API int
nk_combo_callback(struct nk_context *ctx, void (*item_getter)(void *, int, const char **), void *userdata, int selected, int count, int item_height, struct nk_vec2 size)
{
  int            i;
  int            max_height;
  struct nk_vec2 item_spacing;
  struct nk_vec2 window_padding;
  const char    *item;

  NK_ASSERT(ctx);
  NK_ASSERT(item_getter);

  if (!ctx || !item_getter) {
    return selected;
  }

  /* calculate popup window */
  item_spacing   = ctx->style.window.spacing;
  window_padding = nk_panel_get_padding(&ctx->style, ctx->current->layout->type);
  max_height     = count * item_height + count * (int)item_spacing.y;
  max_height    += (int)item_spacing.y * 2 + (int)window_padding.y * 2;
  size.y         = NK_MIN(size.y, (float)max_height);

  item_getter(userdata, selected, &item);

  if (nk_combo_begin_label(ctx, item, size)) {
    nk_layout_row_dynamic(ctx, (float)item_height, 1);

    for (i = 0; i < count; ++i) {
      item_getter(userdata, i, &item);

      if (nk_combo_item_label(ctx, item, NK_TEXT_LEFT)) {
        selected = i;
      }
    }

    nk_combo_end(ctx);
  }
  return selected;
}

NK_API nk_bool
nk_combobox(struct nk_context *ctx, const char *const *items, int count, int *selected, int item_height, struct nk_vec2 size)
{
  int tmp = *selected;

  *selected = nk_combo(ctx, items, count, *selected, item_height, size);
  return tmp != *selected;
}

NK_API nk_bool
nk_combobox_string(struct nk_context *ctx, const char *items_separated_by_zeros, int *selected, int count, int item_height, struct nk_vec2 size)
{
  int tmp = *selected;

  *selected = nk_combo_string(ctx, items_separated_by_zeros, *selected, count, item_height, size);
  return tmp != *selected;
}

NK_API nk_bool
nk_combobox_separator(struct nk_context *ctx, const char *items_separated_by_separator, int separator, int *selected, int count, int item_height, struct nk_vec2 size)
{
  int tmp = *selected;

  *selected = nk_combo_separator(ctx, items_separated_by_separator, separator, *selected, count, item_height, size);
  return tmp != *selected;
}

NK_API nk_bool
nk_combobox_callback(
  struct nk_context *ctx, void (*item_getter)(void *data, int id, const char **out_text), void *userdata, int *selected, int count, int item_height, struct nk_vec2 size)
{
  int tmp = *selected;

  *selected = nk_combo_callback(ctx, item_getter, userdata, *selected, count, item_height, size);
  return tmp != *selected;
}

/* ===============================================================
 *
 *                              TOOLTIP
 *
 * ===============================================================*/
NK_API nk_bool
nk_tooltip_begin(struct nk_context *ctx, float width)
{
  NK_ASSERT(ctx);
  return nk_tooltip_begin_offset(ctx, width, ctx->style.window.tooltip_origin, ctx->style.window.tooltip_offset);
}

NK_API nk_bool
nk_tooltip_begin_offset(struct nk_context *ctx, float width, enum nk_tooltip_pos position, struct nk_vec2 offset)
{
  int                    x, y, w, h;
  struct nk_window      *win;
  const struct nk_input *in;
  struct nk_rect         bounds;
  int                    ret;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);

  if (!ctx || !ctx->current || !ctx->current->layout) {
    return 0;
  }

  /* make sure that no nonblocking popup is currently active */
  win = ctx->current;
  in  = &ctx->input;

  if (win->popup.win && ((int)win->popup.type & (int)NK_PANEL_SET_NONBLOCK)) {
    return 0;
  }

  w = nk_iceilf(width);
  h = (int)NK_MAX(win->layout->row.min_height, ctx->style.font_size + 2 * ctx->style.window.padding.y);

  /* Default origin is top left, plus user offset */
  x = nk_ifloorf(in->mouse.pos.x + 1) - (int)win->layout->clip.x + (int)offset.x;
  y = nk_ifloorf(in->mouse.pos.y + 1) - (int)win->layout->clip.y + (int)offset.y;

  /* Adjust origin based on enum */
  switch (position) {
    case NK_TOP_LEFT:
      /* no change */
      break;

    case NK_TOP_CENTER:
      x -= w / 2;
      break;

    case NK_TOP_RIGHT:
      x -= w;
      break;

    case NK_MIDDLE_LEFT:
      y -= h / 2;
      break;

    case NK_MIDDLE_CENTER:
      x -= w / 2;
      y -= h / 2;
      break;

    case NK_MIDDLE_RIGHT:
      x -= w;
      y -= h / 2;
      break;

    case NK_BOTTOM_LEFT:
      y -= h;
      break;

    case NK_BOTTOM_CENTER:
      x -= w / 2;
      y -= h;
      break;

    case NK_BOTTOM_RIGHT:
      x -= w;
      y -= h;
      break;

    default:
      NK_ASSERT(0 && "Invalid tooltip position");
  }

  bounds.x = (float)x;
  bounds.y = (float)y;
  bounds.w = (float)w;
  bounds.h = (float)nk_iceilf(nk_null_rect.h);

  ret = nk_popup_begin(ctx, NK_POPUP_DYNAMIC, "__##Tooltip##__", NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER, bounds);

  if (ret) {
    win->layout->flags &= ~(nk_flags)NK_WINDOW_ROM;
  }

  win->popup.type            = NK_PANEL_TOOLTIP;
  ctx->current->layout->type = NK_PANEL_TOOLTIP;
  return ret;
}

NK_API void
nk_tooltip_end(struct nk_context *ctx)
{
  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);

  if (!ctx || !ctx->current) {
    return;
  }

  ctx->current->seq--;
  nk_popup_close(ctx);
  nk_popup_end(ctx);
}

NK_API void
nk_tooltip_text_offset(struct nk_context *ctx, const char *text, int len, enum nk_tooltip_pos position, struct nk_vec2 offset)
{
  const struct nk_style *style;
  struct nk_vec2         padding;

  float text_width;
  float text_height;

  NK_ASSERT(ctx);
  NK_ASSERT(ctx->current);
  NK_ASSERT(ctx->current->layout);
  NK_ASSERT(text);
  NK_ASSERT(len >= 0);

  if (!ctx || !ctx->current || !ctx->current->layout || !text || len < 0) {
    return;
  }

  /* fetch configuration data */
  style   = &ctx->style;
  padding = style->window.padding;

  /* calculate size of the text and tooltip */
  text_width  = nk_text_width(ctx, style->font, style->font_size, text, len);
  text_width += (4 * padding.x);
  text_height = (style->font_size + 2 * padding.y);

  /* execute tooltip and fill with text */
  if (nk_tooltip_begin_offset(ctx, (float)text_width, position, offset)) {
    nk_layout_row_dynamic(ctx, (float)text_height, 1);
    nk_text(ctx, text, len, NK_TEXT_LEFT);
    nk_tooltip_end(ctx);
  }
}

NK_API void
nk_tooltip_offset(struct nk_context *ctx, const char *text, enum nk_tooltip_pos position, struct nk_vec2 offset)
{
  NK_ASSERT(ctx);
  nk_tooltip_text_offset(ctx, text, nk_strlen(text), position, offset);
}

NK_API void
nk_tooltip(struct nk_context *ctx, const char *text)
{
  NK_ASSERT(ctx);
  nk_tooltip_offset(ctx, text, ctx->style.window.tooltip_origin, ctx->style.window.tooltip_offset);
}

NK_API void
nk_tooltip_text(struct nk_context *ctx, const char *text, int len)
{
  NK_ASSERT(ctx);
  nk_tooltip_text_offset(ctx, text, len, ctx->style.window.tooltip_origin, ctx->style.window.tooltip_offset);
}

#ifdef NK_INCLUDE_STANDARD_VARARGS
NK_API void
nk_tooltipf_offset(struct nk_context *ctx, enum nk_tooltip_pos position, struct nk_vec2 offset, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_tooltipfv_offset(ctx, position, offset, fmt, args);
  va_end(args);
}

NK_API void
nk_tooltipf(struct nk_context *ctx, const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  nk_tooltipfv(ctx, fmt, args);
  va_end(args);
}

NK_API void
nk_tooltipfv_offset(struct nk_context *ctx, enum nk_tooltip_pos position, struct nk_vec2 offset, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_tooltip_text_offset(ctx, buf, nk_strlen(buf), position, offset);
}

NK_API void
nk_tooltipfv(struct nk_context *ctx, const char *fmt, va_list args)
{
  char buf[256];

  nk_strfmt(buf, NK_LEN(buf), fmt, args);
  nk_tooltip(ctx, buf);
}
#endif
