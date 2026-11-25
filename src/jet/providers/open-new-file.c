#include "../provider.h"
#include "../editor.h"
#include "../buffer.h"
#include "../platform.h"
#include "../config.h"

static PaletteOptions get_opts(Editor *editor, WideStr content) {
  (void) editor;

  PaletteOptions result;
  result.len = 1;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  result.items[0] = (PaletteOption) { {}, BG_COLOR, FG_COLOR };
  result.items[0].name.len = content.len;
  result.items[0].name.ptr =
    malloc(result.items[0].name.len * sizeof(*result.items[0].name.ptr));
  memcpy(result.items[0].name.ptr, content.ptr,
         result.items[0].name.len * sizeof(*result.items[0].name.ptr));
  return result;
}

static void free_opts(PaletteOptions options) {
  if (options.len > 0) {
    free(options.items[0].name.ptr);
    free(options.items);
  }
}

static bool execute(Editor *editor, PaletteOptions options, u32 index) {
  char *path = wide_str_to_cstr(options.items[index].name);

  MainBuffer main_buffer = main_buffer_make(path);
  buffer_read_file(&main_buffer.buffer, path);
  main_buffer_rebuild_entries(&main_buffer, editor);
  highlighter_do_main_buffer(&main_buffer);
  DA_APPEND(editor->main_buffers, main_buffer);
  editor_build_completions(editor, editor->main_buffers.len - 1);
  editor->completing = false;
  editor_switch_main_buffer(editor, editor->main_buffers.len - 1);

  return true;
}

Provider open_new_file_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
