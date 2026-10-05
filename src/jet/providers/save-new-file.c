#include "../provider.h"
#include "../editor.h"
#include "../buffer.h"

static WideStrs get_opts(WideStr content) {
  WideStrs result;
  result.len = 1;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  result.items[0] = content;
  return result;
}

static void free_opts(WideStrs options) {
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, u32 index) {
  (void) index;

  char *path = wide_str_to_cstr(buffer_get_current_line(editor->current_buffer));
  buffer_write_file(&editor->editor_buffer, path);
  editor->current_file_path = path;

  return true;
}

Provider save_new_file_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
