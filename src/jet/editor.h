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
  u32 row, col, len;
  Str abs_file_path;
} Completion;

typedef Da(Completion) Completions;

struct Editor {
  MainBuffers main_buffers;
  Buffer      palette_buffer;
  u32         current_main_buffer_index;
  u32         prev_current_main_buffer_index;

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

  u32 prev_current_main_buffer_rows;
  u32 prev_current_main_buffer_cursor_row;

  bool        completing;
  Completions completions;
  Completions actual_completions;
  u32         completions_scroll;
  u32         prev_cursor_row;
  u32         prev_cursor_col;
  Str         prev_abs_file_path;
  u32         completion_before_cursor_index;
  u32         completion_at_cursor_index;
};

MainBuffer    main_buffer_make(char *path);
void          main_buffer_rebuild_entries(MainBuffer *buffer, Editor *editor);

Buffer       *editor_current_buffer(Editor *editor);
void          editor_clear_entries(Editor *editor);
MessageEntry *editor_get_entry(Editor *editor, u32 index);
void          editor_go_to_entry(Editor *editor, MessageEntry *entry);
void          editor_build_completions(Editor *editor, u32 main_buffer_index, u32 begin);
void          editor_update_completion_at_cursor_if_cursor_moved(Editor *editor);
void          editor_update_completions_on_buffer_change(Editor *editor);
void          editor_remove_invalidated_completions(Editor *editor,
                                                    Str invalidated_abs_file_path);

#endif // EDITOR_H
