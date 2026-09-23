#ifndef BUFFER_H
#define BUFFER_H

#include "shl/shl-defs.h"

typedef Da(u32) Line;
typedef Da(Line) Lines;

typedef struct {
  Lines lines;
  u32   cursor_row;
  u32   cursor_col;
  u32   desired_col;
} Buffer;

Buffer buffer_make(void);
void   buffer_insert(Buffer *buffer, u32 _char);
void   buffer_insert_new_line(Buffer *buffer);
void   buffer_remove_before_cursor(Buffer *buffer);
void   buffer_remove_at_cursor(Buffer *buffer);
void   buffer_move_left(Buffer *buffer);
void   buffer_move_right(Buffer *buffer);
void   buffer_move_down(Buffer *buffer);
void   buffer_move_up(Buffer *buffer);
void   buffer_move_left_word(Buffer *buffer);
void   buffer_move_right_word(Buffer *buffer);
void   buffer_move_down_paragraph(Buffer *buffer);
void   buffer_move_up_paragraph(Buffer *buffer);
void   buffer_delete(Buffer *buffer);

#endif // BUFFER_H
