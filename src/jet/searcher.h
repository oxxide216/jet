#ifndef SEARCHER_H
#define SEARCHER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "buffer.h"

typedef enum {
  SearchHistoryItemKindNextMatch = 0,
  SearchHistoryItemKindUpdatePattern,
} SearchHistoryItemKind;

typedef struct {
  SearchHistoryItemKind kind;
  union {
    struct {
      u32 prev_match_row;
      u32 prev_match_col;
    };
    WideStr prev_pattern;
  };
} SearchHistoryItem;

typedef Da(SearchHistoryItem) SearchHistory;

typedef struct {
  Buffer        *buffer;
  SearchHistory  history;
  WideStr        pattern;
} Searcher;

void searcher_init(Searcher *searcher, Buffer *buffer);
void searcher_update_pattern(Searcher *searcher, WideStr pattern);
bool searcher_next_match(Searcher *searcher, u32 *row, u32 *col, bool add_to_historty);
bool searcher_prev_match(Searcher *searcher, u32 *row, u32 *col, bool add_to_historty);
bool searcher_undo(Searcher *searcher, u32 *row, u32 *col, Buffer *search_buffer);
bool searcher_has_succesful_searches_in_history(Searcher *searcher);
void searcher_delete(Searcher *searcher);

#endif // SEARCHER_H
