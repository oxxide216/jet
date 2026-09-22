#ifndef IO_H
#define IO_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

Str  read_file(char *path);
bool write_file(char *path, Str content);

#endif // IO_H
