#include <stdio.h>

#include "buffer.h"

Buffer buffer_make(void) {
  Buffer buffer = {0};
  DA_APPEND(buffer.lines, (Line) {0});
  return buffer;
}

void buffer_insert(Buffer *buffer, u32 _char) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  DA_INSERT(*line, buffer->cursor_col, _char);
  ++buffer->cursor_col;
}

void buffer_insert_new_line(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  Line new_line = {0};
  new_line.len = line->len - buffer->cursor_col;
  new_line.cap = new_line.len;
  new_line.items = malloc(new_line.cap * sizeof(u32));
  memcpy(new_line.items, line->items + buffer->cursor_col, new_line.len * sizeof(u32));

  line->len = buffer->cursor_col;

  ++buffer->cursor_row;
  buffer->cursor_col = 0;
  DA_INSERT(buffer->lines, buffer->cursor_row, new_line);
}

static void merge_line_down(Buffer *buffer, u32 index) {
  Line *line0 = buffer->lines.items + index;
  Line *line1 = buffer->lines.items + index + 1;

  if (line0->cap < line0->len + line1->len) {
    line0->cap = line0->len + line1->len;
    line0->items = realloc(line0->items, line0->cap * sizeof(u32));
  }

  memcpy(line0->items + line0->len, line1->items, line1->len * sizeof(u32));
  line0->len += line1->len;

  if (line1->items)
    free(line1->items);
  DA_REMOVE_AT(buffer->lines, index + 1);
}

void buffer_remove_before_cursor(Buffer *buffer) {
  if (buffer->cursor_col > 0) {
    Line *line = buffer->lines.items + buffer->cursor_row;
    --buffer->cursor_col;
    DA_REMOVE_AT(*line, buffer->cursor_col);
  } else if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    buffer->cursor_col = line->len;
    merge_line_down(buffer, buffer->cursor_row);
  }
}

void buffer_remove_at_cursor(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col < line->len)
    DA_REMOVE_AT(*line, buffer->cursor_col);
  else if (buffer->cursor_row + 1 < buffer->lines.len)
    merge_line_down(buffer, buffer->cursor_row);
}

void buffer_move_left(Buffer *buffer) {
  if (buffer->cursor_col > 0)
    --buffer->cursor_col;
}

void buffer_move_right(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col < line->len)
    ++buffer->cursor_col;
}

void buffer_move_down(Buffer *buffer) {
  if (buffer->cursor_row + 1 < buffer->lines.len) {
    ++buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    if (buffer->cursor_col > line->len)
      buffer->cursor_col = line->len;
  }
}

void buffer_move_up(Buffer *buffer) {
  if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    if (buffer->cursor_col > line->len)
      buffer->cursor_col = line->len;
  }
}

void buffer_delete(Buffer *buffer) {
  for (u32 i = 0; i < buffer->lines.len; ++i) {
    Line *line = buffer->lines.items + i;
    if (line->items)
      free(line->items);
  }
  if (buffer->lines.items)
    free(buffer->lines.items);
}
