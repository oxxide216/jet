#ifndef BUFFER_H
#define BUFFER_H

#include "shl/shl-defs.h"
#include "common.h"

typedef Da(u32) Line;
typedef Da(Line) Lines;

// TODO: maybe try linked list of arrays instead of an array of arrays?
typedef struct {
  Lines lines;
  u32   anchor_row;
  u32   anchor_col;
  u32   cursor_row;
  u32   cursor_col;
  u32   desired_col;
  bool  is_selecting;
  bool  is_dirty;
} Buffer;

Buffer buffer_make(void);
void   buffer_reset(Buffer *buffer);
void   buffer_insert(Buffer *buffer, u32 _char);
void   buffer_insert_new_line(Buffer *buffer);
void   buffer_remove_before_cursor(Buffer *buffer);
void   buffer_remove_at_cursor(Buffer *buffer);
void   buffer_move_left(Buffer *buffer, bool is_selecting);
void   buffer_move_right(Buffer *buffer, bool is_selecting);
void   buffer_move_down(Buffer *buffer, bool is_selecting, u32 max_visual_line_len);
void   buffer_move_up(Buffer *buffer, bool is_selecting, u32 max_visual_line_len);
void   buffer_remove_word_before_cursor(Buffer *buffer);
void   buffer_remove_word_at_cursor(Buffer *buffer);
void   buffer_move_left_word(Buffer *buffer, bool is_selecting);
void   buffer_move_right_word(Buffer *buffer, bool is_selecting);
void   buffer_move_down_paragraph(Buffer *buffer, bool is_selecting);
void   buffer_move_up_paragraph(Buffer *buffer, bool is_selecting);
void   buffer_goto_line_begin(Buffer *buffer, bool is_selecting);
void   buffer_goto_line_end(Buffer *buffer, bool is_selecting);
void   buffer_goto_buffer_begin(Buffer *buffer, bool is_selecting);
void   buffer_goto_buffer_end(Buffer *buffer, bool is_selecting);
void   buffer_remove_line(Buffer *buffer);
void   buffer_delete(Buffer *buffer);

void buffer_read_file(Buffer *buffer, char *path);
void buffer_write_file(Buffer *buffer, char *path);

WideStr buffer_get_prev_line(Buffer *buffer);
WideStr buffer_get_current_line(Buffer *buffer);
WideStr buffer_get_next_line(Buffer *buffer);

#endif // BUFFER_H
