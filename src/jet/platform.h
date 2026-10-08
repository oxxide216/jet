#ifndef PLATFORM_H
#define PLATFORM_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"

const char *get_socket_path_prefix(void);
u32         get_process_id(void);
Str         get_current_dir(void);

#endif // PLATFORM_H
