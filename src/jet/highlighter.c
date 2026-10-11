#include <wctype.h>

#include "highlighter.h"
#include "editor.h"
#include "config.h"

extern HighlighterInfo c_highlighter_info;

static HighlighterInfo *infos[] = { &c_highlighter_info };

void highlighter_do_main_buffer(MainBuffer *buffer) {
  buffer->highlighter.info = NULL;
  for (u32 i = 0; i < ARRAY_LEN(infos); ++i) {
    HighlighterInfo *info = infos[i];
    for (u32 j = 0; j < info->file_extensions_len; ++j) {
      if (str_ends_with(buffer->abs_file_path, info->file_extensions[j])) {
        buffer->highlighter.info = info;
        break;
      }
    }
    if (buffer->highlighter.info)
      break;
  }

  if (!buffer->highlighter.info)
    return;

  buffer->highlighter.states.len = 0;
  for (u32 i = 0; i < buffer->highlighter.rangess.len; ++i)
    if (buffer->highlighter.rangess.items[i].items)
      free(buffer->highlighter.rangess.items[i].items);
  buffer->highlighter.rangess.len = 0;

  highlighter_redo_line(buffer, 0);
}

static bool highlighter_state_eq(HighlighterState *a, HighlighterState *b) {
  return
    a->kind == b->kind &&
    (a->kind != HighlighterStateKindString ||
     a->str_end_index == b->str_end_index) &&
    (a->kind != HighlighterStateKindMLComment ||
     a->ml_comment_level == b->ml_comment_level);
}

void highlighter_redo_line(MainBuffer *buffer, u32 row) {
  if (!buffer->highlighter.info)
    return;

  u32 rows = buffer_get_rows(&buffer->buffer);
  bool iterate = true;

  while (iterate) {
    while (row >= buffer->highlighter.states.len)
      DA_APPEND(buffer->highlighter.states,
                ((HighlighterState) { HighlighterStateKindBasic, {} }));

    while (row >= buffer->highlighter.rangess.len)
      DA_APPEND(buffer->highlighter.rangess, (ColorRanges) {0});

    HighlighterState state = buffer->highlighter.states.items[row];

    ColorRanges *ranges = buffer->highlighter.rangess.items + row;
    ranges->len = 0;
    switch (state.kind) {
    case HighlighterStateKindBasic: break;

    case HighlighterStateKindString: {
      ColorRange range = { 0, 0, STRING_COLOR };
      DA_APPEND(*ranges, range);
    } break;

    case HighlighterStateKindSLComment: break;

    case HighlighterStateKindMLComment: {
      ColorRange range = { 0, 0, COMMENT_COLOR };
      DA_APPEND(*ranges, range);
    } break;
    }

    WideStr line = buffer_get_line(&buffer->buffer, row);
    for (u32 i = 0; i < line.len; ++i) {
      if (iswspace(line.ptr[i])) {
        while (iswspace(line.ptr[i]))
          ++i;
        --i;
      }

      WideStr sub_line = { line.ptr + i, line.len - i };

      switch (state.kind) {
      case HighlighterStateKindBasic: {
        bool found = false;

        for (u32 j = 0; j < buffer->highlighter.info->keywords_len; ++j) {
          Keyword *keyword = buffer->highlighter.info->keywords + j;
          if ((i > 0 && is_part_of_word(line.ptr[i - 1])) ||
              (i + keyword->str.len < line.len &&
               is_part_of_word(line.ptr[i + keyword->str.len])))
            continue;

          if (wide_str_begins_with(sub_line, keyword->str)) {
            ColorRange range = {
              i,
              i + keyword->str.len,
              keyword->r,
              keyword->g,
              keyword->b,
            };
            DA_APPEND(*ranges, range);

            i += keyword->str.len - 1;

            found = true;

            break;
          }
        }

        if (found)
          break;

        for (u32 j = 0; j < buffer->highlighter.info->str_kinds_len; ++j) {
          WideStr begin = buffer->highlighter.info->str_begins[j];

          if (wide_str_begins_with(sub_line, begin)) {
            state.kind = HighlighterStateKindString;
            state.str_end_index = j;

            ColorRange range = {
              i,
              i + begin.len,
              STRING_COLOR,
            };
            DA_APPEND(*ranges, range);

            i += begin.len - 1;

            found = true;

            break;
          }
        }

        if (found)
          break;

        WideStr begin = buffer->highlighter.info->sl_comment_begin;
        if (wide_str_begins_with(sub_line, begin)) {
          state.kind = HighlighterStateKindSLComment;

          ColorRange range = {
            i,
            i + begin.len,
            COMMENT_COLOR,
          };
          DA_APPEND(*ranges, range);

          i += begin.len - 1;

          break;
        }

        begin = buffer->highlighter.info->ml_comment_begin;
        if (wide_str_begins_with(sub_line, begin)) {
          state.kind = HighlighterStateKindMLComment;
          state.ml_comment_level = 1;

          ColorRange range = {
            i,
            i + begin.len,
            COMMENT_COLOR,
          };
          DA_APPEND(*ranges, range);

          i += begin.len - 1;

          break;
        }
      } break;

      case HighlighterStateKindString: {
        WideStr end = buffer->highlighter.info->str_ends[state.str_end_index];
        if (wide_str_begins_with(sub_line, end) &&
            (i == 0 ||
             line.ptr[i - 1] != '\\' ||
             (i >= 2 && line.ptr[i - 2] == '\\'))) {
          state.kind = HighlighterStateKindBasic;
          ranges->items[ranges->len - 1].end += end.len;
          i += end.len - 1;
        } else {
          ++ranges->items[ranges->len - 1].end;
        }
      } break;

      case HighlighterStateKindSLComment: {
        ++ranges->items[ranges->len - 1].end;
        if (i + 1 == line.len)
          state.kind = HighlighterStateKindBasic;
      } break;

      case HighlighterStateKindMLComment: {
        WideStr end = buffer->highlighter.info->ml_comment_end;
        if (wide_str_begins_with(sub_line, end)) {
          if (--state.ml_comment_level == 0) {
            state.kind = HighlighterStateKindBasic;
            ranges->items[ranges->len - 1].end += end.len;
            i += end.len - 1;
          }

          break;
        } else {
          ++ranges->items[ranges->len - 1].end;
        }

        WideStr begin = buffer->highlighter.info->ml_comment_begin;
        if (wide_str_begins_with(sub_line, begin)) {
          ++state.ml_comment_level;

          i += begin.len - 1;
        }
      } break;
      }
    }

    ++row;

    iterate =
      row < rows &&
      (row >= buffer->highlighter.states.len ||
       !highlighter_state_eq(buffer->highlighter.states.items + row, &state));

    if (iterate) {
      if (row < buffer->highlighter.states.len)
        buffer->highlighter.states.items[row] = state;
      else
        DA_APPEND(buffer->highlighter.states, state);
    }
  }
}

void highlighter_update_before_action(MainBuffer *buffer, Action action) {
  switch (action) {
  case ActionAddLine: {
    if (buffer->buffer.cursor_row < buffer->highlighter.states.len)
      DA_INSERT(buffer->highlighter.states,
                buffer->buffer.cursor_row,
                buffer->highlighter.states.items[buffer->buffer.cursor_row + 1]);
    else
      DA_INSERT(buffer->highlighter.states,
                buffer->buffer.cursor_row,
                ((HighlighterState) { HighlighterStateKindBasic, {} }));
    DA_INSERT(buffer->highlighter.rangess, buffer->buffer.cursor_row, (ColorRanges) {0});
  } break;

  case ActionRemoveLineBeforeCursor:
  case ActionRemoveLineAfterCursor: {
    DA_REMOVE_AT(buffer->highlighter.states, buffer->buffer.cursor_row);
    DA_REMOVE_AT(buffer->highlighter.rangess, buffer->buffer.cursor_row);
  } break;

  case ActionAdd:                break;
  case ActionRemoveBeforeCursor: break;
  case ActionRemoveAfterCursor:  break;
  }
}

void highlighter_delete(Highlighter *highlighter) {
  if (highlighter->states.items)
    free(highlighter->states.items);
  for (u32 i = 0; i < highlighter->rangess.len; ++i)
    if (highlighter->rangess.items[i].items)
      free(highlighter->rangess.items[i].items);
  if (highlighter->rangess.items)
    free(highlighter->rangess.items);
}
