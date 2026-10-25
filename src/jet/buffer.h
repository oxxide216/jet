#ifndef BUFFER_H
#define BUFFER_H

#include "shl/shl-defs.h"
#include "common.h"

typedef Da(u32) Line;
typedef Da(Line) Lines;

typedef enum {
  BufferActionKindInsert = 0,
  BufferActionKindInsertLine,
  BufferActionKindRemoveBeforeCursor,
  BufferActionKindRemoveAtCursor,
} BufferActionKind;

typedef struct {
  BufferActionKind kind;
  WideStr          data;
  u32              cursor_row;
  u32              cursor_col;
} BufferAction;

typedef Da(BufferAction) BufferActions;

typedef struct {
  Lines         lines;
  BufferActions undos;
  BufferActions redos;
  u32           anchor_row;
  u32           anchor_col;
  u32           cursor_row;
  u32           cursor_col;
  u32           desired_col;
  bool          is_selecting;
  bool          is_dirty;
  bool          is_recording_undos;
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
void   buffer_undo(Buffer *buffer);
void   buffer_redo(Buffer *buffer);
void   buffer_delete(Buffer *buffer);

void buffer_read_file(Buffer *buffer, char *path);
void buffer_write_file(Buffer *buffer, char *path);

WideStr buffer_get_prev_line(Buffer *buffer);
WideStr buffer_get_current_line(Buffer *buffer);
WideStr buffer_get_next_line(Buffer *buffer);
WideStr buffer_get_line(Buffer *buffer, u32 index);

u32 buffer_get_rows(Buffer *buffer);

WideStr buffer_get_word_at_cursor(Buffer *buffer);

u32 buffer_get_chars_amount_for_remove_word_before_cursor(Buffer *buffer);
u32 buffer_get_chars_amount_for_remove_word_at_cursor(Buffer *buffer);

Str buffer_get_selection_str(Buffer *buffer);

#endif // BUFFER_H
