#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "provider.h"
#include "../common/protocol.h"
#include "cns/cns.h"

typedef Da(MessageEntry *) MessageEntryPtrs;

typedef struct {
  Buffer            buffer;
  u32               scroll;
  char             *file_path;
  Str               abs_file_path;
  MessageEntryPtrs  errors;
  MessageEntryPtrs  warnings;
  MessageEntryPtrs  infos;
} MainBuffer;

typedef Da(MainBuffer) MainBuffers;

typedef enum {
  JetModeEditor = 0,
  JetModeCommandPalette,
} JetMode;

typedef struct {
  WideStringBuilder wsb;
  u32               row, col;
  Str               abs_file_path;
} Completion;

typedef Da(Completion) Completions;

typedef enum {
  ActionAddLine = 0,
  ActionRemoveLineBeforeCursor,
  ActionRemoveLineAfterCursor,
  ActionAdd,
  ActionRemoveBeforeCursor,
  ActionRemoveAfterCursor,
} Action;

struct Editor {
  MainBuffers main_buffers;
  Buffer      palette_buffer;
  u32         current_main_buffer_index;

  Provider       *provider;
  PaletteOptions  options;
  u32             selected_option;
  f32             palette_scroll_x;
  u32             palette_scroll_y;

  JetMode mode;

  u32 font_scale;

  CnsConnection  *jwrap;
  MessageEntries  errors;
  MessageEntries  warnings;
  MessageEntries  infos;
  u32             entry_cursor;

  bool        completing;
  Completions completions;
  Completions actual_completions;
  u32         completions_scroll;
  u32         current_row_completions_begin;
  u32         prev_cursor_row;
  u32         prev_current_main_buffer_index;
};

MainBuffer    main_buffer_make(char *path);
void          main_buffer_rebuild_entries(MainBuffer *buffer, Editor *editor);

Buffer       *editor_current_buffer(Editor *editor);
void          editor_clear_entries(Editor *editor);
MessageEntry *editor_get_entry(Editor *editor, u32 index);
void          editor_go_to_entry(Editor *editor, MessageEntry *entry);
void          editor_update_inline_errors_before_action(Editor *editor, Action action);
void          editor_build_completions(Editor *editor, u32 main_buffer_index);
void          editor_update_current_row_completions_begin(Editor *editor);
void          editor_update_completions_before_action(Editor *editor,
                                                      Action action,
                                                      u32 param);
void          editor_remove_invalidated_completions(Editor *editor,
                                                    Str invalidated_abs_file_path);

#endif // EDITOR_H
