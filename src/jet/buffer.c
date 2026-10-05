#include <stdio.h>
#include <wctype.h>

#include "buffer.h"
#include "common.h"

Buffer buffer_make(void) {
  Buffer buffer = {0};
  DA_APPEND(buffer.lines, (Line) {0});
  return buffer;
}

void buffer_reset(Buffer *buffer) {
  for (u32 i = 1; i < buffer->lines.len; ++i)
    if (buffer->lines.items[i].items)
      free(buffer->lines.items[i].items);
  buffer->lines.len = 1;
  buffer->lines.items[0].len = 0;
  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_insert(Buffer *buffer, u32 _char) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  DA_INSERT(*line, buffer->cursor_col, _char);
  ++buffer->cursor_col;
  buffer->desired_col = buffer->cursor_col;
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
  buffer->desired_col = buffer->cursor_col;
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
    buffer->desired_col = buffer->cursor_col;
    DA_REMOVE_AT(*line, buffer->cursor_col);
  } else if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    buffer->cursor_col = line->len;
    buffer->desired_col = buffer->cursor_col;
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
  if (buffer->cursor_col > 0) {
    --buffer->cursor_col;
  } else if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    Line *line = buffer->lines.items + buffer->cursor_row;
    buffer->cursor_col = line->len;
  }
  buffer->desired_col = buffer->cursor_col;
}

void buffer_move_right(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->cursor_col < line->len) {
    ++buffer->cursor_col;
  } else if (buffer->cursor_row + 1 < buffer->lines.len) {
    ++buffer->cursor_row;
    buffer->cursor_col = 0;
  }
  buffer->desired_col = buffer->cursor_col;
}

static void ensure_safe_cursor_col(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  if (buffer->desired_col > line->len)
    buffer->cursor_col = line->len;
  else
    buffer->cursor_col = buffer->desired_col;
}

void buffer_move_down(Buffer *buffer) {
  if (buffer->cursor_row + 1 < buffer->lines.len) {
    ++buffer->cursor_row;
    ensure_safe_cursor_col(buffer);
  }
}

void buffer_move_up(Buffer *buffer) {
  if (buffer->cursor_row > 0) {
    --buffer->cursor_row;
    ensure_safe_cursor_col(buffer);
  }
}

void buffer_remove_word_before_cursor(Buffer *buffer) {
  bool found_word = false;

  if (buffer->cursor_col == 0)
    buffer_remove_before_cursor(buffer);

  Line *line = buffer->lines.items + buffer->cursor_row;
  while (buffer->cursor_col > 0 &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col - 1]))) {
    if (iswalnum(line->items[buffer->cursor_col - 1]))
      found_word = true;
    --buffer->cursor_col;
    buffer->desired_col = buffer->cursor_col;
    DA_REMOVE_AT(*line, buffer->cursor_col);
  }
}

void buffer_remove_word_at_cursor(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  bool found_word = false;

  if (buffer->cursor_col == line->len)
    buffer_remove_at_cursor(buffer);

  while (buffer->cursor_col < line->len &&
         (!found_word ||
          iswalnum(line->items[buffer->cursor_col]))) {
    if (iswalnum(line->items[buffer->cursor_col]))
      found_word = true;
    DA_REMOVE_AT(*line, buffer->cursor_col);
  }
}

void buffer_move_left_word(Buffer *buffer) {
  bool found_word = false;

  if (buffer->cursor_col == 0)
    buffer_move_left(buffer);

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

void buffer_move_right_word(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  bool found_word = false;

  if (buffer->cursor_col == line->len)
    buffer_move_right(buffer);

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

void buffer_move_down_paragraph(Buffer *buffer) {
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

  ensure_safe_cursor_col(buffer);
}

void buffer_move_up_paragraph(Buffer *buffer) {
  bool found_paragraph = false;

  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;

  while (buffer->cursor_row > 0 &&
         (!found_paragraph ||
          buffer->lines.items[buffer->cursor_row - 1].len > 0)) {
    if (buffer->lines.items[buffer->cursor_row - 1].len > 0)
      found_paragraph = true;
    --buffer->cursor_row;
  }

  ensure_safe_cursor_col(buffer);
}

void buffer_goto_line_begin(Buffer *buffer) {
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_line_end(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  buffer->cursor_col = line->len;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_buffer_begin(Buffer *buffer) {
  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_goto_buffer_end(Buffer *buffer) {
  buffer->cursor_row = buffer->lines.len - 1;
  Line *line = buffer->lines.items + buffer->cursor_row;
  buffer->cursor_col = line->len;
  buffer->desired_col = buffer->cursor_col;
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

void buffer_delete_line(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  line->len = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_read_file(Buffer *buffer, char *path) {
  FILE *file = fopen(path, "r");
  if (!file)
    return;

  u32 wide_char;
  while ((wide_char = get_wide_char(file)) != (u32) -1) {
    if (wide_char == U'\n')
      buffer_insert_new_line(buffer);
    else
      buffer_insert(buffer, wide_char);
  }

  fclose(file);

  buffer->cursor_row = 0;
  buffer->cursor_col = 0;
  buffer->desired_col = buffer->cursor_col;
}

void buffer_write_file(Buffer *buffer, char *path) {
  FILE *file = fopen(path, "w");
  if (!file)
    return;

  for (u32 i = 0; i < buffer->lines.len; ++i) {
    Line *line = buffer->lines.items + i;
    if (i > 0)
      put_wide_char('\n', file);
    for (u32 j = 0; j < line->len; ++j)
      put_wide_char(line->items[j], file);
  }

  fclose(file);
}

WideStr buffer_get_current_line(Buffer *buffer) {
  Line *line = buffer->lines.items + buffer->cursor_row;
  return (WideStr) {
    line->items,
    line->len,
  };
}
