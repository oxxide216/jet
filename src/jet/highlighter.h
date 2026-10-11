#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "common.h"

typedef struct {
  WideStr str;
  f32 r, g, b;
} Keyword;

typedef struct {
  Str     *file_extensions;
  u32      file_extensions_len;
  Keyword *keywords;
  u32      keywords_len;
  WideStr *str_begins;
  WideStr *str_ends;
  u32      str_kinds_len;
  WideStr  sl_comment_begin;
  WideStr  ml_comment_begin;
  WideStr  ml_comment_end;
} HighlighterInfo;

typedef enum {
  HighlighterStateKindBasic = 0,
  HighlighterStateKindString,
  HighlighterStateKindSLComment,
  HighlighterStateKindMLComment,
} HighlighterStateKind;

typedef struct {
  HighlighterStateKind kind;
  union {
    u32 str_end_index;
    u32 ml_comment_level;
  };
} HighlighterState;

typedef Da(HighlighterState) HighlighterStates;

typedef struct {
  u32 begin, end;
  f32 r, g, b;
} ColorRange;

typedef Da(ColorRange) ColorRanges;
typedef Da(ColorRanges) ColorRangess;

typedef struct {
  HighlighterInfo   *info;
  HighlighterStates  states;
  ColorRangess       rangess;
} Highlighter;

typedef struct MainBuffer MainBuffer;

void highlighter_do_main_buffer(MainBuffer *buffer);
void highlighter_redo_line(MainBuffer *buffer, u32 row);
void highlighter_update_before_action(MainBuffer *buffer, Action action);
void highlighter_delete(Highlighter *highlighter);

#endif // HIGHLIGHTER_H
