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
  return (MainBuffer) { buffer_make(), 0.0, path, abs_path, {}, {}, {}, {}, {} };
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
  case JetModeSearch:         return &editor->search_buffer;
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
    main_buffer.abs_file_path.len = entry->file_path.len;
    main_buffer.abs_file_path.ptr =
      malloc(main_buffer.abs_file_path.len * sizeof(*main_buffer.abs_file_path.ptr));
    memcpy(main_buffer.abs_file_path.ptr,
           entry->file_path.ptr,
           main_buffer.abs_file_path.len * sizeof(*main_buffer.abs_file_path.ptr));
    buffer_read_file(&main_buffer.buffer, path);
    main_buffer_rebuild_entries(&main_buffer, editor);
    highlighter_do_main_buffer(&main_buffer);
    main_buffer.buffer.cursor_row = entry->row - 1;
    if (main_buffer.buffer.cursor_row > buffer_get_rows(&main_buffer.buffer))
      main_buffer.buffer.cursor_row = buffer_get_rows(&main_buffer.buffer);
    main_buffer.buffer.cursor_col = entry->col - 1;
    WideStr line = buffer_get_current_line(&main_buffer.buffer);
    if (main_buffer.buffer.cursor_col > line.len)
      main_buffer.buffer.cursor_col = line.len;
    DA_APPEND(editor->main_buffers, main_buffer);
    editor_build_completions(editor, editor->main_buffers.len - 1);
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
  case ActionInsertLine: {
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

  case ActionRemoveLineAtCursor: {
    if (main_buffer->buffer.cursor_row + 1 < rows) {
      for (u32 i = 0; i < ARRAY_LEN(ptrs); ++i) {
        for (u32 j = main_buffer->buffer.cursor_row + 1; j < ptrs[i]->len; ++j) {
          ptrs[i]->items[j - 1] = ptrs[i]->items[j];
          ptrs[i]->items[j] = NULL;
        }
      }
    }
  } break;

  case ActionInsert:             break;
  case ActionRemoveBeforeCursor: break;
  case ActionRemoveAtCursor:     break;
  }
}

void editor_build_completions(Editor *editor, u32 main_buffer_index) {
  MainBuffer *main_buffer = editor->main_buffers.items + main_buffer_index;

  u32 rows = buffer_get_rows(&main_buffer->buffer);
  for (u32 i = 0; i < rows; ++i) {
    WideStr line = buffer_get_line(&main_buffer->buffer, i);
    u32 anchor = 0;
    bool found_word = false;

    DA_APPEND(main_buffer->completion_lines, (Completions) {0});
    Completions *completions =
      main_buffer->completion_lines.items + main_buffer->completion_lines.len - 1;

    for (u32 j = 0; j < line.len; ++j) {
      if (is_part_of_word(line.ptr[j])) {
        if (!found_word) {
          anchor = j;
          found_word = true;
        }
      } else {
        if (found_word) {
          if (j - anchor >= MINIMAL_COMPLETION_PREFIX_LENGTH) {
            Completion new_completion = { {}, anchor, 0 };
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
            DA_APPEND(*completions, new_completion);
          }
          found_word = false;
        }
      }
    }

    if (found_word && line.len > anchor) {
      Completion new_completion = { {}, anchor, 0 };
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
      DA_APPEND(*completions, new_completion);
    }
  }
}

void editor_update_completions_before_action(Editor *editor, Action action, u32 param) {
  MainBuffer *main_buffer =
    editor->main_buffers.items + editor->current_main_buffer_index;
  u32 rows = buffer_get_rows(&main_buffer->buffer);

  switch (action) {
  case ActionInsertLine: {
    DA_INSERT(main_buffer->completion_lines,
              main_buffer->buffer.cursor_row + 1,
              (Completions) {0});
  } break;

  case ActionRemoveLineBeforeCursor: {
    if (main_buffer->buffer.cursor_row > 0) {
      Completions *completions0 =
        main_buffer->completion_lines.items + main_buffer->buffer.cursor_row - 1;
      Completions *completions1 =
        main_buffer->completion_lines.items + main_buffer->buffer.cursor_row;

      if (completions0->cap < completions0->len + completions1->len) {
        completions0->cap = completions0->len + completions1->len;
        completions0->items =
          realloc(completions0->items, completions0->cap * sizeof(*completions0->items));
      }

      memcpy(completions0->items + completions0->len,
             completions1->items,
             completions1->len * sizeof(*completions1->items));
      completions0->len += completions1->len;

      if (completions1->items)
        free(completions1->items);
      DA_REMOVE_AT(main_buffer->completion_lines, main_buffer->buffer.cursor_row);
    }
  } break;

  case ActionRemoveLineAtCursor: {
    if (main_buffer->buffer.cursor_row + 1 < rows) {
      Completions *completions0 =
        main_buffer->completion_lines.items + main_buffer->buffer.cursor_row;
      Completions *completions1 =
        main_buffer->completion_lines.items + main_buffer->buffer.cursor_row + 1;

      if (completions0->cap < completions0->len + completions1->len) {
        completions0->cap = completions0->len + completions1->len;
        completions0->items =
          realloc(completions0->items, completions0->cap * sizeof(*completions0->items));
      }

      memcpy(completions0->items + completions0->len,
             completions1->items,
             completions1->len * sizeof(*completions1->items));
      completions0->len += completions1->len;

      if (completions1->items)
        free(completions1->items);
      DA_REMOVE_AT(main_buffer->completion_lines, main_buffer->buffer.cursor_row + 1);
    }
  } break;

  case ActionInsert: {
    if (!is_part_of_word(param))
      break;

    Completions *completions =
      main_buffer->completion_lines.items + main_buffer->buffer.cursor_row;

    if (completions->len == 0) {
      WideStr word = buffer_get_word_at_cursor(&main_buffer->buffer);
      Completion new_completion = {
        {},
        main_buffer->buffer.cursor_col - word.len,
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
      DA_APPEND(*completions, new_completion);

      break;
    }

    bool prev_was_before_cursor = false;
    for (u32 i = 0; i < completions->len; ++i) {
      Completion *completion = completions->items + i;
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
          main_buffer->buffer.cursor_col,
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
        DA_INSERT(*completions, i, new_completion);
      }
    }
  } break;

  case ActionRemoveBeforeCursor: {
    Completions *completions =
      main_buffer->completion_lines.items + main_buffer->buffer.cursor_row;

    for (u32 i = 0; i < completions->len; ++i) {
      Completion *completion = completions->items + i;
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
          free(completion->wsb.items);
          DA_REMOVE_AT(*completions, i);
          --i;
        } else {
          completion->hash = wide_str_hash((WideStr) {
              completion->wsb.items,
              completion->wsb.len,
            });
        }
      }
    }
  } break;

  case ActionRemoveAtCursor: {
    Completions *completions =
      main_buffer->completion_lines.items + main_buffer->buffer.cursor_row;

      for (u32 i = 0; i < completions->len; ++i) {
        Completion *completion = completions->items + i;
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
            free(completions->items[i].wsb.items);
            DA_REMOVE_AT(*completions, i);
            --i;
          } else {
            completion->hash = wide_str_hash((WideStr) {
                completion->wsb.items,
                completion->wsb.len,
              });
          }
        }
      }
  } break;
  }
}

void editor_update_before_buffer_action(Editor *editor,
                                        BufferAction *action,
                                        bool is_redo) {
  MainBuffer *main_buffer =
    editor->main_buffers.items + editor->current_main_buffer_index;
  Buffer *buffer = editor_current_buffer(editor);
  u32 prev_cursor_row = buffer->cursor_row;
  u32 prev_cursor_col = buffer->cursor_col;

  buffer->cursor_row = action->cursor_row;
  buffer->cursor_col = action->cursor_col;

  if (is_redo) {
    switch (action->kind) {
    case BufferActionKindInsert: {
      if (editor->mode == JetModeEditor)
        highlighter_update_before_action(main_buffer, ActionInsert);
    } break;

    case BufferActionKindInsertLine: {
      editor_update_inline_errors_before_action(editor, ActionInsertLine);
      editor_update_completions_before_action(editor, ActionInsertLine, 0);
      if (editor->mode == JetModeEditor)
        highlighter_update_before_action(main_buffer, ActionInsertLine);
    } break;

    case BufferActionKindRemoveBeforeCursor: {
      for (u32 i = 0; i < action->data.len; ++i) {
        if (action->data.ptr[i] == U'\n') {
          editor_update_inline_errors_before_action(editor, ActionRemoveLineBeforeCursor);
          editor_update_completions_before_action(editor, ActionRemoveLineBeforeCursor, 0);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionRemoveLineBeforeCursor);
          --buffer->cursor_row;
          buffer->cursor_col = buffer_get_current_line(buffer).len;
        } else {
          editor_update_inline_errors_before_action(editor, ActionRemoveBeforeCursor);
          editor_update_completions_before_action(editor, ActionRemoveBeforeCursor, 1);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionRemoveBeforeCursor);
          --buffer->cursor_col;
        }
      }
    } break;

    case BufferActionKindRemoveAtCursor: {
      for (u32 i = 0; i < action->data.len; ++i) {
        if (action->data.ptr[i] == U'\n') {
          editor_update_inline_errors_before_action(editor, ActionRemoveLineAtCursor);
          editor_update_completions_before_action(editor, ActionRemoveLineAtCursor, 0);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionRemoveLineAtCursor);
        } else {
          editor_update_inline_errors_before_action(editor, ActionRemoveAtCursor);
          editor_update_completions_before_action(editor, ActionRemoveAtCursor, 1);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionRemoveAtCursor);
        }
      }
    } break;
    }
  } else {
    switch (action->kind) {
    case BufferActionKindInsert: {
      for (u32 i = 0; i < action->data.len; ++i) {
        editor_update_inline_errors_before_action(editor, ActionRemoveAtCursor);
        editor_update_completions_before_action(editor, ActionRemoveAtCursor, 1);
        if (editor->mode == JetModeEditor)
          highlighter_update_before_action(main_buffer, ActionRemoveAtCursor);
      }
    } break;

    case BufferActionKindInsertLine: {
      editor_update_inline_errors_before_action(editor, ActionRemoveLineAtCursor);
      editor_update_completions_before_action(editor, ActionRemoveLineAtCursor, 0);
      if (editor->mode == JetModeEditor)
        highlighter_update_before_action(main_buffer, ActionRemoveLineAtCursor);
    } break;

    case BufferActionKindRemoveBeforeCursor: {
      for (u32 i = 0; i < action->data.len; ++i) {
        if (action->data.ptr[i] == U'\n') {
          editor_update_inline_errors_before_action(editor, ActionInsertLine);
          editor_update_completions_before_action(editor, ActionInsertLine, 0);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionInsertLine);
          ++buffer->cursor_row;
        }
      }
    } break;

    case BufferActionKindRemoveAtCursor: {
      for (u32 i = 0; i < action->data.len; ++i) {
        if (action->data.ptr[i] == U'\n') {
          editor_update_inline_errors_before_action(editor, ActionInsertLine);
          editor_update_completions_before_action(editor, ActionInsertLine, 0);
          if (editor->mode == JetModeEditor)
            highlighter_update_before_action(main_buffer, ActionInsertLine);
        }
      }
    } break;
    }
  }

  buffer->cursor_row = prev_cursor_row;
  buffer->cursor_col = prev_cursor_col;
}
