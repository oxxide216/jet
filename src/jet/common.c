// Information about utf-8 format was taken from
// https://www.cprogramming.com/tutorial/unicode.html

#include <wctype.h>
#include <wchar.h>
#include <assert.h>

#include "common.h"

static u8 offset_table[3] = { 7, 5, 4 };

bool wide_str_begins_with(WideStr str, WideStr prefix) {
  if (str.len < prefix.len)
    return false;

  for (u32 i = 0; i < prefix.len; ++i)
    if (towlower(str.ptr[i]) != towlower(prefix.ptr[i]))
      return false;

  return true;
}

WideStrs wide_strs_select_prefixed(WideStrs strs, WideStr content) {
  WideStrs result = {0};

  for (u32 i = 0; i < strs.len; ++i)
    if (wide_str_begins_with(strs.items[i], content))
      DA_APPEND(result, strs.items[i]);

  return result;
}

WideStr wide_str_dup(WideStr str) {
  u32 *new_ptr = malloc(str.len * sizeof(*new_ptr));
  memcpy(new_ptr, str.ptr, str.len * sizeof(*new_ptr));
  str.ptr = new_ptr;
  return str;
}

void put_wide_char(u32 ch, FILE *stream) {
  u8 *ptr = (u8 *) &ch;

  for (u8 i = 0; i < 4; ++i)
    if (ptr[i])
      putc(ptr[i], stream);
}

u32 get_wide_char(FILE *stream) {
  char ptr[4] = {0};

  ptr[0] = getc(stream);
  if (ptr[0] == EOF)
    return (u32) -1;
  for (u32 i = 0; i < sizeof(offset_table); ++i) {
    if (!((ptr[0] >> offset_table[i]) & 1))
      break;
    ptr[i + 1] = getc(stream);
  if (ptr[i + 1] == EOF)
    return (u32) -1;
  }

  return *(u32 *) ptr;
}

static u32 read_wide_char(Str *str) {
  u8 ptr[4] = {0};

  ptr[0] = str->ptr++[0];
  for (u32 i = 0; i < sizeof(offset_table); ++i) {
    if (str->len <= i + 1 || !((ptr[0] >> offset_table[i]) & 1))
      break;
    ptr[i + 1] = str->ptr++[0];
  }

  return *(u32 *) ptr;
}

WideStr str_to_wide_str(Str str) {
  u32 wlen = 0;
  for (u32 i = 0; i < str.len; ++i)
    wlen += (str.ptr[i] >> 6) != 0b10;

  u32 *wstr = malloc(wlen * sizeof(*wstr));
  for (u32 i = 0; i < wlen; ++i)
    wstr[i] = read_wide_char(&str);

  return (WideStr) {
    .ptr = wstr,
    .len = wlen,
  };
}

void wsb_push_str(WideStringBuilder *wsb, Str str) {
  u32 wlen = 0;
  for (u32 i = 0; i < str.len; ++i)
    wlen += (str.ptr[i] >> 6) != 0b10;

  for (u32 i = 0; i < wlen; ++i) {
    u32 wide_char = read_wide_char(&str);
    DA_APPEND(*wsb, wide_char);
  }
}

void wsb_push_u32(WideStringBuilder *wsb, u32 num) {
  u32 _num = num;
  u32 len = 1;

  while (_num >= 10) {
    _num /= 10;
    ++len;
  }

  if (wsb->cap < wsb->len + len + 1) {
    if (wsb->cap != 0) {
      while (wsb->cap < wsb->len + len + 1)
        wsb->cap *= 2;
      wsb->items = realloc(wsb->items, wsb->cap * sizeof(*wsb->items));
    } else {
      wsb->cap = len;
      wsb->items = malloc(wsb->cap * sizeof(*wsb->items));
    }
  }

  static_assert(sizeof(u32) == sizeof(wchar_t));
  swprintf((wchar_t *) (wsb->items + wsb->len), len + 1, L"%u", num);
  wsb->len += len;
}

char *wide_str_to_cstr(WideStr str) {
  char *cstr = malloc(str.len * sizeof(*str.ptr));
  u32 len = 0;
  for (u32 i = 0; i < str.len; ++i)
    for (u32 j = 0; j < sizeof(*str.ptr); ++j)
      if (((char *) (str.ptr + i))[j])
        cstr[len++] = ((char *) (str.ptr + i))[j];
  cstr[len] = '\0';
  return cstr;
}

Str wide_str_to_str(WideStr str) {
  Str result = { malloc(str.len * sizeof(*result.ptr)), 0 };
  for (u32 i = 0; i < str.len; ++i)
    for (u32 j = 0; j < sizeof(*str.ptr); ++j)
      if (((char *) (str.ptr + i))[j])
        result.ptr[result.len++] = ((char *) (str.ptr + i))[j];
  return result;
}

void destructurize_color(f32 *o_r, f32 *o_g, f32 *o_b, f32 r, f32 g, f32 b) {
  *o_r = r;
  *o_g = g;
  *o_b = b;
}
