#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "provider.h"
#include "highlighter.h"
#include "searcher.h"
#include "../common/protocol.h"
#include "cns/cns.h"

typedef Da(MessageEntry *) MessageEntryPtrs;

typedef struct {
  WideStringBuilder wsb;
  u64               hash;
} Completion;

typedef Da(Completion) Completions;
typedef Da(Completions) Completionss;

struct MainBuffer {
  Buffer            buffer;
  u32               scroll;
  char             *file_path;
  Str               abs_file_path;
  MessageEntryPtrs  errors;
  MessageEntryPtrs  warnings;
  MessageEntryPtrs  infos;
  Completionss      completion_lines;
  Highlighter       highlighter;
};

typedef Da(MainBuffer) MainBuffers;

typedef enum {
  JetModeEditor = 0,
  JetModeCommandPalette,
  JetModeSearch,
} JetMode;

struct Editor {
  MainBuffers main_buffers;
  Buffer      palette_buffer;
  Buffer      search_buffer;
  u32         current_main_buffer_index;

  Provider       *provider;
  PaletteOptions  options;
  u32             selected_option;
  f32             palette_scroll_x;
  u32             palette_scroll_y;
  f32             palette_alpha;

  JetMode mode;

  u32 font_scale;
  f32 x_offset;

  CnsConnection  *jwrap;
  MessageEntries  errors;
  MessageEntries  warnings;
  MessageEntries  infos;
  u32             entry_cursor;

  bool         completing;
  Completions  actual_completions;
  u32          completions_scroll;

  Searcher searcher;
  f32      search_scroll_x;
  bool     is_searching_backwards;

  u32 max_visible_line_number;
};

MainBuffer    main_buffer_make(char *path);
void          main_buffer_rebuild_entries(MainBuffer *buffer, Editor *editor);

Buffer       *editor_current_buffer(Editor *editor);
void          editor_clear_entries(Editor *editor);
MessageEntry *editor_get_entry(Editor *editor, u32 index);
void          editor_go_to_entry(Editor *editor, MessageEntry *entry);
void          editor_update_inline_errors_before_action(Editor *editor, Action action);
void          editor_build_completions(Editor *editor, u32 main_buffer_index);
void          editor_update_completions_before_action(Editor *editor,
                                                      Action action);
void          editor_update_before_buffer_action(Editor *editor,
                                                 BufferAction *action,
                                                 bool is_redo);

#endif // EDITOR_H
