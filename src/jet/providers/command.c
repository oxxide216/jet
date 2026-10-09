#include "../provider.h"
#include "../editor.h"
#include "../buffer.h"
#include "../common.h"
#include "../config.h"
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

static PaletteOptions get_opts(Editor *editor, WideStr content) {
  (void) editor;

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

  strs = wide_strs_select_prefixed(strs, content);

  PaletteOptions result;
  result.len = strs.len;
  result.cap = result.len;
  result.items = malloc(result.cap * sizeof(*result.items));
  for (u32 i = 0; i < strs.len; ++i) {
    result.items[i].name = strs.items[i];
    destructurize_color(&result.items[i].bg_r,
                        &result.items[i].bg_g,
                        &result.items[i].bg_b,
                        BG_COLOR);
    destructurize_color(&result.items[i].fg_r,
                        &result.items[i].fg_g,
                        &result.items[i].fg_b,
                        FG_COLOR);
  }
  if (strs.items)
    free(strs.items);

  return result;
}

static void free_opts(PaletteOptions options) {
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, PaletteOptions options, u32 index) {
  (void) editor;
  (void) options;

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
