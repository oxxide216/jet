#include "../provider.h"
#include "../editor.h"
#include "../buffer.h"
#include "../config.h"

static PaletteOptions get_opts(Editor *editor, WideStr content) {
  (void) editor;

  PaletteOptions result;
  result.len = 1;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  result.items[0] = (PaletteOption) { content, BG_COLOR, FG_COLOR };
  return result;
}

static void free_opts(PaletteOptions options) {
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, PaletteOptions options, u32 index) {
  (void) options;
  (void) index;

  MainBuffer *current_buffer = editor->main_buffers.items + editor->current_main_buffer_index;
  char *path = wide_str_to_cstr(buffer_get_current_line(editor_current_buffer(editor)));
  buffer_write_file(&current_buffer->buffer, path);
  current_buffer->file_path = path;

  if (editor->jwrap) {
    ByteBuffer buffer = {0};
    Message message = { MessageKindRerun, {} };
    encode_message(&buffer, &message);
    cns_unix_send(editor->jwrap, buffer.items, buffer.len);
    free(buffer.items);
  }

  return true;
}

Provider save_new_file_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
