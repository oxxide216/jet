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

void editor_update_inline_errors_before_action(Editor *editor, Action action) {
  MainBuffer *main_buffer =
    editor->main_buffers.items + editor->current_main_buffer_index;

  if (editor->mode != JetModeEditor ||
      (main_buffer->errors.len == 0 &&
       main_buffer->warnings.len == 0 &&
       main_buffer->infos.len == 0))
    return;

  MessageEntryPtrs *ptrs[] = {
    &main_buffer->errors,
    &main_buffer->warnings,
    &main_buffer->infos,
  };
  u32 rows = buffer_get_rows(&main_buffer->buffer);

  switch (action) {
  case ActionAddLine: {
    for (u32 i = 0; i < ARRAY_LEN(ptrs); ++i) {
      if (ptrs[i]->len < rows + 1)
        DA_APPEND(*ptrs[i], NULL);

      for (u32 j = ptrs[i]->len; j > main_buffer->buffer.cursor_row + 1; --j) {
        ptrs[i]->items[j - 1] = ptrs[i]->items[j - 2];
        ptrs[i]->items[j - 2] = NULL;
      }
    }
  } break;

  case ActionRemoveLineBeforeCursor: {
    if (main_buffer->buffer.cursor_row > 0) {
      for (u32 i = 0; i < ARRAY_LEN(ptrs); ++i) {
        for (u32 j = main_buffer->buffer.cursor_row; j < ptrs[i]->len; ++j) {
          ptrs[i]->items[j - 1] = ptrs[i]->items[j];
          ptrs[i]->items[j] = NULL;
        }
      }
    }
  } break;

  case ActionRemoveLineAfterCursor: {
    if (main_buffer->buffer.cursor_row + 1 < rows) {
      for (u32 i = 0; i < ARRAY_LEN(ptrs); ++i) {
        for (u32 j = main_buffer->buffer.cursor_row + 1; j < ptrs[i]->len; ++j) {
          ptrs[i]->items[j - 1] = ptrs[i]->items[j];
          ptrs[i]->items[j] = NULL;
        }
      }
    }
  } break;

  case ActionAdd:                break;
  case ActionRemoveBeforeCursor: break;
  case ActionRemoveAfterCursor:  break;
  }
}

void editor_build_completions(Editor *editor, u32 main_buffer_index) {
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
            Completion new_completion = { {}, i, anchor, main_buffer->abs_file_path, 0 };
            new_completion.wsb.len = j - anchor;
            new_completion.wsb.cap = new_completion.wsb.len;
            new_completion.wsb.items =
              malloc(new_completion.wsb.len * sizeof(*new_completion.wsb.items));
            memcpy(new_completion.wsb.items, line.ptr + anchor,
                   new_completion.wsb.len * sizeof(*new_completion.wsb.items));
            new_completion.hash = wide_str_hash((WideStr) {
                new_completion.wsb.items,
                new_completion.wsb.len,
              });
            DA_APPEND(editor->completions, new_completion);
          }
          found_word = false;
        }
      }
    }

    if (found_word && line.len > anchor) {
      Completion new_completion = { {}, i, anchor, main_buffer->abs_file_path, 0 };
      new_completion.wsb.len = line.len - anchor;
      new_completion.wsb.cap = new_completion.wsb.len;
      new_completion.wsb.items =
        malloc(new_completion.wsb.len * sizeof(*new_completion.wsb.items));
      memcpy(new_completion.wsb.items, line.ptr + anchor,
             new_completion.wsb.len * sizeof(*new_completion.wsb.items));
      new_completion.hash = wide_str_hash((WideStr) {
          new_completion.wsb.items,
          new_completion.wsb.len,
        });
      DA_APPEND(editor->completions, new_completion);
    }
  }
}

void editor_update_current_row_completions_begin(Editor *editor) {
  if (!editor->completing)
    return;

  MainBuffer *main_buffer =
    editor->main_buffers.items + editor->current_main_buffer_index;

  if (editor->prev_current_main_buffer_index !=
      editor->current_main_buffer_index) {
    editor->current_row_completions_begin = 0;
    while (editor->current_row_completions_begin < editor->completions.len &&
           (editor->completions.items[editor->current_row_completions_begin].row <
            main_buffer->buffer.cursor_row ||
            !str_eq(editor->completions.items[editor->current_row_completions_begin].abs_file_path,
                    main_buffer->abs_file_path)))
      ++editor->current_row_completions_begin;

    editor->prev_current_main_buffer_index = editor->current_main_buffer_index;
    editor->prev_cursor_row = main_buffer->buffer.cursor_row;

    return;
  }

  if (editor->prev_cursor_row < main_buffer->buffer.cursor_row) {
    while (editor->current_row_completions_begin < editor->completions.len &&
           (editor->completions.items[editor->current_row_completions_begin].row <
            main_buffer->buffer.cursor_row ||
            !str_eq(editor->completions.items[editor->current_row_completions_begin].abs_file_path,
                    main_buffer->abs_file_path)))
      ++editor->current_row_completions_begin;

    editor->prev_cursor_row = main_buffer->buffer.cursor_row;
  } else if (editor->prev_cursor_row > main_buffer->buffer.cursor_row) {
    while (editor->current_row_completions_begin > 0 &&
           (editor->completions.items[editor->current_row_completions_begin].row >
            main_buffer->buffer.cursor_row ||
            !str_eq(editor->completions.items[editor->current_row_completions_begin].abs_file_path,
                    main_buffer->abs_file_path)))
      --editor->current_row_completions_begin;

    if (editor->completions.items[editor->current_row_completions_begin].row <
        main_buffer->buffer.cursor_row)
      ++editor->current_row_completions_begin;

    editor->prev_cursor_row = main_buffer->buffer.cursor_row;
  }
}

void editor_update_completions_before_action(Editor *editor, Action action, u32 param) {
  MainBuffer *main_buffer =
    editor->main_buffers.items + editor->current_main_buffer_index;
  u32 rows = buffer_get_rows(&main_buffer->buffer);

  switch (action) {
  case ActionAddLine: {
    for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
      Completion *completion = editor->completions.items + i;
      if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path))
        break;
      if (completion->col >= main_buffer->buffer.cursor_col)
        ++completion->row;
    }
  } break;

  case ActionRemoveLineBeforeCursor: {
    if (main_buffer->buffer.cursor_row > 0) {
      for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
        Completion *completion = editor->completions.items + i;
        if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path))
          break;
        if (completion->col >= main_buffer->buffer.cursor_col)
          ++completion->row;
      }
    }
  } break;

  case ActionRemoveLineAfterCursor: {
    if (main_buffer->buffer.cursor_row + 1 < rows) {
      for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
        Completion *completion = editor->completions.items + i;
        if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path))
          break;
        if (completion->row > main_buffer->buffer.cursor_row)
          ++completion->row;
      }
    }
  } break;

  case ActionAdd: {
    if (!is_part_of_word(param))
      break;

    bool prev_was_before_cursor = false;
    bool line_empty = true;
    for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
      Completion *completion = editor->completions.items + i;
      if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path) ||
          completion->row > main_buffer->buffer.cursor_row)
        break;
      line_empty = false;
      if (completion->col > main_buffer->buffer.cursor_col) {
        ++completion->col;
        prev_was_before_cursor = true;
      } else if (completion->col + completion->wsb.len >= main_buffer->buffer.cursor_col) {
        DA_INSERT(completion->wsb,
                  main_buffer->buffer.cursor_col - completion->col,
                  param);
        completion->hash = wide_str_hash((WideStr) {
            completion->wsb.items,
            completion->wsb.len,
          });
        prev_was_before_cursor = false;
      } else if (prev_was_before_cursor) {
        WideStr word = buffer_get_word_at_cursor(&main_buffer->buffer);
        Completion new_completion = {
          {},
          main_buffer->buffer.cursor_row,
          main_buffer->buffer.cursor_col,
          main_buffer->abs_file_path,
          0,
        };
        new_completion.wsb.len = word.len + 1;
        new_completion.wsb.cap = new_completion.wsb.len;
        new_completion.wsb.items =
          malloc(new_completion.wsb.len * sizeof(*new_completion.wsb.items));
        memcpy(new_completion.wsb.items,
               word.ptr,
               word.len * sizeof(*new_completion.wsb.items));
        new_completion.wsb.items[word.len] = param;
        new_completion.hash = wide_str_hash((WideStr) {
            new_completion.wsb.items,
            new_completion.wsb.len,
          });
        DA_INSERT(editor->completions, i, new_completion);
      }
    }

    if (line_empty) {
      WideStr word = buffer_get_word_at_cursor(&main_buffer->buffer);
      Completion new_completion = {
        {},
        main_buffer->buffer.cursor_row,
        main_buffer->buffer.cursor_col,
        main_buffer->abs_file_path,
        0,
      };
      new_completion.wsb.len = word.len + 1;
      new_completion.wsb.cap = new_completion.wsb.len;
      new_completion.wsb.items =
        malloc(new_completion.wsb.len * sizeof(*new_completion.wsb.items));
      memcpy(new_completion.wsb.items,
             word.ptr,
             word.len * sizeof(*new_completion.wsb.items));
      new_completion.wsb.items[word.len] = param;
      new_completion.hash = wide_str_hash((WideStr) {
          new_completion.wsb.items,
          new_completion.wsb.len,
        });
      DA_INSERT(editor->completions,
                editor->current_row_completions_begin,
                new_completion);
    }
  } break;

  case ActionRemoveBeforeCursor: {
    if (main_buffer->buffer.cursor_col > 0) {
      for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
        Completion *completion = editor->completions.items + i;
        if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path) ||
            completion->row > main_buffer->buffer.cursor_row)
          break;
        if (completion->col >= main_buffer->buffer.cursor_col) {
          completion->col -= param;
        } else if (completion->col <=
                   main_buffer->buffer.cursor_col &&
                   completion->col + completion->wsb.len >=
                   main_buffer->buffer.cursor_col - param) {
          for (u32 j = 0; j < param; ++j) {
            if (completion->wsb.len <
                main_buffer->buffer.cursor_col - completion->col - param)
              break;
            DA_REMOVE_AT(completion->wsb,
                         main_buffer->buffer.cursor_col - completion->col - param);
          }

          if (completion->wsb.len == 0) {
            free(editor->completions.items[i].wsb.items);
            DA_REMOVE_AT(editor->completions, i);
            --i;
          } else {
            editor->completions.items[i].hash = wide_str_hash((WideStr) {
                editor->completions.items[i].wsb.items,
                editor->completions.items[i].wsb.len,
              });
          }
        }
      }
    }
  } break;

  case ActionRemoveAfterCursor: {
      for (u32 i = editor->current_row_completions_begin; i < editor->completions.len; ++i) {
        Completion *completion = editor->completions.items + i;
        if (!str_eq(completion->abs_file_path, main_buffer->abs_file_path) ||
            completion->row > main_buffer->buffer.cursor_row)
          break;
        if (completion->col >= main_buffer->buffer.cursor_col + param) {
          completion->col -= param;
        } else if (completion->col + completion->wsb.len >
                   main_buffer->buffer.cursor_col &&
                   completion->col <
                   main_buffer->buffer.cursor_col + param) {
          for (u32 j = 0; j < param; ++j)
            if (completion->wsb.len >
                main_buffer->buffer.cursor_col - completion->col)
              DA_REMOVE_AT(completion->wsb,
                           main_buffer->buffer.cursor_col - completion->col);

          if (completion->wsb.len == 0) {
            free(editor->completions.items[i].wsb.items);
            DA_REMOVE_AT(editor->completions, i);
            --i;
          } else {
            editor->completions.items[i].hash = wide_str_hash((WideStr) {
                editor->completions.items[i].wsb.items,
                editor->completions.items[i].wsb.len,
              });
          }
        }
      }
  } break;
  }
}

void editor_remove_invalidated_completions(Editor *editor, Str invalidated_abs_file_path) {
  for (u32 i = editor->completions.len; i > 0; --i) {
    Completion *completion = editor->completions.items + i - 1;
    if (str_eq(completion->abs_file_path, invalidated_abs_file_path)) {
      free(completion->wsb.items);
      DA_REMOVE_AT(editor->completions, i - 1);
    }
  }
}
