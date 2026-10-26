#include "searcher.h"

void searcher_init(Searcher *searcher, Buffer *buffer) {
  searcher->buffer = buffer;
  searcher->history = (SearchHistory) {0};
  searcher->pattern = (WideStr) {0};
}

void searcher_update_pattern(Searcher *searcher, WideStr pattern) {
  SearchHistoryItem item = {
    SearchHistoryItemKindUpdatePattern,
    {
      .prev_pattern = searcher->pattern,
    },
  };
  DA_APPEND(searcher->history, item);

  searcher->pattern.len = pattern.len;
  searcher->pattern.ptr = malloc(searcher->pattern.len * sizeof(*searcher->pattern.ptr));
  memcpy(searcher->pattern.ptr, pattern.ptr,
         searcher->pattern.len * sizeof(*searcher->pattern.ptr));
}

bool searcher_next_match(Searcher *searcher, u32 *row, u32 *col, bool add_to_historty) {
  u32 rows = buffer_get_rows(searcher->buffer);
  u32 temp_col = *col + 1;
  for (u32 i = *row; i < rows; ++i) {
    WideStr line = buffer_get_line(searcher->buffer, i);
    for (u32 j = temp_col; j + searcher->pattern.len < line.len; ++j) {
      WideStr sub_line = { line.ptr + j, searcher->pattern.len };
      if (wide_str_eq(sub_line, searcher->pattern)) {
        if (add_to_historty) {
          SearchHistoryItem item = {
            SearchHistoryItemKindNextMatch,
            {
              .prev_match_row = *row,
              .prev_match_col = *col,
            },
          };
          DA_APPEND(searcher->history, item);
        }

        *row = i;
        *col = j;
        return true;
      }
    }
    temp_col = 0;
  }

  return false;
}

bool searcher_prev_match(Searcher *searcher, u32 *row, u32 *col, bool add_to_historty) {
  for (u32 i = *row; i > 0; --i) {
    WideStr line = buffer_get_line(searcher->buffer, i - 1);
    if (line.len < searcher->pattern.len)
      continue;
    for (u32 j = line.len - searcher->pattern.len + 1; j > 0; --j) {
      WideStr sub_line = { line.ptr + j - 1, searcher->pattern.len };
      if (wide_str_eq(sub_line, searcher->pattern)) {
        if (add_to_historty) {
          SearchHistoryItem item = {
            SearchHistoryItemKindNextMatch,
            {
              .prev_match_row = *row,
              .prev_match_col = *col,
            },
          };
          DA_APPEND(searcher->history, item);
        }

        *row = i - 1;
        *col = j - 1;
        return true;
      }
    }
  }

  return false;
}

bool searcher_undo(Searcher *searcher, u32 *row, u32 *col, Buffer *search_buffer) {
  if (searcher->history.len == 0)
    return false;

  SearchHistoryItem *item = searcher->history.items + --searcher->history.len;
  switch (item->kind) {
  case SearchHistoryItemKindNextMatch: {
    *row = item->prev_match_row;
    *col = item->prev_match_col;
  } break;

  case SearchHistoryItemKindUpdatePattern: {
    searcher->pattern = item->prev_pattern;
    buffer_reset(search_buffer);
    for (u32 i = 0; i < item->prev_pattern.len; ++i)
      buffer_insert(search_buffer, item->prev_pattern.ptr[i]);
  } break;
  }

  return true;
}

bool searcher_has_succesful_searches_in_history(Searcher *searcher) {
  for (u32 i = searcher->history.len; i > 0; --i)
    if (searcher->history.items[i - 1].kind == SearchHistoryItemKindNextMatch)
      return true;

  return false;
}

void searcher_delete(Searcher *searcher) {
  for (u32 i = 0; i < searcher->history.len; ++i)
    if (searcher->history.items[i].kind == SearchHistoryItemKindUpdatePattern &&
        searcher->history.items[i].prev_pattern.ptr)
      free(searcher->history.items[i].prev_pattern.ptr);
  if (searcher->history.items)
    free(searcher->history.items);
  if (searcher->pattern.ptr)
    free(searcher->pattern.ptr);
}
