#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

typedef struct {
  u32 *ptr;
  u32  len;
} WideStr;

typedef Da(WideStr) WideStrs;

bool      wide_str_begins_with(WideStr str, WideStr prefix);
WideStrs  wide_strs_select_prefixed(WideStrs strs, WideStr content);
WideStr   wide_str_dup(WideStr str);
void      put_wide_char(u32 ch, FILE *stream);
u32       get_wide_char(FILE *stream);
WideStr   str_to_wide_str(Str str);
char     *wide_str_to_cstr(WideStr str);

#endif // COMMON_H
