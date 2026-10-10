#include "editor.h"
#include "platform.h"
#include "config.h"

MainBuffer main_buffer_make(char *path) {
  Str abs_path = {0};
  if (path) {
    Str cwd = get_current_dir();
    u32 path_len = strlen(path);
    abs_path.len = cwd.len + 1 + path_len;
    abs_path.ptr = malloc(abs_path.len * sizeof(*abs_path.ptr));
    memcpy(abs_path.ptr, cwd.ptr, cwd.len * sizeof(*abs_path.ptr));
    abs_path.ptr[cwd.len] = '/';
    memcpy(abs_path.ptr + cwd.len + 1, path, path_len * sizeof(*path));
    free(cwd.ptr);
  }
  return (MainBuffer) { buffer_make(), 0.0, path, abs_path, {}, {}, {} };
}

void main_buffer_rebuild_entries(MainBuffer *buffer, Editor *editor) {
  buffer->errors.len = 0;
  buffer->warnings.len = 0;
  buffer->infos.len = 0;

  for (u32 i = 0; i < editor->errors.len; ++i) {
    if (str_eq(editor->errors.items[i].file_path, buffer->abs_file_path)) {
      while (buffer->errors.len < editor->errors.items[i].row)
        DA_APPEND(buffer->errors, NULL);
      buffer->errors.items[editor->errors.items[i].row - 1] = editor->errors.items + i;
    }
  }

  for (u32 i = 0; i < editor->warnings.len; ++i) {
    if (str_eq(editor->warnings.items[i].file_path, buffer->abs_file_path)) {
      while (buffer->warnings.len < editor->warnings.items[i].row)
        DA_APPEND(buffer->warnings, NULL);
      buffer->warnings.items[editor->warnings.items[i].row - 1] = editor->warnings.items + i;
    }
  }

  for (u32 i = 0; i < editor->infos.len; ++i) {
    if (str_eq(editor->infos.items[i].file_path, buffer->abs_file_path)) {
      while (buffer->infos.len < editor->infos.items[i].row)
        DA_APPEND(buffer->infos, NULL);
      buffer->infos.items[editor->infos.items[i].row - 1] = editor->infos.items + i;
    }
  }
}

Buffer *editor_current_buffer(Editor *editor) {
  switch (editor->mode) {
  case JetModeEditor:         return &editor->main_buffers.items[editor->current_main_buffer_index].buffer;
  case JetModeCommandPalette: return &editor->palette_buffer;
  }

  return NULL;
}

void editor_clear_entries(Editor *editor) {
  MessageEntries *ptrs[] = {
    &editor->errors,
    &editor->warnings,
    &editor->infos,
  };

  for (u32 i = 0; i < ARRAY_LEN(ptrs); ++i) {
    for (u32 j = 0; j < ptrs[i]->len; ++j) {
      free(ptrs[i]->items[j].file_path.ptr);
      free(ptrs[i]->items[j].message.ptr);
    }
    if (ptrs[i]->items)
      free(ptrs[i]->items);
    *ptrs[i] = (MessageEntries) {0};
  }
}

MessageEntry *editor_get_entry(Editor *editor, u32 index) {
  if (index < editor->errors.len)
    return editor->errors.items + index;
  else if (index < editor->warnings.len + editor->errors.len)
    return editor->warnings.items + index - editor->errors.len;
  else
    return editor->infos.items + index - editor->errors.len - editor->warnings.len;
}

void editor_go_to_entry(Editor *editor, MessageEntry *entry) {
  u32 main_buffer_index = (u32) -1;

  for (u32 i = 0; i < editor->main_buffers.len; ++i) {
    if (str_eq(editor->main_buffers.items[i].abs_file_path, entry->file_path)) {
      main_buffer_index = i;
      break;
    }
  }

  if (main_buffer_index == (u32) -1) {
    char *path = malloc(entry->file_path.len + 1);
    memcpy(path, entry->file_path.ptr, entry->file_path.len);
    path[entry->file_path.len] = '\0';

    char *rel_path = path + entry->file_path.len;
    while (rel_path > path && rel_path[-1] != '/')
      --rel_path;

    char *new_rel_path = malloc(entry->file_path.len - (rel_path - path) + 1);
    strcpy(new_rel_path, rel_path);

    editor->current_main_buffer_index = editor->main_buffers.len;
    MainBuffer main_buffer = main_buffer_make(new_rel_path);
    buffer_read_file(&main_buffer.buffer, path);
    main_buffer_rebuild_entries(&main_buffer, editor);
    main_buffer.buffer.cursor_row = entry->row - 1;
    if (main_buffer.buffer.cursor_row > buffer_get_rows(&main_buffer.buffer))
      main_buffer.buffer.cursor_row = buffer_get_rows(&main_buffer.buffer);
    main_buffer.buffer.cursor_col = entry->col - 1;
    WideStr line = buffer_get_current_line(&main_buffer.buffer);
    if (main_buffer.buffer.cursor_col > line.len)
      main_buffer.buffer.cursor_col = line.len;
    DA_APPEND(editor->main_buffers, main_buffer);
  } else {
    editor->current_main_buffer_index = main_buffer_index;
    Buffer *buffer = &editor->main_buffers.items[editor->current_main_buffer_index].buffer;
    buffer->cursor_row = entry->row - 1;
    if (buffer->cursor_row > buffer_get_rows(buffer))
      buffer->cursor_row = buffer_get_rows(buffer);
    buffer->cursor_col = entry->col - 1;
    WideStr line = buffer_get_current_line(buffer);
    if (buffer->cursor_col > line.len)
      buffer->cursor_col = line.len;
  }
}

void editor_build_completions(Editor *editor, u32 main_buffer_index, u32 begin) {
  MainBuffer *main_buffer = editor->main_buffers.items + main_buffer_index;

  u32 rows = buffer_get_rows(&main_buffer->buffer);
  for (u32 i = 0; i < rows; ++i) {
    WideStr line = buffer_get_line(&main_buffer->buffer, i);
    u32 anchor = 0;
    bool found_word = false;
    for (u32 j = 0; j < line.len; ++j) {
      if (is_part_of_word(line.ptr[j])) {
        if (!found_word) {
          anchor = j;
          found_word = true;
        }
      } else {
        if (found_word) {
          if (j - anchor >= MINIMAL_COMPLETION_PREFIX_LENGTH) {
            Completion completion = { i, anchor, j - anchor, main_buffer->abs_file_path };
            if (begin == (u32) -1) {
              DA_APPEND(editor->completions, completion);
            } else {
              if (begin < editor->completions.len &&
                  str_eq(editor->completions.items[begin].abs_file_path,
                         main_buffer->abs_file_path))
                editor->completions.items[begin] = completion;
              else
                DA_INSERT(editor->completions, begin, completion);
              ++begin;
            }
          }
          found_word = false;
        }
      }
    }

    if (found_word && line.len > anchor) {
      Completion completion = { i, anchor, line.len - anchor, main_buffer->abs_file_path };
      if (begin == (u32) -1) {
        DA_APPEND(editor->completions, completion);
      } else {
        if (begin < editor->completions.len &&
            str_eq(editor->completions.items[begin].abs_file_path,
                   main_buffer->abs_file_path))
          editor->completions.items[begin] = completion;
        else
          DA_INSERT(editor->completions, begin, completion);
        ++begin;
      }
    }
  }
}

void editor_update_completion_at_cursor_if_cursor_moved(Editor *editor) {
  MainBuffer *main_buffer = editor->main_buffers.items + editor->current_main_buffer_index;
  Buffer *buffer = &main_buffer->buffer;

  bool is_path_same = str_eq(editor->prev_abs_file_path, main_buffer->abs_file_path);

  if (editor->prev_cursor_row == buffer->cursor_row &&
      editor->prev_cursor_col == buffer->cursor_col &&
      is_path_same)
    return;

  u32 begin =
    (editor->completion_at_cursor_index == (u32) -1 ||
     !is_path_same ||
     editor->prev_cursor_row > buffer->cursor_row ||
     (editor->prev_cursor_row == buffer->cursor_row &&
      editor->prev_cursor_col > buffer->cursor_col)) ?
    0 :
    editor->completion_at_cursor_index;

  editor->completion_at_cursor_index = (u32) -1;

  for (u32 i = begin; i < editor->completions.len; ++i) {
    Completion *completion = editor->completions.items + i;
    if (str_eq(main_buffer->abs_file_path, completion->abs_file_path)) {
      if (buffer->cursor_row == completion->row &&
          buffer->cursor_col >= completion->col &&
          buffer->cursor_col <= completion->col + completion->len) {
        editor->completion_at_cursor_index = i;
        break;
      } else if (buffer->cursor_row > completion->row ||
                 (buffer->cursor_row == completion->row &&
                  buffer->cursor_col > completion->col)) {
        editor->completion_before_cursor_index = i;
      }
    }
  }

  editor->prev_cursor_row = buffer->cursor_row;
  editor->prev_cursor_col = buffer->cursor_col;
  editor->prev_abs_file_path = main_buffer->abs_file_path;
}

void editor_update_completions_on_buffer_change(Editor *editor) {
  MainBuffer *main_buffer = editor->main_buffers.items + editor->current_main_buffer_index;

  editor_update_completion_at_cursor_if_cursor_moved(editor);

  u32 begin = (u32) -1;
  for (u32 i = 0; i < editor->completions.len; ++i) {
    if (str_eq(editor->completions.items[i].abs_file_path, main_buffer->abs_file_path)) {
      begin = i;
      break;
    }
  }

  if (begin != (u32) -1)
    editor_build_completions(editor, editor->current_main_buffer_index, begin);
}

void editor_remove_invalidated_completions(Editor *editor, Str invalidated_abs_file_path) {
  for (u32 i = editor->completions.len; i > 0; --i) {
    Completion *completion = editor->completions.items + i - 1;
    if (str_eq(completion->abs_file_path, invalidated_abs_file_path)) {
      if (i - 1 == editor->completion_at_cursor_index)
        editor->completion_at_cursor_index = (u32) -1;
      else if (i - 1 < editor->completion_at_cursor_index)
        --editor->completion_at_cursor_index;
      DA_REMOVE_AT(editor->completions, i - 1);
    }
  }
}
