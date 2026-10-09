#include "../provider.h"
#include "../editor.h"
#include "../config.h"

#define GET_OPTS_LOOP_BODY(bg_color, fg_color)                                     \
  do {                                                                             \
    WideStringBuilder wsb = {0};                                                   \
    wsb_push_str(&wsb, entry->file_path);                                          \
    wsb_push_str(&wsb, STR_LIT(":"));                                              \
    wsb_push_u32(&wsb, entry->row);                                                \
    wsb_push_str(&wsb, STR_LIT(":"));                                              \
    wsb_push_u32(&wsb, entry->col);                                                \
    wsb_push_str(&wsb, STR_LIT(": "));                                             \
    u32 message_begin = wsb.len;                                                   \
    wsb_push_str(&wsb, entry->message);                                            \
    WideStr entry_wstr = { wsb.items, wsb.len };                                   \
    WideStr message_wstr = { wsb.items + message_begin, wsb.len - message_begin }; \
    if (wide_str_begins_with(entry_wstr, content) ||                               \
        wide_str_begins_with(message_wstr, content)) {                             \
      PaletteOption option = { entry_wstr, bg_color, fg_color };                   \
      DA_APPEND(result, option);                                                   \
    } else {                                                                       \
      free(wsb.items);                                                             \
    }                                                                              \
  } while (0)

static PaletteOptions get_opts(Editor *editor, WideStr content) {
  PaletteOptions result = {0};

  for (u32 i = 0; i < editor->errors.len; ++i) {
    MessageEntry *entry = editor->errors.items + i;
    GET_OPTS_LOOP_BODY(BG_COLOR, ERROR_COLOR);
  }

  for (u32 i = 0; i < editor->warnings.len; ++i) {
    MessageEntry *entry = editor->warnings.items + i;
    GET_OPTS_LOOP_BODY(BG_COLOR, WARN_COLOR);
  }

  for (u32 i = 0; i < editor->infos.len; ++i) {
    MessageEntry *entry = editor->infos.items + i;
    GET_OPTS_LOOP_BODY(BG_COLOR, INFO_COLOR);
  }

  return result;
}

static void free_opts(PaletteOptions options) {
  for (u32 i = 0; i < options.len; ++i)
    free(options.items[i].name.ptr);
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, PaletteOptions options, u32 index) {
  (void) options;

  editor->entry_cursor = index;
  editor_go_to_entry(editor, editor_get_entry(editor, index));

  return true;
}

Provider error_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
