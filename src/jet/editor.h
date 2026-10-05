#ifndef EDITOR_H
#define EDITOR_H

#include "buffer.h"
#include "provider.h"

typedef enum {
  JetModeEditor = 0,
  JetModeCommandPalette,
} JetMode;

struct Editor {
  Buffer  editor_buffer;
  Buffer  palette_buffer;
  Buffer *current_buffer;

  char *current_file_path;

  Provider *provider;
  WideStrs  options;
  u32       selected_option;

  JetMode mode;

  f32 font_scale;
};

#endif // EDITOR_H
