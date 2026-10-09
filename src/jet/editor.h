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

struct Editor {
  MainBuffers  main_buffers;
  Buffer       palette_buffer;
  Buffer      *current_buffer;
  u32          current_main_buffer_index;

  Provider       *provider;
  PaletteOptions  options;
  u32             selected_option;
  f32             palette_scroll_x;
  u32             palette_scroll_y;

  JetMode mode;

  f32 font_scale;

  CnsConnection  *jwrap;
  MessageEntries  errors;
  MessageEntries  warnings;
  MessageEntries  infos;
  u32             entry_cursor;
};

MainBuffer    main_buffer_make(char *path);
void          main_buffer_rebuild_entries(MainBuffer *buffer, Editor *editor);

void          editor_clear_entries(Editor *editor);
MessageEntry *editor_get_entry(Editor *editor, u32 index);
void          editor_go_to_entry(Editor *editor, MessageEntry *entry);

#endif // EDITOR_H
