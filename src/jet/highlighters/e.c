#include "../highlighter.h"
#include "../config.h"

static Str file_extensions[] = { STR_LIT(".e") };

static Keyword keywords[] = {
  { WSTR_LIT(U"proc"), KW_COLOR },
  { WSTR_LIT(U"end"), KW_COLOR },
  { WSTR_LIT(U"let"), KW_COLOR },
  { WSTR_LIT(U"true"), KW_COLOR },
  { WSTR_LIT(U"false"), KW_COLOR },
  { WSTR_LIT(U"ret"), KW_COLOR },
  { WSTR_LIT(U"retval"), KW_COLOR },
  { WSTR_LIT(U"while"), KW_COLOR },
  { WSTR_LIT(U"if"), KW_COLOR },
  { WSTR_LIT(U"elif"), KW_COLOR },
  { WSTR_LIT(U"else"), KW_COLOR },
  { WSTR_LIT(U"const"), KW_COLOR },
  { WSTR_LIT(U"use"), KW_COLOR },
  { WSTR_LIT(U"super"), KW_COLOR },
  { WSTR_LIT(U"null"), KW_COLOR },
  { WSTR_LIT(U"asm"), KW_COLOR },
  { WSTR_LIT(U"as"), KW_COLOR },
  { WSTR_LIT(U"lenof"), KW_COLOR },
  { WSTR_LIT(U"struct"), KW_COLOR },
  { WSTR_LIT(U"sizeof"), KW_COLOR },
  { WSTR_LIT(U"static"), KW_COLOR },
  { WSTR_LIT(U"break"), KW_COLOR },
  { WSTR_LIT(U"continue"), KW_COLOR },
  { WSTR_LIT(U"nobreak"), KW_COLOR },
  { WSTR_LIT(U"bool"), TYPE_COLOR },
  { WSTR_LIT(U"s8"), TYPE_COLOR },
  { WSTR_LIT(U"s16"), TYPE_COLOR },
  { WSTR_LIT(U"s32"), TYPE_COLOR },
  { WSTR_LIT(U"s64"), TYPE_COLOR },
  { WSTR_LIT(U"u8"), TYPE_COLOR },
  { WSTR_LIT(U"u16"), TYPE_COLOR },
  { WSTR_LIT(U"u32"), TYPE_COLOR },
  { WSTR_LIT(U"u64"), TYPE_COLOR },
  { WSTR_LIT(U"str"), TYPE_COLOR },
};

static WideStr strs[] = { WSTR_LIT(U"\""), WSTR_LIT(U"'") };

HighlighterInfo e_highlighter_info = {
  .file_extensions = file_extensions,
  .file_extensions_len = ARRAY_LEN(file_extensions),
  .keywords = keywords,
  .keywords_len = ARRAY_LEN(keywords),
  .str_begins = strs,
  .str_ends = strs,
  .str_kinds_len = ARRAY_LEN(strs),
  .sl_comment_begin = WSTR_LIT(U"#"),
  .ml_comment_begin = WSTR_LIT(U"#*"),
  .ml_comment_end = WSTR_LIT(U"*#"),
};
