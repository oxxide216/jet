#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <unistd.h>
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
