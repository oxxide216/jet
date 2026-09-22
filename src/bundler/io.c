#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"

Str read_file(char *path) {
  Str content;

  FILE *file = fopen(path, "r");
  if (!file)
    return (Str) { NULL, (unsigned int) -1 };

  fseek(file, 0, SEEK_END);
  content.len = ftell(file);
  content.ptr = malloc(content.len);
  fseek(file, 0, SEEK_SET);
  fread(content.ptr, 1, content.len, file);
  fclose(file);

  return content;
}

bool write_file(char *path, Str content) {
  FILE *file = fopen(path, "w");
  if (!file) {
    return false;
  }

  fwrite(content.ptr, sizeof(*content.ptr), content.len, file);
  fclose(file);

  return true;
}
