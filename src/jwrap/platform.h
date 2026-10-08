#ifndef PLATFORM_H
#define PLATFORM_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

#ifdef _WIN32
#error "No Windows support for now"
#else
#include <sys/types.h>

typedef i32 Fd;

typedef struct {
  pid_t pid;
  Fd    pipe;
} Child;
#endif

typedef Da(char) Stream;

const char *get_first_jet_socket_path(void);
Child       run_command_capturing_output(char **const args);
bool        child_read(Child *child, Stream *output);
void        child_wait(Child *child);
Str         get_current_dir(void);

#endif // PLATFORM_H
