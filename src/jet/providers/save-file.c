// TODO: Windows support
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../provider.h"
#include "../editor.h"
#include "../config.h"

static WideStrs opts = {0};

static WideStrs get_opts(WideStr content) {
  DIR *dir = opendir(".");
  if (!dir)
    return (WideStrs) {0};

  WideStrs result = {0};

  WideStr wide_name = { NEW_FILE_NAME, ARRAY_LEN(NEW_FILE_NAME) - 1 };
  DA_APPEND(result, wide_name);

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
  for (u32 i = 1; i < options.len; ++i)
    free(options.items[i].ptr);
  if (options.len > 0)
    free(options.items);
}

static bool execute(Editor *editor, u32 index) {
  if (index == 0) {
    editor->provider->free_opts(editor->options);
    editor->options.len = 0;
    editor->provider = &save_new_file_provider;
    editor->selected_option = 0;
    buffer_delete_line(editor->current_buffer);

    return false;
  }

  struct stat stat_data;
  char *path = wide_str_to_cstr(opts.items[index]);
  if (lstat(path, &stat_data) < 0)
    return true;

  // Whether or not it is a regular file
  if (S_ISREG(stat_data.st_mode)) {
    buffer_write_file(&editor->main_buffers.items[editor->current_main_buffer_index].buffer, path);

    free(path);
    return true;
  } else {
    chdir(path);
    editor->selected_option = 0;

    free(path);
    return false;
  }
}

Provider save_file_provider = {
  .get_opts = get_opts,
  .free_opts = free_opts,
  .execute = execute,
};
