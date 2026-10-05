// Information about utf-8 format was taken from
// https://www.cprogramming.com/tutorial/unicode.html

#include <wctype.h>

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
