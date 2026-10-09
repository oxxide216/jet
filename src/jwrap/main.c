// TODO: Debounce

#include "shl/shl-defs.h"
#include "shl/shl-log.h"
#include "cns/cns.h"
#include "platform.h"
#include "../common/protocol.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

static CnsResult connected(CnsCtx *ctx, CnsConnection *connection);
static CnsResult data(CnsCtx *ctx, CnsConnection *connection, unsigned char *data, unsigned long data_len);
static void      disconnected(CnsCtx *ctx, CnsConnection *connection);

static char **global_argv;
static const char *socket_path;
static bool is_running = true;
static CnsConnection *server_connection = NULL;
static Child child;
static bool is_reading = true;
static MessageEntries entries = {0};

static CnsUnixConnectInfo connect_info = {
  .receive_timeout = 15,
  .connected_cb = connected,
  .data_cb = data,
  .disconnected_cb = disconnected,
};

static void print_usage(char *program_name) {
  printf("%s <command>\n", program_name);
}

static bool str_begins_with(Str str, Str prefix) {
  if (str.len < prefix.len)
    return false;

  str.len = prefix.len;
  return str_eq(str, prefix);
}

static void parse_entries(MessageEntries *entries, Stream *stream) {
  Str cwd = get_current_dir();

  u32 cursor = 0;
  while (cursor < stream->len) {
    while (cursor < stream->len) {
      u32 anchor = cursor;
      while (cursor < stream->len &&
             stream->items[cursor] != '\n' &&
             stream->items[cursor] != ':')
        ++cursor;

      if (cursor == stream->len || stream->items[cursor] != ':')
        break;

      Str file_path = { NULL, cursor - anchor };
      file_path.ptr = malloc((cwd.len + 1 + file_path.len) * sizeof(*file_path.ptr));
      memcpy(file_path.ptr, cwd.ptr, cwd.len);
      file_path.ptr[cwd.len] = '/';
      memcpy(file_path.ptr + cwd.len + 1, stream->items + anchor, file_path.len);
      file_path.len += cwd.len + 1;

      ++cursor;

      anchor = cursor;
      while (cursor < stream->len &&
             stream->items[cursor] != '\n' &&
             stream->items[cursor] != ':')
        ++cursor;

      if (cursor == stream->len || stream->items[cursor] != ':') {
        free(file_path.ptr);
        break;
      }

      Str row_str = { stream->items + anchor, cursor - anchor };
      u32 row = str_to_u32(row_str);
      if (row == 0) {
        free(file_path.ptr);
        break;
      }

      ++cursor;

      anchor = cursor;
      while (cursor < stream->len &&
             stream->items[cursor] != '\n' &&
             stream->items[cursor] != ':')
        ++cursor;

      if (cursor == stream->len || stream->items[cursor] != ':') {
        free(file_path.ptr);
        break;
      }

      Str col_str = { stream->items + anchor, cursor - anchor };
      u32 col = str_to_u32(col_str);
      if (col == 0) {
        free(file_path.ptr);
        break;
      }

      ++cursor;
      if (cursor < stream->len)
        ++cursor;

      anchor = cursor;
      while (cursor < stream->len &&
             stream->items[cursor] != '\n')
        ++cursor;

      if (cursor == anchor) {
        free(file_path.ptr);
        break;
      }

      Str message = { stream->items + anchor, cursor - anchor };

      EntryKind kind;
      if (str_begins_with(message, STR_LIT("error: "))) {
        kind = EntryKindError;
        anchor += STR_LIT("error: ").len;
      } else if (str_begins_with(message, STR_LIT("warning: "))) {
        kind = EntryKindWarn;
        anchor += STR_LIT("warning: ").len;
      } else if (str_begins_with(message, STR_LIT("note: "))) {
        kind = EntryKindInfo;
        anchor += STR_LIT("note: ").len;
      } else {
        free(file_path.ptr);
        break;
      }

      message.ptr = malloc(message.len * sizeof(*message.ptr));
      memcpy(message.ptr, stream->items + anchor, message.len);

      MessageEntry entry = { file_path, row, col, message, kind };
      DA_APPEND(*entries, entry);
    }

    while (cursor < stream->len &&
           stream->items[cursor] != '\n')
      ++cursor;

    if (cursor < stream->len)
      ++cursor;
  }

  memmove(stream->items, stream->items + cursor, stream->len - cursor);
  stream->len -= cursor;

  free(cwd.ptr);
}

static CnsResult connected(CnsCtx *ctx, CnsConnection *connection) {
  (void) ctx;

  server_connection = connection;

  INFO("Connected to server\n");

  return CnsResultOk;
}

static CnsResult data(CnsCtx *ctx, CnsConnection *connection, unsigned char *data, unsigned long data_len) {
  (void) ctx;
  (void) connection;

  u32 len = data_len;
  Message message;
  while (len > 0 && decode_message(&message, &data, &len)) {
    if (message.kind == MessageKindRerun) {
      child_wait(&child);
      child = run_command_capturing_output(global_argv);
      is_reading = true;
    }
  }

  return CnsResultOk;
}

static void disconnected(CnsCtx *ctx, CnsConnection *connection) {
  (void) ctx;
  (void) connection;

  INFO("Disconnected from server, shutting down\n");

  is_running = false;
}

i32 main(i32 argc, char **argv) {
  char *program_name = argv[0];

  --argc;
  ++argv;

  if (argc == 0) {
    print_usage(program_name);
    ERROR("Command was not provided\n");
    return 1;
  }

  global_argv = argv;

  CnsCtx *cns = cns_create();

  socket_path = get_first_jet_socket_path();
  if (!socket_path) {
    ERROR("No running instance of Jet was found\n");
    exit(1);
  }

  CnsError cns_error = cns_unix_connect(cns, socket_path, &connect_info);
  if (cns_error != CnsErrorOk) {
    ERROR("Failed to connect to Jet server: %s\n", cns_get_error_str(cns_error));
    exit(1);
  }

  Stream stream;
  stream.len = 0;
  stream.cap = 1024;
  stream.items = malloc(stream.cap * sizeof(*stream.items));
  u32 stream_cursor = 0;

  child = run_command_capturing_output(global_argv);
  is_reading = true;

  while (is_running) {
    if (is_reading) {
      is_reading = child_read(&child, &stream);
      printf("%.*s", (i32) (stream.len - stream_cursor), stream.items + stream_cursor);
      stream_cursor = stream.len;
    }

    if (server_connection) {
      parse_entries(&entries, &stream);
      ByteBuffer buffer = {0};

      for (u32 i = 0; i < entries.len; ++i) {
        Message message = { MessageKindEntry, { entries.items[i] } };
        encode_message(&buffer, &message);
      }

      cns_unix_send(server_connection, buffer.items, buffer.len);
      if (buffer.items)
        free(buffer.items);
      for (u32 i = 0; i < entries.len; ++i) {
        free(entries.items[i].file_path.ptr);
        free(entries.items[i].message.ptr);
      }
      entries.len = 0;
    }

    cns_step(cns, 10000);
  }

  cns_destroy(cns);

  return 0;
}
