#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

typedef enum {
  MessageKindEntry = 0,
  MessageKindRerun,
} MessageKind;

typedef enum {
  EntryKindError = 0,
  EntryKindWarn,
  EntryKindInfo,
} EntryKind;

typedef struct {
  Str       file_path;
  u32       row, col;
  Str       message;
  EntryKind kind;
} MessageEntry;

typedef struct {
  MessageKind kind;
  union {
    MessageEntry entry;
  } as;
} Message;

typedef Da(u8) Buffer;

bool decode_message(Message *message, u8 **buffer, u32 *len);
void encode_message(Buffer *buffer, Message *message);

#endif // PROTOCOL_H
