#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#endif

#include "platform.h"
#include "shl/shl-log.h"

static char static_buffer[512];

const char *get_first_jet_socket_path(void) {
#ifdef _WIN32
#error "No Windows support for now"
#else
  DIR *dir = opendir("/tmp");
  if (!dir)
    return NULL;

  const char prefix[] = "jet-socket-";

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (strlen(entry->d_name) >= ARRAY_LEN(prefix) - 1) {
      bool begins_with_prefix = true;

      for (u32 i = 0; i < ARRAY_LEN(prefix) - 1; ++i) {
        if (entry->d_name[i] != prefix[i]) {
          begins_with_prefix = false;
          break;
        }
      }

      if (begins_with_prefix) {
        snprintf(static_buffer, sizeof(static_buffer), "/tmp/%s", entry->d_name);
        closedir(dir);
        return static_buffer;
      }
    }
  }

  closedir(dir);
  return NULL;
#endif
}

Child run_command_capturing_output(char **const args) {
#ifdef _WIN32
#error "No Windows support for now"
#else
  i32 pipe_fds[2];
  if (pipe(pipe_fds) < 0) {
    ERROR("Failed to create pipe\n");
    exit(1);
  }

  pid_t pid = fork();
  if (pid < 0) {
    ERROR("Failed to fork\n");
    exit(1);
  }

  if (pid == 0) {
    close(pipe_fds[0]);

    if (dup2(pipe_fds[1], 1) < 0) {
      ERROR("Failed to dup2\n");
      exit(1);
    }

    if (dup2(pipe_fds[1], 2) < 0) {
      ERROR("Failed to dup2\n");
      exit(1);
    }

    if (execvp(args[0], args) < 0) {
      ERROR("Failed to run command: %s\n", strerror(errno));
      exit(1);
    }
  }

  close(pipe_fds[1]);

  fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK);

  return (Child) { pid, pipe_fds[0] };
#endif
}

bool child_read(Child *child, Stream *output) {
  i32 len;
  while ((len = read(child->pipe, output->items + output->len, output->cap - output->len)) > 0) {
    if ((u32) len == output->len + output->cap) {
      output->len += len;
      output->cap *= 2;
      output->items = realloc(output->items, output->cap * sizeof(*output->items));
    } else {
      output->len += len;
    }
  }

  if (len == 0)
    return false;

  return true;
}

void child_wait(Child *child) {
  waitpid(child->pid, NULL, 0);
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
