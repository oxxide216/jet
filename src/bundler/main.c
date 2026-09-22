#include "io.h"
#include "shl/shl-defs.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"
#include "shl/shl-log.h"

#define INPUTS_START 2

typedef Da(Str) Strs;

void print_usage(char *file_name) {
  fprintf(stderr, "Usage: %s <output> <inputs...>\n", file_name);
}

static u32 copy_file_as_c(char *src_path, FILE *dest) {
  FILE *src = fopen(src_path, "r");
  if (!src) {
    ERROR("Could not read %s\n", src_path);
    return 0;
  }

  u8 buffer[1024];
  u32 size = 0;

  while (true) {
    u64 n = fread(buffer, 1, sizeof(buffer), src);
    if (n > 0) {
      for (u32 i = 0; i < n; ++i) {
        if (size == 0 && i == 0) {
          fprintf(dest, "  ");
        } else {
          fputc(',', dest);
          if (i % 10 == 0)
            fprintf(dest, "\n  ");
          else
            fputc(' ', dest);
        }
        fprintf(dest, "0x%02x", buffer[i]);
      }
      size += n;
    }
    if (n < sizeof(buffer))
      break;
  }

  return size;
}

i32 main(i32 argc, char **argv) {
  if (argc < 2) {
    print_usage(argv[0]);
    ERROR("Output file was not provided\n");
    return 1;
  }

  remove(argv[1]);
  FILE *output_file = fopen(argv[1], "w");
  if (!output_file) {
    ERROR("Could not write %s\n", argv[1]);
    return 1;
  }

  Strs shaders;
  shaders.len = argc - INPUTS_START;
  shaders.cap = argc - INPUTS_START;
  shaders.items = malloc(shaders.len * sizeof(Str));

  fprintf(output_file, "#include \"shl/shl-defs.h\"\n");
  fprintf(output_file, "#include \"shl/shl-str.h\"\n");
  for (u32 i = INPUTS_START; i < (u32) argc; ++i) {
    u32 len = strlen(argv[i]);
    fprintf(output_file, "char ");
    for (u32 j = 0; j < len; ++j) {
      if (argv[i][j] == '/' || argv[i][j] == '.')
        fputc('_', output_file);
      else
        fputc(argv[i][j], output_file);
    }
    fprintf(output_file, "_cstr[] = {\n");
    u32 size = copy_file_as_c(argv[i], output_file);
    shaders.items[i - INPUTS_START].len = size;
    if (size == 0) {
      free(shaders.items);
      fclose(output_file);
      return 1;
    }
    fprintf(output_file, "\n};\n");
  }
  for (u32 i = INPUTS_START; i < (u32) argc; ++i) {
    u32 len = strlen(argv[i]);
    fprintf(output_file, "Str ");
    for (u32 j = 0; j < len; ++j) {
      if (argv[i][j] == '/' || argv[i][j] == '.')
        fputc('_', output_file);
      else
        fputc(argv[i][j], output_file);
    }
    fprintf(output_file, " = { ");
    for (u32 j = 0; j < len; ++j) {
      if (argv[i][j] == '/' || argv[i][j] == '.')
        fputc('_', output_file);
      else
        fputc(argv[i][j], output_file);
    }
    fprintf(output_file, "_cstr, %u", shaders.items[i - INPUTS_START].len);
    fprintf(output_file, " };\n");
  }

  free(shaders.items);
  fclose(output_file);
  return 0;
}
