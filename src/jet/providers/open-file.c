// TODO: Windows support
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../provider.h"
#include "../editor.h"

static WideStrs opts = {0};

static WideStrs get_opts(WideStr content) {
  DIR *dir = opendir(".");
  if (!dir)
    return (WideStrs) {0};

  WideStrs result = {0};

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    Str name = { entry->d_name, strlen(entry->d_name) };
    WideStr wide_name = str_to_wide_str(name);
    if (wide_str_begins_with(wide_name, content))
      DA_APPEND(result, wide_name);
    else
      free(wide_name.ptr);
  }

  closedir(dir);

  opts = result;
  return result;
}

static void free_opts(WideStrs options) {
  for (u32 i = 0; i < options.len; ++i)
    free(options.items[i].ptr);
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, u32 index) {
  struct stat stat_data;
  char *path = wide_str_to_cstr(opts.items[index]);
  if (lstat(path, &stat_data) < 0)
    return false;

  // Whether or not it is a regular file
  if (S_ISREG(stat_data.st_mode)) {
    editor->current_main_buffer_index = editor->main_buffers.len;
    MainBuffer main_buffer = { buffer_make(), path };
    buffer_read_file(&main_buffer.buffer, path);
    DA_APPEND(editor->main_buffers, main_buffer);

    return true;
  } else {
    chdir(path);
    editor->selected_option = 0;

    free(path);
    return false;
  }
}

Provider open_file_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
