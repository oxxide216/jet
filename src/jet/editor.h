#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "provider.h"
#include "../common/protocol.h"

typedef struct {
  Buffer  buffer;
  u32     scroll;
  char   *file_path;
  Str     abs_file_path;
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

  Provider *provider;
  WideStrs  options;
  u32       selected_option;
  f32       palette_scroll_x;
  u32       palette_scroll_y;

  JetMode mode;

  f32 font_scale;

  bool           is_jwrap_connected;
  MessageEntries errors;
  MessageEntries warnings;
  MessageEntries infos;
};

MainBuffer main_buffer_make(char *path);
void       editor_clear_entries(Editor *editor);

#endif // EDITOR_H
