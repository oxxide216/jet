#include "editor.h"

MainBuffer main_buffer_make(char *path) {
  return (MainBuffer) { buffer_make(), 0.0, path };
}
