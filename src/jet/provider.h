#ifndef PROVIDER_H
#define PROVIDER_H

#include "shl/shl-defs.h"
#include "shl/shl-str.h"
#include "common.h"

typedef struct Editor Editor;

typedef struct {
  WideStr name;
  f32     bg_r, bg_g, bg_b;
  f32     fg_r, fg_g, fg_b;
} PaletteOption;

typedef Da(PaletteOption) PaletteOptions;

typedef PaletteOptions (*ProviderGetOptionsFunc)(Editor *editor, WideStr content);
typedef void (*ProviderFreeOptionsFunc)(PaletteOptions options);
typedef bool (*ProviderExecute)(Editor *editor, PaletteOptions options, u32 index);

typedef struct {
  ProviderGetOptionsFunc  get_opts;
  ProviderFreeOptionsFunc free_opts;
  ProviderExecute         execute;
} Provider;

extern Provider command_provider;
extern Provider open_file_provider;
extern Provider save_file_provider;
extern Provider save_new_file_provider;
extern Provider error_provider;

#endif // PROVIDER_H
