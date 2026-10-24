#include <stdio.h>
#include <wctype.h>

#include "buffer.h"
#include "common.h"
#include "config.h"

Buffer buffer_make(void) {
  Buffer buffer = {0};
  buffer.is_recording_undos = true;
  DA_APPEND(buffer.lines, (Line) {0});
  return buffer;
}

void buffer_reset(Buffer *buffer) {
  for (u32 i = 1; i < buffer->lines.len; ++i)
    if (buffer->lines.items[i].items)
      free(buffer->lines.items[i].items);
  buffer->lines.len = 1;
  buffer->lines.items[0].len = 0;
  buffer->undos.len = 0;
  buffer->redos.len = 0;
  buffer->anchor_row = 0;
  buffer->anchor_col = 0;
  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
  buffer->is_dirty = false;
}

static void get_buffer_selection_bounds(Buffer *buffer,
                                        u32 *min_row, u32 *min_col,
                                        u32 *max_row, u32 *max_col) {
  if (buffer->cursor_row > buffer->anchor_row ||
      (buffer->cursor_row == buffer->anchor_row &&
       buffer->cursor_col >= buffer->anchor_col)) {
    *min_row = buffer->anchor_row;
    *min_col = buffer->anchor_col;
    *max_row = buffer->cursor_row;
    *max_col = buffer->cursor_col;
  } else {
    *min_row = buffer->cursor_row;
    *min_col = buffer->cursor_col;
    *max_row = buffer->anchor_row;
    *max_col = buffer->anchor_col;
  }
}

static void merge_line_down(Buffer *buffer, u32 index) {
  Line *line0 = buffer->lines.items + index;
  Line *line1 = buffer->lines.items + index + 1;

  if (line0->cap < line0->len + line1->len) {
    line0->cap = line0->len + line1->len;
    line0->items = realloc(line0->items, line0->cap * sizeof(*line0->items));
  }

  memcpy(line0->items + line0->len, line1->items, line1->len * sizeof(*line1->items));
  line0->len += line1->len;

  if (line1->items)
    free(line1->items);
  DA_REMOVE_AT(buffer->lines, index + 1);
}

static void buffer_add_undo(Buffer *buffer, BufferActionKind kind, WideStr data,
                            u32 cursor_row, u32 cursor_col) {
  if (!buffer->is_recording_undos) {
    if (data.ptr)
      free(data.ptr);
    return;
  }

  bool modified_prev = false;

  if (buffer->undos.len > 0) {
    BufferAction *prev = buffer->undos.items + buffer->undos.len - 1;
    if (prev->kind == kind) {
      switch (kind) {
      case BufferActionKindInsert: {
        if (cursor_row == prev->cursor_row &&
            cursor_col == prev->cursor_col + prev->data.len) {
          WideStr new_data;
          new_data.len = prev->data.len + data.len;
          new_data.ptr = malloc(new_data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr, prev->data.ptr, prev->data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr + prev->data.len, data.ptr, data.len * sizeof(*new_data.ptr));

          free(prev->data.ptr);
          free(data.ptr);

          prev->data = new_data;

          modified_prev = true;
        }
      } break;

      case BufferActionKindInsertLine: break;

      case BufferActionKindRemoveBeforeCursor: {
        if (cursor_row == prev->cursor_row &&
            cursor_col + prev->data.len == prev->cursor_col) {
          WideStr new_data;
          new_data.len = data.len + prev->data.len;
          new_data.ptr = malloc(new_data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr, data.ptr, data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr + data.len, prev->data.ptr, prev->data.len * sizeof(*new_data.ptr));

          free(prev->data.ptr);
          free(data.ptr);

          prev->data = new_data;

          modified_prev = true;
        }
      } break;

      case BufferActionKindRemoveAtCursor: {
        if (cursor_row == prev->cursor_row &&
            cursor_col == prev->cursor_col) {
          WideStr new_data;
          new_data.len = prev->data.len + data.len;
          new_data.ptr = malloc(new_data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr, prev->data.ptr, prev->data.len * sizeof(*new_data.ptr));
          memcpy(new_data.ptr + prev->data.len, data.ptr, data.len * sizeof(*new_data.ptr));

          free(prev->data.ptr);
          free(data.ptr);

          prev->data = new_data;

          modified_prev = true;
        }
      } break;
      }
    }
  }

  if (!modified_prev)
    DA_APPEND(buffer->undos, ((BufferAction) { kind, data, cursor_row, cursor_col }));

  for (u32 i = 0; i < buffer->redos.len; ++i)
    if (buffer->redos.items[i].kind == BufferActionKindInsert ||
        buffer->redos.items[i].kind == BufferActionKindRemoveBeforeCursor ||
        buffer->redos.items[i].kind == BufferActionKindRemoveAtCursor)
      free(buffer->redos.items[i].data.ptr);

  buffer->redos.len = 0;
}

static void buffer_remove_selection(Buffer *buffer) {
  u32 min_row, min_col, max_row, max_col;
  get_buffer_selection_bounds(buffer,
                              &min_row, &min_col,
                              &max_row, &max_col);

  if (min_row == max_row) {
    Line *line = buffer->lines.items + min_row;

    WideStr undo_data;
    undo_data.len = max_col - min_col;
    undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
    memcpy(undo_data.ptr,
           line->items + min_col,
           undo_data.len * sizeof(*undo_data.ptr));
    BufferActionKind undo_kind =
      min_col == buffer->anchor_col ?
      BufferActionKindRemoveBeforeCursor :
      BufferActionKindRemoveAtCursor;
    buffer_add_undo(buffer, undo_kind, undo_data,
                    buffer->cursor_row, buffer->cursor_col);

    memmove(line->items + min_col,
            line->items + max_col,
            (line->len - max_col) * sizeof(*line->items));
    line->len -= max_col - min_col;
  } else {
    WideStringBuilder undo_wsb = {0};

    Line *line0 = buffer->lines.items + min_row;

    WideStr line = { line0->items + min_col, line0->len - min_col };
    wsb_push_wstr(&undo_wsb, line);

    line0->len = min_col;

    for (u32 i = min_row + 1; i < max_row; ++i) {
      WideStr line = buffer_get_line(buffer, min_row + 1);
      wsb_push_wstr(&undo_wsb, line);
      if (buffer->lines.items[min_row + 1].items)
        free(buffer->lines.items[min_row + 1].items);
      DA_REMOVE_AT(buffer->lines, min_row + 1);
    }

    Line *line1 = buffer->lines.items + min_row + 1;

    line.len = max_col;
    line.ptr = line1->items;
    wsb_push_wstr(&undo_wsb, line);

    memmove(line1->items,
            line1->items + max_col,
            (line1->len - max_col) * sizeof(*line1->items));
    line1->len -= max_col;
    merge_line_down(buffer, min_row);

    WideStr undo_data = { undo_wsb.items, undo_wsb.len };
    BufferActionKind undo_kind =
      (min_col == buffer->anchor_col &&
       min_row == buffer->anchor_row) ?
      BufferActionKindRemoveBeforeCursor :
      BufferActionKindRemoveAtCursor;
    buffer_add_undo(buffer, undo_kind, undo_data,
                    buffer->cursor_row, buffer->cursor_col);
  }
  buffer->cursor_row = min_row;
  buffer->cursor_col = min_col;
  buffer->is_selecting = false;
  buffer->is_dirty = true;
}

void buffer_insert(Buffer *buffer, u32 _char) {
  if (buffer->is_selecting)
    buffer_remove_selection(buffer);

  WideStr undo_data;
  undo_data.len = 1;
  undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
  undo_data.ptr[0] = _char;
  buffer_add_undo(buffer, BufferActionKindInsert, undo_data,
                  buffer->cursor_row, buffer->cursor_col);

  Line *line = buffer->lines.items + buffer->cursor_row;
  DA_INSERT(*line, buffer->cursor_col, _char);
  ++buffer->cursor_col;
  buffer->desired_col = buffer->cursor_col;

  buffer->anchor_col = buffer->cursor_col;

  buffer->is_dirty = true;
}

void buffer_insert_new_line(Buffer *buffer) {
  if (buffer->is_selecting)
    buffer_remove_selection(buffer);

  WideStr undo_data = {0};
  buffer_add_undo(buffer, BufferActionKindInsertLine, undo_data,
                  buffer->cursor_row, buffer->cursor_col);

  Line *line = buffer->lines.items + buffer->cursor_row;
  Line new_line = {0};
  new_line.len = line->len - buffer->cursor_col;
  new_line.cap = new_line.len;
  if (new_line.cap > 0) {
    new_line.items = malloc(new_line.cap * sizeof(u32));
    memcpy(new_line.items, line->items + buffer->cursor_col, new_line.len * sizeof(u32));
  }

  line->len = buffer->cursor_col;

  ++buffer->cursor_row;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
  DA_INSERT(buffer->lines, buffer->cursor_row, new_line);

  buffer->anchor_col = buffer->cursor_col;
  buffer->anchor_row = buffer->cursor_row;

  buffer->is_dirty = true;
}

void buffer_remove_before_cursor(Buffer *buffer) {
  if (buffer->is_selecting) {
    buffer_remove_selection(buffer);
    return;
  }

  if (buffer->cursor_col > 0) {
    Line *line = buffer->lines.items + buffer->cursor_row;

    WideStr undo_data;
    undo_data.len = 1;
    undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
    undo_data.ptr[0] = line->items[buffer->cursor_col - 1];
    buffer_add_undo(buffer, BufferActionKindRemoveBeforeCursor, undo_data,
                    buffer->cursor_row, buffer->cursor_col);

    --buffer->cursor_col;
    buffer->desired_col = buffer->cursor_col;
    DA_REMOVE_AT(*line, buffer->cursor_col);

    buffer->is_dirty = true;
  } else if (buffer->cursor_row > 0) {
    WideStr undo_data;
    undo_data.len = 1;
    undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
    undo_data.ptr[0] = U'\n';
    buffer_add_undo(buffer, BufferActionKindRemoveBeforeCursor, undo_data,
                    buffer->cursor_row, buffer->cursor_col);

    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    buffer->cursor_col = line->len;
    buffer->desired_col = buffer->cursor_col;
    merge_line_down(buffer, buffer->cursor_row);

    buffer->is_dirty = true;
  }
}

void buffer_remove_at_cursor(Buffer *buffer) {
  if (buffer->is_selecting) {
    buffer_remove_selection(buffer);
    return;
  }

  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col < line->len) {
    WideStr undo_data;
    undo_data.len = 1;
    undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
    undo_data.ptr[0] = line->items[buffer->cursor_col];
    buffer_add_undo(buffer, BufferActionKindRemoveAtCursor, undo_data,
                    buffer->cursor_row, buffer->cursor_col);

    DA_REMOVE_AT(*line, buffer->cursor_col);

    buffer->is_dirty = true;
  } else if (buffer->cursor_row + 1 < buffer->lines.len) {
    WideStr undo_data;
    undo_data.len = 1;
    undo_data.ptr = malloc(undo_data.len * sizeof(*undo_data.ptr));
    undo_data.ptr[0] = U'\n';
    buffer_add_undo(buffer, BufferActionKindRemoveAtCursor, undo_data,
                    buffer->cursor_row, buffer->cursor_col);

    merge_line_down(buffer, buffer->cursor_row);

    buffer->is_dirty = true;
  }
}

void buffer_move_left(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  } else if (buffer->is_selecting && !is_selecting) {
    buffer->is_selecting = is_selecting;
    return;
  }

  buffer->is_selecting = is_selecting;

  if (buffer->cursor_col > 0) {
    --buffer->cursor_col;
  } else if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    buffer->cursor_col = line->len;
  }
  buffer->desired_col = buffer->cursor_col;
}

void buffer_move_right(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  } else if (buffer->is_selecting && !is_selecting) {
    buffer->is_selecting = is_selecting;
    return;
  }

  buffer->is_selecting = is_selecting;

  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col < line->len) {
    ++buffer->cursor_col;
  } else if (buffer->cursor_row + 1 < buffer->lines.len) {
    ++buffer->cursor_row;
    buffer->cursor_col = 0;
  }
}

// This is black magic
void buffer_move_down(Buffer *buffer, bool is_selecting, u32 max_visual_line_len) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col + max_visual_line_len < line->len) {
    u32 rem = buffer->cursor_col % max_visual_line_len;
    if (rem < ARRAY_LEN(LINE_WRAP_MARKER))
      buffer->cursor_col += max_visual_line_len - rem - 1;
    else
      buffer->cursor_col += max_visual_line_len - ARRAY_LEN(LINE_WRAP_MARKER);
  } else if (buffer->cursor_col + max_visual_line_len + 1 <
             line->len + ARRAY_LEN(LINE_WRAP_MARKER)) {
    buffer->cursor_col += max_visual_line_len - ARRAY_LEN(LINE_WRAP_MARKER);
  } else if (buffer->cursor_row + 1 < buffer->lines.len) {
    ++buffer->cursor_row;

    buffer->cursor_col = buffer->desired_col;
    if (buffer->cursor_col + 1 >= max_visual_line_len) {
      bool flag = buffer->cursor_col + 1 == max_visual_line_len;
      if (flag)
        ++buffer->cursor_col;
      buffer->cursor_col %= max_visual_line_len;
      buffer->cursor_col += ARRAY_LEN(LINE_WRAP_MARKER) - flag;
    }

    Line *line = buffer->lines.items + buffer->cursor_row;
    if (buffer->cursor_col > line->len)
      buffer->cursor_col = line->len;
  }
}

// This is black magic too
void buffer_move_up(Buffer *buffer, bool is_selecting, u32 max_visual_line_len) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  if (buffer->cursor_col > max_visual_line_len - ARRAY_LEN(LINE_WRAP_MARKER)) {
    buffer->cursor_col -= max_visual_line_len - ARRAY_LEN(LINE_WRAP_MARKER);
  } else if (buffer->cursor_row > 0) {
    --buffer->cursor_row;

    Line *line = buffer->lines.items + buffer->cursor_row;

    if (line->len >= max_visual_line_len)
      buffer->cursor_col =
        line->len -
        (line->len - max_visual_line_len) %
        (max_visual_line_len - ARRAY_LEN(LINE_WRAP_MARKER)) -
        1;
    else
      buffer->cursor_col = 0;
    buffer->cursor_col += buffer->desired_col;

    if (buffer->cursor_col > line->len)
      buffer->cursor_col = line->len;
  }
}

void buffer_remove_word_before_cursor(Buffer *buffer) {
  if (buffer->is_selecting) {
    buffer_remove_selection(buffer);
    return;
  }

  if (buffer->cursor_col == 0) {
    buffer_remove_before_cursor(buffer);
    return;
  }

  WideStringBuilder undo_wsb = {0};
  u32 prev_cursor_col = buffer->cursor_col;

  bool found_word = false;
  Line *line = buffer->lines.items + buffer->cursor_row;
  while (buffer->cursor_col > 0 &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col - 1]))) {
    if (iswalnum(line->items[buffer->cursor_col - 1]))
      found_word = true;
    --buffer->cursor_col;
    buffer->desired_col = buffer->cursor_col;
    DA_INSERT(undo_wsb, 0, line->items[buffer->cursor_col]);
    DA_REMOVE_AT(*line, buffer->cursor_col);

    buffer->is_dirty = true;
  }

  WideStr undo_data = { undo_wsb.items, undo_wsb.len };
  buffer_add_undo(buffer, BufferActionKindRemoveBeforeCursor, undo_data,
                  buffer->cursor_row, prev_cursor_col);
}

void buffer_remove_word_at_cursor(Buffer *buffer) {
  if (buffer->is_selecting) {
    buffer_remove_selection(buffer);
    return;
  }

  Line *line = buffer->lines.items + buffer->cursor_row;

  if (buffer->cursor_col == line->len) {
    buffer_remove_at_cursor(buffer);
    return;
  }

  WideStringBuilder undo_wsb = {0};

  bool found_word = false;
  while (buffer->cursor_col < line->len &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col]))) {
    if (iswalnum(line->items[buffer->cursor_col]))
      found_word = true;
    DA_APPEND(undo_wsb, line->items[buffer->cursor_col]);
    DA_REMOVE_AT(*line, buffer->cursor_col);

    buffer->is_dirty = true;
  }

  WideStr undo_data = { undo_wsb.items, undo_wsb.len };
  buffer_add_undo(buffer, BufferActionKindRemoveAtCursor, undo_data,
                  buffer->cursor_row, buffer->cursor_col);
}

void buffer_move_left_word(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  bool found_word = false;

  if (buffer->cursor_col == 0)
    buffer_move_left(buffer, is_selecting);

  Line *line = buffer->lines.items + buffer->cursor_row;
  while (buffer->cursor_col > 0 &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col - 1]))) {
    if (iswalnum(line->items[buffer->cursor_col - 1]))
      found_word = true;
    --buffer->cursor_col;
    buffer->desired_col = buffer->cursor_col;
  }
}

void buffer_move_right_word(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  Line *line = buffer->lines.items + buffer->cursor_row;
  bool found_word = false;

  if (buffer->cursor_col == line->len)
    buffer_move_right(buffer, is_selecting);

  line = buffer->lines.items + buffer->cursor_row;
  while (buffer->cursor_col < line->len &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col]))) {
    if (iswalnum(line->items[buffer->cursor_col]))
      found_word = true;
    ++buffer->cursor_col;
    buffer->desired_col = buffer->cursor_col;
  }
}

void buffer_move_down_paragraph(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  bool found_paragraph = false;

  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;

  while (buffer->cursor_row + 1 < buffer->lines.len &&
         (!found_paragraph ||
          buffer->lines.items[buffer->cursor_row].len > 0)) {
    if (buffer->lines.items[buffer->cursor_row].len > 0)
      found_paragraph = true;
    ++buffer->cursor_row;
  }
}

void buffer_move_up_paragraph(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  bool found_paragraph = false;

  buffer->cursor_col = 0;

  while (buffer->cursor_row > 0 &&
         (!found_paragraph ||
          buffer->lines.items[buffer->cursor_row].len > 0)) {
    if (buffer->lines.items[buffer->cursor_row].len > 0)
      found_paragraph = true;
    --buffer->cursor_row;
  }
}

void buffer_goto_line_begin(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_line_end(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  Line *line = buffer->lines.items + buffer->cursor_row;
  buffer->cursor_col = line->len;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_buffer_begin(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_buffer_end(Buffer *buffer, bool is_selecting) {
  if (!buffer->is_selecting && is_selecting) {
    buffer->anchor_row = buffer->cursor_row;
    buffer->anchor_col = buffer->cursor_col;
  }

  buffer->is_selecting = is_selecting;

  buffer->cursor_row = buffer->lines.len - 1;
  Line *line = buffer->lines.items + buffer->cursor_row;
  buffer->cursor_col = line->len;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_undo(Buffer *buffer) {
  if (buffer->undos.len == 0)
    return;

  buffer->is_selecting = false;
  buffer->is_dirty = true;
  buffer->is_recording_undos = false;

  BufferAction action = buffer->undos.items[--buffer->undos.len];
  switch (action.kind) {
  case BufferActionKindInsert: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = 0; i < action.data.len; ++i)
      buffer_remove_at_cursor(buffer);
  } break;

  case BufferActionKindInsertLine: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    buffer_remove_at_cursor(buffer);
  } break;

  case BufferActionKindRemoveBeforeCursor: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = action.data.len; i > 0; --i)
      buffer_move_left(buffer, false);

    for (u32 i = 0; i < action.data.len; ++i) {
      if (action.data.ptr[i] == U'\n')
        buffer_insert_new_line(buffer);
      else
        buffer_insert(buffer, action.data.ptr[i]);
    }
  } break;

  case BufferActionKindRemoveAtCursor: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = 0; i < action.data.len; ++i) {
      if (action.data.ptr[i] == U'\n')
        buffer_insert_new_line(buffer);
      else
        buffer_insert(buffer, action.data.ptr[i]);
    }

    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;
  } break;
  }

  buffer->is_recording_undos = true;

  DA_APPEND(buffer->redos, action);
}

void buffer_redo(Buffer *buffer) {
  if (buffer->redos.len == 0)
    return;

  buffer->is_selecting = false;
  buffer->is_dirty = true;
  buffer->is_recording_undos = false;

  BufferAction action = buffer->redos.items[--buffer->redos.len];
  switch (action.kind) {
  case BufferActionKindInsert: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = 0; i < action.data.len; ++i) {
      if (action.data.ptr[i] == U'\n')
        buffer_insert_new_line(buffer);
      else
        buffer_insert(buffer, action.data.ptr[i]);
    }
  } break;

  case BufferActionKindInsertLine: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    buffer_insert_new_line(buffer);
  } break;

  case BufferActionKindRemoveBeforeCursor: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = 0; i < action.data.len; ++i)
      buffer_remove_before_cursor(buffer);
  } break;

  case BufferActionKindRemoveAtCursor: {
    buffer->cursor_row = action.cursor_row;
    buffer->cursor_col = action.cursor_col;

    for (u32 i = 0; i < action.data.len; ++i)
      buffer_remove_at_cursor(buffer);
  } break;
  }

  buffer->is_recording_undos = true;

  DA_APPEND(buffer->undos, action);
}

void buffer_delete(Buffer *buffer) {
  for (u32 i = 0; i < buffer->lines.len; ++i) {
    Line *line = buffer->lines.items + i;
    if (line->items)
      free(line->items);
  }
  if (buffer->lines.items)
    free(buffer->lines.items);

  for (u32 i = 0; i < buffer->undos.len; ++i)
    if (buffer->undos.items[i].kind == BufferActionKindInsert ||
        buffer->undos.items[i].kind == BufferActionKindRemoveBeforeCursor ||
        buffer->undos.items[i].kind == BufferActionKindRemoveAtCursor)
      free(buffer->undos.items[i].data.ptr);
  if (buffer->undos.items)
    free(buffer->undos.items);

  for (u32 i = 0; i < buffer->redos.len; ++i)
    if (buffer->redos.items[i].kind == BufferActionKindInsert ||
        buffer->redos.items[i].kind == BufferActionKindRemoveBeforeCursor ||
        buffer->redos.items[i].kind == BufferActionKindRemoveAtCursor)
      free(buffer->redos.items[i].data.ptr);
  if (buffer->redos.items)
    free(buffer->redos.items);
}

void buffer_read_file(Buffer *buffer, char *path) {
  FILE *file = fopen(path, "r");
  if (!file)
    return;

  buffer->is_recording_undos = false;

  u32 wide_char;
  while ((wide_char = read_wide_char(file)) != (u32) -1) {
    if (wide_char == U'\n')
      buffer_insert_new_line(buffer);
    else
      buffer_insert(buffer, wide_char);
  }

  fclose(file);

  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
  buffer->is_selecting = false;
  buffer->is_dirty = false;
  buffer->is_recording_undos = true;
}

void buffer_write_file(Buffer *buffer, char *path) {
  FILE *file = fopen(path, "w");
  if (!file)
    return;

  for (u32 i = 0; i < buffer->lines.len; ++i) {
    Line *line = buffer->lines.items + i;
    if (i > 0)
      write_wide_char(U'\n', file);
    for (u32 j = 0; j < line->len; ++j)
      write_wide_char(line->items[j], file);
  }

  fclose(file);

  buffer->is_dirty = false;
}

WideStr buffer_get_prev_line(Buffer *buffer) {
  if (buffer->cursor_row == 0)
    return (WideStr) {0};

  Line *line = buffer->lines.items + buffer->cursor_row - 1;
  return (WideStr) {
    line->items,
    line->len,
  };
}

WideStr buffer_get_current_line(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  return (WideStr) {
    line->items,
    line->len,
  };
}

WideStr buffer_get_next_line(Buffer *buffer) {
  if (buffer->cursor_row + 1 == buffer->lines.len)
    return (WideStr) {0};

  Line *line = buffer->lines.items + buffer->cursor_row + 1;
  return (WideStr) {
    line->items,
    line->len,
  };
}

WideStr buffer_get_line(Buffer *buffer, u32 index) {
  if (index >= buffer->lines.len)
    return (WideStr) {0};

  Line *line = buffer->lines.items + index;
  return (WideStr) {
    line->items,
    line->len,
  };
}

u32 buffer_get_rows(Buffer *buffer) {
  return buffer->lines.len;
}

WideStr buffer_get_word_at_cursor(Buffer *buffer) {
  WideStr result = buffer_get_current_line(buffer);
  u32 line_len = result.len;
  u32 offset = buffer->cursor_col;
  result.ptr += offset;
  result.len = 0;

  while (result.len < buffer->cursor_col &&
         is_part_of_word(result.ptr[-1])) {
    --offset;
    --result.ptr;
    ++result.len;
  }

  while (offset + result.len < line_len &&
         is_part_of_word(result.ptr[result.len]))
    ++result.len;

  return result;
}

u32 buffer_get_chars_amount_for_remove_word_before_cursor(Buffer *buffer) {
  bool found_word = false;

  if (buffer->cursor_col == 0)
    return 1;

  u32 result = 0;

  u32 col = buffer->cursor_col;
  Line *line = buffer->lines.items + buffer->cursor_row;
  while (col > 0 &&
         (!found_word ||
          iswalnum(line->items[col - 1]))) {
    if (iswalnum(line->items[col - 1]))
      found_word = true;
    --col;
    ++result;
  }

  return result;
}

u32 buffer_get_chars_amount_for_remove_word_at_cursor(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;

  if (buffer->cursor_col == line->len)
    return 1;

  u32 result = 0;

  u32 col = buffer->cursor_col;
  bool found_word = false;
  while (col < line->len &&
         (!found_word ||
          iswalnum(line->items[col]))) {
    if (iswalnum(line->items[col]))
      found_word = true;
    ++col;
    ++result;
  }

  return result;
}

Str  buffer_get_selection_str(Buffer *buffer) {
  if (!buffer->is_selecting)
    return (Str) {0};

  u32 min_row, min_col, max_row, max_col;
  get_buffer_selection_bounds(buffer,
                              &min_row, &min_col,
                              &max_row, &max_col);

  StringBuilder sb = {0};

  for (u32 i = min_row; i <= max_row; ++i) {
    if (i > min_row)
      sb_push_char(&sb, '\n');
    Line *line = buffer->lines.items + i;
    u32 min = i == min_row ? min_col : 0;
    u32 max = i == max_row ? max_col : line->len;
    for (u32 j = min; j < max; ++j)
      for (u32 k = 0; k < 4; ++k)
        if (((char *) (line->items + j))[k])
          sb_push_char(&sb, ((char *) (line->items + j))[k]);
  }

  return sb_to_str(sb);
}

void buffer_set_selection_str(Buffer *buffer, Str str) {
  if (buffer->is_selecting) {
    buffer_remove_selection(buffer);
  }

  while (str.len > 0) {
    u32 _char = get_wide_char(&str);
    if (_char == U'\n')
      buffer_insert_new_line(buffer);
    else
      buffer_insert(buffer, _char);
  }
}
