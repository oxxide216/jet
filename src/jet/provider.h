#ifndef PROVIDER_H
#define PROVIDER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "common.h"

typedef struct Editor Editor;

typedef WideStrs (*ProviderGetOptionsFunc)(WideStr content);
typedef void (*ProviderFreeOptionsFunc)(WideStrs options);
typedef bool (*ProviderExecute)(Editor *editor, u32 index);

typedef struct {
  ProviderGetOptionsFunc  get_opts;
  ProviderFreeOptionsFunc free_opts;
  ProviderExecute         execute;
} Provider;

extern Provider command_provider;
extern Provider open_file_provider;
extern Provider save_file_provider;
extern Provider save_new_file_provider;

#endif // PROVIDER_H
