#include "protocol.h"

bool decode_message(Message *message, u8 **buffer, u32 *len) {
  if (*len < sizeof(u8))
    return false;
  message->kind = (MessageKind) **buffer;
  *len -= sizeof(u8);
  *buffer += sizeof(u8);

  switch (message->kind) {
  case MessageKindEntry: {
    if (*len < sizeof(u32))
      return false;
    message->as.entry.file_path.len = *(u32 *) *buffer;
    *len += sizeof(u32);
    *buffer += sizeof(u32);

    if (*len < message->as.entry.file_path.len)
      return false;
    memcpy(message->as.entry.file_path.ptr, *buffer, message->as.entry.file_path.len);
    *len += message->as.entry.file_path.len;
    *buffer += message->as.entry.file_path.len;

    if (*len < sizeof(u32))
      return false;
    message->as.entry.row = *(u32 *) *buffer;
    *len += sizeof(u32);
    *buffer += sizeof(u32);

    if (*len < sizeof(u32))
      return false;
    message->as.entry.col = *(u32 *) *buffer;
    *len += sizeof(u32);
    *buffer += sizeof(u32);

    if (*len < sizeof(u8))
      return false;
    message->as.entry.kind = (EntryKind) **buffer;
    *len += sizeof(u8);
    *buffer += sizeof(u8);

    if (*len < sizeof(u32))
      return false;
    message->as.entry.message.len = *(u32 *) *buffer;
    *len += sizeof(u32);
    *buffer += sizeof(u32);

    if (*len < message->as.entry.message.len)
      return false;
    memcpy(message->as.entry.message.ptr, *buffer, message->as.entry.message.len);
    *len += message->as.entry.message.len;
    *buffer += message->as.entry.message.len;
  } break;

  case MessageKindRerun: break;
  }

  return true;
}

void encode_message(Buffer *buffer, Message *message) {
  DA_APPEND(*buffer, (u8) message->kind);

  switch (message->kind) {
  case MessageKindEntry: {
    for (u32 i = 0; i < 4; ++i)
      DA_APPEND(*buffer, ((u8 *) &message->as.entry.file_path.len)[i]);

    for (u32 i = 0; i < message->as.entry.file_path.len; ++i)
      DA_APPEND(*buffer, (u8) message->as.entry.file_path.ptr[i]);

    for (u32 i = 0; i < 4; ++i)
      DA_APPEND(*buffer, ((u8 *) &message->as.entry.row)[i]);

    for (u32 i = 0; i < 4; ++i)
      DA_APPEND(*buffer, ((u8 *) &message->as.entry.col)[i]);

    DA_APPEND(*buffer, (u8) message->as.entry.kind);

    for (u32 i = 0; i < 4; ++i)
      DA_APPEND(*buffer, ((u8 *) &message->as.entry.message.len)[i]);

    for (u32 i = 0; i < message->as.entry.message.len; ++i)
      DA_APPEND(*buffer, (u8) message->as.entry.message.ptr[i]);
  } break;

  case MessageKindRerun: break;
  }
}
