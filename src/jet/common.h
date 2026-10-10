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

typedef Da(u32) WideStringBuilder;

typedef Da(u32) Indices;

bool      wide_str_begins_with(WideStr str, WideStr prefix);
WideStrs  wide_strs_select_prefixed(WideStrs strs, WideStr content);
WideStr   wide_str_dup(WideStr str);
bool      wide_str_eq(WideStr a, WideStr b);
u64       wide_str_hash(WideStr str);
void      put_wide_char(u32 ch, FILE *stream);
u32       get_wide_char(FILE *stream);
WideStr   str_to_wide_str(Str str);
void      wsb_push_str(WideStringBuilder *wsb, Str str);
void      wsb_push_u32(WideStringBuilder *wsb, u32 num);
char     *wide_str_to_cstr(WideStr str);
Str       wide_str_to_str(WideStr str);
void      destructurize_color(f32 *o_r, f32 *o_g, f32 *o_b, f32 r, f32 g, f32 b);
// Used for autocompletion
bool      is_part_of_word(u32 _char);

#endif // COMMON_H
