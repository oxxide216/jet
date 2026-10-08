#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <unistd.h>
#include <linux/limits.h>
#endif

#include "platform.h"

const char *get_socket_path_prefix(void) {
#ifdef _WIN32
  return "C:\\jet-socket-";
#else
  return "/tmp/jet-socket-";
#endif
}

u32 get_process_id(void) {
#ifdef _WIN32
  return GetProcessId(GetCurrentProcess());
#else
  return getpid();
#endif
}

Str get_current_dir(void) {
  char buffer[PATH_MAX];
  getcwd(buffer, sizeof(buffer));
  Str result;
  result.len = strlen(buffer);
  result.ptr = malloc(result.len * sizeof(*result.ptr));
  memcpy(result.ptr, buffer, result.len * sizeof(*result.ptr));
  return result;
}
