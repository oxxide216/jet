#include "../provider.h"
#include "../editor.h"
#include "../buffer.h"
#include "shl/shl-log.h"

// Uncomment code below when adding first command
typedef enum {
  CommandsLen,
} Command;

static u32 *command_cstrs[CommandsLen] = {
};
static WideStr commands[ARRAY_LEN(command_cstrs)];
/* static bool initialized = false; */

/* static u32 wide_cstr_len(u32 *str) { */
/*   u32 len = 0; */

/*   while (*str++) */
/*     ++len; */

/*   return len; */
/* } */

static WideStrs get_opts(WideStr content) {
  // Commented out because of a warning
  /* if (!initialized) { */
  /*   for (u32 i = 0; i < ARRAY_LEN(command_cstrs); ++i) { */
  /*     commands[i].ptr = command_cstrs[i]; */
  /*     commands[i].len = wide_cstr_len(command_cstrs[i]); */
  /*   } */

  /*   initialized = true; */
  /* } */

  WideStrs strs = {
    commands,
    ARRAY_LEN(commands),
    ARRAY_LEN(commands),
  };

  return wide_strs_select_prefixed(strs, content);
}

static void free_opts(WideStrs options) {
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, u32 index) {
  (void) editor;

  switch ((Command) index) {
  case CommandsLen: break;
  }

  ERROR("Unexpected command index\n");
  exit(1);
}

Provider command_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
