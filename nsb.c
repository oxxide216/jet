#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

#define BUILD_FILE_NAME "build.nsb"

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define CUR(parser) ((parser)->content.ptr + (parser)->current)
#define PARSE_ERROR(parser, message)        \
  printf("%s:%u:%u [ERROR] " message,       \
         (parser)->path, (parser)->row + 1, \
         (parser)->col + 1)
#define PARSE_ERRORF(parser, fmt, ...)      \
  printf("%s:%u:%u [ERROR] " fmt,           \
         (parser)->path, (parser)->row + 1, \
         (parser)->col + 1, __VA_ARGS__)

#define STR_LIT(lit) (Str) { lit, sizeof(lit) - 1 }

#define Da(type)  \
  struct {        \
    type *items;  \
    u32 len, cap; \
  }

#define DA_APPEND_MANY(da, elements, _len)                               \
  do {                                                                   \
    if ((da).cap < (da).len + (_len)) {                                  \
      if ((da).cap != 0) {                                               \
        while ((da).cap < (da).len + (_len))                             \
          (da).cap *= 2;                                                 \
        (da).items = realloc((da).items, sizeof(*elements) * (da).cap);  \
      } else {                                                           \
        (da).cap = _len;                                                 \
        (da).items = malloc(sizeof(*elements) * (da).cap);               \
      }                                                                  \
    }                                                                    \
    memcpy((da).items + (da).len, elements, sizeof(*elements) * (_len)); \
    (da).len += _len;                                                    \
  } while (0)

#define DA_APPEND(da, element) DA_INSERT(da, (da).len, element)

#define DA_INSERT(da, index, element)                                 \
  do {                                                                \
    if ((da).cap <= (da).len) {                                       \
      if ((da).cap != 0) {                                            \
        while ((da).cap <= (da).len)                                  \
          (da).cap *= 2;                                              \
        (da).items = realloc((da).items, sizeof(element) * (da).cap); \
      } else {                                                        \
        (da).cap = 1;                                                 \
        (da).items = malloc(sizeof(element));                         \
      }                                                               \
    }                                                                 \
    memmove((da).items + (index) + 1,                                 \
            (da).items + (index),                                     \
            ((da).len - (index)) * sizeof(element));                  \
    (da).items[index] = element;                                      \
    ++(da).len;                                                       \
  } while (0)

typedef char               i8;
typedef unsigned char      u8;
typedef short              i16;
typedef unsigned short     u16;
typedef int                i32;
typedef unsigned int       u32;
#ifdef _WIN32
typedef long long          i64;
typedef unsigned long long u64;
#else
typedef long               i64;
typedef unsigned long      u64;
#endif

typedef struct {
  char *build_file_dir;
  bool  help;
  bool  debug;
  bool  verbose;
  bool  build_all;
  bool  rebuild_main_target;
  bool  rebuild_all_targets;
} Config;

typedef struct {
  char *ptr;
  u32   len;
} Str;

typedef Da(char *) CStrs;
typedef Da(Str)    Strs;
typedef Da(Strs)   Strss;

typedef Da(char) StringBuilder;

typedef struct {
  Str name;
  Str value;
} Var;

typedef Da(Var) Vars;

typedef enum {
  TypeExecutable = 0,
  TypeStaticLib,
  TypeSharedLib,
  TypeCustom,
} Type;

typedef struct TargetBuildInfo TargetBuildInfo;

typedef struct {
  Str              file;
  Type             type;
  Type             name_type;
  bool             is_name_type_explicit;
  Str              cmd;
  Str              src_pattern;
  Str              deps_pattern;
  Str              ghost_deps_pattern;
  Str              compiler;
  Str              archiver;
  Str              cflags;
  Str              ldflags;
  Str              arflags;
  Str              incpath;
  TargetBuildInfo *info;
} Target;

typedef Da(Target) Targets;

typedef struct {
  Target *target;
  bool    is_ghost;
} Dep;

typedef Da(Dep) Deps;

typedef struct {
  Str     content;
  Vars    vars;
  Targets targets;
} BuildConfig;

typedef struct {
  Str          content;
  u32          current;
  u32          row, col;
  char        *path;
  BuildConfig *build_config;
} Parser;

typedef enum {
  BuildResultOk = 0,
  BuildResultFail,
  BuildResultUpToDate,
} BuildResult;

struct TargetBuildInfo {
  Str         file;
  Type        type;
  Str         cmd;
  Str         srcs_expanded;
  Str         deps_expanded;
  Str         ghost_deps_expanded;
  Strs        srcs;
  Strss       header_deps;
  Str         compiler;
  Str         archiver;
  Str         cflags;
  Str         ldflags;
  Str         arflags;
  Str         incpath;
  Deps        deps;
  bool        rebuild;
  BuildResult build_result;
};

static Strs all_files_rec;
static Config config;

static void print_usage(char *program_name) {
  printf("Usage: %s <options...> <build.nsb directory>\n", program_name);
  printf("        -h --help          Show this message\n");
  printf("        --debug            Enable debug output\n");
  printf("        -v --verbose       Enable verbose output\n");
  printf("        -a --all           Build all targets (default - first only)\n");
  printf("        -r --rebuild       Always rebuild first target\n");
  printf("        -R --rebuild-all   Always rebuild all targets\n");
}

static bool parse_args(u32 argc, char **argv) {
  config.build_file_dir = ".";

  for (u32 i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-h") == 0) {
      config.help = true;
    } else if (strcmp(argv[i], "--help") == 0) {
      config.help = true;
    } else if (strcmp(argv[i], "--debug") == 0) {
      config.debug = true;
    } else if (strcmp(argv[i], "-v") == 0) {
      config.verbose = true;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      config.verbose = true;
    } else if (strcmp(argv[i], "-a") == 0) {
      config.build_all = true;
    } else if (strcmp(argv[i], "--all") == 0) {
      config.build_all = true;
    } else if (strcmp(argv[i], "-r") == 0) {
      config.rebuild_main_target = true;
    } else if (strcmp(argv[i], "--rebuild") == 0) {
      config.rebuild_main_target = true;
    } else if (strcmp(argv[i], "-R") == 0) {
      config.rebuild_main_target = true;
      config.rebuild_all_targets = true;
    } else if (strcmp(argv[i], "--rebuild-all") == 0) {
      config.rebuild_main_target = true;
      config.rebuild_all_targets = true;
    } else if (argv[i][0] == '-') {
      print_usage(argv[0]);
      printf("[ERROR] Unknown flag: %s\n", argv[i]);
      return false;
    } else {
      config.build_file_dir = argv[1];
    }
  }

  return true;
}

static void parser_destroy(Parser *parser) {
  (void) parser;
  // Empty for now
}

static void target_build_info_destroy(TargetBuildInfo *info) {
  free(info->file.ptr);
  free(info->srcs_expanded.ptr);
  free(info->deps_expanded.ptr);
  free(info->ghost_deps_expanded.ptr);
  if (info->srcs.items)
    free(info->srcs.items);
  for (u32 i = 0; i < info->header_deps.len; ++i) {
    Strs *header_deps = info->header_deps.items + i;
    for (u32 j = 0; j < header_deps->len; ++j)
      free(header_deps->items[j].ptr);
    if (header_deps->items)
      free(header_deps->items);
  }
  if (info->header_deps.items)
    free(info->header_deps.items);
  free(info->compiler.ptr);
  free(info->cflags.ptr);
  free(info->ldflags.ptr);
  free(info->incpath.ptr);
  if (info->deps.items)
    free(info->deps.items);
  free(info);
}

static void build_config_destroy(BuildConfig *build_config) {
  if (build_config->content.ptr)
    free(build_config->content.ptr);
  if (build_config->vars.items)
    free(build_config->vars.items);
  for (u32 i = 0; i < build_config->targets.len; ++i)
    if (build_config->targets.items[i].info)
      target_build_info_destroy(build_config->targets.items[i].info);
  if (build_config->targets.items)
    free(build_config->targets.items);
}

static void all_files_rec_destroy(void) {
  for (u32 i = 0; i < all_files_rec.len; ++i)
    free(all_files_rec.items[i].ptr);
  if (all_files_rec.items)
    free(all_files_rec.items);
}

static Str parse_ident(Parser *parser) {
  Str ident = { CUR(parser), 0 };

  if (!isalpha(*ident.ptr) && *ident.ptr != '_')
    return ident;

  while (parser->current < parser->content.len &&
         (isalnum(*CUR(parser)) ||
          *CUR(parser) == '_'))
    ++parser->current;

  ident.len = CUR(parser) - ident.ptr;
  parser->col += ident.len;

  return ident;
}

static Str parse_until(Parser *parser, char _char) {
  Str ident = { CUR(parser), 0 };

  while (parser->current < parser->content.len &&
         *CUR(parser) != _char) {
    if (*CUR(parser) == '\n') {
      ++parser->row;
      parser->col = 0;
    } else {
      ++parser->col;
    }
    ++parser->current;
  }

  ident.len = CUR(parser) - ident.ptr;

  return ident;
}

static void skip_whitespace(Parser *parser) {
  while (parser->current < parser->content.len &&
         isspace(*CUR(parser))) {
    if (*CUR(parser) == '\n') {
      ++parser->row;
      parser->col = 0;
    } else {
      ++parser->col;
    }
    ++parser->current;
  }
}

static void skip_whitespace_no_newline(Parser *parser) {
  while (parser->current < parser->content.len &&
         isspace(*CUR(parser)) && *CUR(parser) != '\n') {
    ++parser->col;
    ++parser->current;
  }
}

static void unexpected_error(Parser *parser) {
  if (parser->current >= parser->content.len)
    PARSE_ERROR(parser, "Unexpected EOF\n");

  PARSE_ERRORF(parser, "Unexpected character: %c\n", *CUR(parser));
}

static bool expect_char(Parser *parser, char _char) {
  if (parser->current >= parser->content.len) {
    PARSE_ERROR(parser, "Unexpected EOF\n");
    return false;
  }

  if (*CUR(parser) != _char) {
    PARSE_ERRORF(parser, "Unexpected character: %c, expected %c\n",
                 *CUR(parser), _char);
    return false;
  }

  return true;
}

static bool parse_var_def(Parser *parser) {
  Var var;

  var.name = parse_ident(parser);
  skip_whitespace(parser);
  if (!expect_char(parser, '='))
    return false;
  ++parser->current;
  ++parser->col;
  skip_whitespace_no_newline(parser);
  var.value = parse_until(parser, '\n');
  if (parser->current < parser->content.len) {
    ++parser->current;
    ++parser->row;
    parser->col = 0;
  }

  DA_APPEND(parser->build_config->vars, var);

  return true;
}

static bool str_eq(Str a, Str b) {
  if (a.len != b.len)
    return false;

  for (u32 i = 0; i < a.len; ++i)
    if (a.ptr[i] != b.ptr[i])
      return false;

  return true;
}

static Str choose_compiler(void) {
#ifdef _WIN32
  return STR_LIT("cl");
#else
  return STR_LIT("cc");
#endif
}

static Str choose_archiver(void) {
#ifdef _WIN32
  return STR_LIT("lib");
#else
  return STR_LIT("ar");
#endif
}

static Str choose_arflags(void) {
#ifdef _WIN32
  return STR_LIT("");
#else
  return STR_LIT("rcs");
#endif
}

static Str choose_compiler_executable_output_prefix(void) {
#ifdef _WIN32
  return STR_LIT("/Fe");
#else
  return STR_LIT("-o ");
#endif
}

static Str choose_compiler_shared_lib_output_prefix(void) {
#ifdef _WIN32
  return STR_LIT("/LD /Fe");
#else
  return STR_LIT("-shared -o ");
#endif
}

static Str choose_compiler_objective_output_prefix(void) {
#ifdef _WIN32
  return STR_LIT("/c /Fo");
#else
  return STR_LIT("-c -o ");
#endif
}

static Str choose_archiver_output_prefix(void) {
#ifdef _WIN32
  return STR_LIT("/out:");
#else
  return STR_LIT("");
#endif
}

static Str get_objective_extension(void) {
#ifdef _WIN32
  return STR_LIT(".obj");
#else
  return STR_LIT(".o");
#endif
}

static Str get_executable_extension(void) {
#ifdef _WIN32
  return STR_LIT(".exe");
#else
  return STR_LIT("");
#endif
}

static Str get_static_lib_extension(void) {
#ifdef _WIN32
  return STR_LIT(".lib");
#else
  return STR_LIT(".a");
#endif
}

static Str get_shared_lib_extension(void) {
#ifdef _WIN32
  return STR_LIT(".dll");
#else
  return STR_LIT(".so");
#endif
}

static bool get_quote_output_file(void) {
#ifdef _WIN32
  return true;
#else
  return false;
#endif
}

static bool parse_target(Parser *parser) {
  Target target = {0};

  ++parser->current;

  skip_whitespace(parser);

  target.file = parse_until(parser, ']');
  if (target.file.len == 0) {
    unexpected_error(parser);
    return false;
  }

  skip_whitespace(parser);

  if (!expect_char(parser, ']')) {
    return false;
  }

  ++parser->current;

  while (parser->current < parser->content.len && *CUR(parser) != '[') {
    skip_whitespace(parser);
    u32 name_row = parser->row;
    u32 name_col = parser->col;
    Str name = parse_ident(parser);
    skip_whitespace(parser);
    if (!expect_char(parser, '='))
      return false;
    ++parser->current;
    ++parser->col;
    skip_whitespace_no_newline(parser);
    u32 value_row = parser->row;
    u32 value_col = parser->col;
    Str value = parse_until(parser, '\n');
    if (parser->current < parser->content.len) {
      ++parser->current;
      ++parser->row;
      parser->col = 0;
    }
    skip_whitespace(parser);

    if (str_eq(name, STR_LIT("type")) || str_eq(name, STR_LIT("name_type"))) {
      Type type;

      if (str_eq(value, STR_LIT("executable"))) {
        type = TypeExecutable;
      } else if (str_eq(value, STR_LIT("static_lib"))) {
        type = TypeStaticLib;
      } else if (str_eq(value, STR_LIT("shared_lib"))) {
        type = TypeSharedLib;
      } else if (str_eq(value, STR_LIT("custom"))) {
        type = TypeCustom;
      } else {
        parser->row = value_row;
        parser->col = value_col;
        PARSE_ERRORF(parser, "Unknown target type: %.*s\n",
                     value.len, value.ptr);
        fprintf(stderr, "Known ones are:\n");
        fprintf(stderr, "    executable\n");
        fprintf(stderr, "    static_lib\n");
        fprintf(stderr, "    shared_lib\n");
        return false;
      }

      if (str_eq(name, STR_LIT("type"))) {
        target.type = type;
        if (!target.is_name_type_explicit)
          target.name_type = type;
      } else {
        target.name_type = type;
        target.is_name_type_explicit = true;
      }
    } else if (str_eq(name, STR_LIT("cmd"))) {
      target.cmd = value;
    } else if (str_eq(name, STR_LIT("src"))) {
      target.src_pattern = value;
    } else if (str_eq(name, STR_LIT("deps"))) {
      target.deps_pattern = value;
    } else if (str_eq(name, STR_LIT("ghost_deps"))) {
      target.ghost_deps_pattern = value;
    } else if (str_eq(name, STR_LIT("cc"))) {
      target.compiler = value;
    } else if (str_eq(name, STR_LIT("ar"))) {
      target.archiver = value;
    } else if (str_eq(name, STR_LIT("cflags"))) {
      target.cflags = value;
    } else if (str_eq(name, STR_LIT("ldflags"))) {
      target.ldflags = value;
    } else if (str_eq(name, STR_LIT("arflags"))) {
      target.arflags = value;
    } else if (str_eq(name, STR_LIT("incpath"))) {
      target.incpath = value;
    } else {
      parser->row = name_row;
      parser->col = name_col;
      PARSE_ERRORF(parser, "Unknown target field: %.*s\n",
                   name.len, name.ptr);
      return false;
    }
#ifndef _WIN32
    _Static_assert (sizeof(Target) == 200, "Target structure configuration changed");
#endif
  }

  if (target.type != TypeCustom && target.src_pattern.len == 0) {
    fprintf(stderr, "[ERROR] Target %.*s does not have `src` field\n",
            target.file.len, target.file.ptr);
    return false;
  }

  if (target.type == TypeStaticLib && target.incpath.len == 0) {
    fprintf(stderr, "[ERROR] Target of type static_lib %.*s does not have `incpath` field\n",
            target.file.len, target.file.ptr);
    return false;
  }

  if (target.type == TypeCustom && target.cmd.len == 0) {
    fprintf(stderr, "[ERROR] Target of type custom %.*s does not have `cmd` field\n",
            target.file.len, target.file.ptr);
    return false;
  }

  if (target.compiler.len == 0) {
    target.compiler = choose_compiler();
  }

  if (target.archiver.len == 0) {
    target.archiver = choose_archiver();
  }

  if (target.arflags.len == 0) {
    target.arflags = choose_arflags();
  }

  DA_APPEND(parser->build_config->targets, target);

  return true;
}

static Str read_file(char *path) {
  Str content;
  FILE *file = fopen(path, "r");
  if (!file)
    return (Str) { NULL, (u32) -1 };
  fseek(file, 0, SEEK_END);
  content.len = ftell(file);
  content.ptr = malloc(content.len);
  fseek(file, 0, SEEK_SET);
  fread(content.ptr, 1, content.len, file);
  fclose(file);
  return content;
}

static bool parse_build_file(char *path, BuildConfig *build_config) {
  Parser parser = {0};
  parser.path = path;
  parser.build_config = build_config;
  parser.content = read_file(path);

  if (parser.content.len == (u32) -1) {
    printf("[ERROR] Could not open build file\n");
    return false;
  }

  parser.build_config->content = parser.content;

  skip_whitespace(&parser);
  while (parser.current < parser.content.len) {
    if (isalpha(*CUR(&parser))) {
      if (!parse_var_def(&parser)) {
        parser_destroy(&parser);
        return false;
      }
    } else if (*CUR(&parser) == '[') {
      if (!parse_target(&parser)) {
        parser_destroy(&parser);
        return false;
      }
    } else {
      PARSE_ERRORF(&parser, "Unexpected character: %c\n", *CUR(&parser));
      parser_destroy(&parser);
      return false;
    }
    skip_whitespace(&parser);
  }

  parser_destroy(&parser);

  return true;
}

#define PRINT_TARGET_FIELD(target, field_name, field_str) \
  do {                                                    \
    printf(field_str " = %.*s\n",                         \
           (target)->field_name.len,                      \
           (target)->field_name.ptr);                     \
  } while (0)

static char *get_target_type_str(Type type) {
  switch (type) {
  case TypeExecutable: return "executable";
  case TypeStaticLib:  return "static_lib";
  case TypeSharedLib:  return "shared_lib";
  case TypeCustom:     return "custom";
  }

  fprintf(stderr, "UNREACHABLE\n");
  exit(1);
}

static void dump_build_config(BuildConfig *build_config) {
  for (u32 i = 0; i < build_config->vars.len; ++i) {
    Var *var = build_config->vars.items + i;
    printf("%.*s = %.*s\n",
           var->name.len, var->name.ptr,
           var->value.len, var->value.ptr);
  }

  for (u32 i = 0; i < build_config->targets.len; ++i) {
    if (i > 0 || build_config->vars.len > 0)
      putc('\n', stdout);
    Target *target = build_config->targets.items + i;
    printf("[%.*s]\n", target->file.len, target->file.ptr);
    printf("type = %s\n", get_target_type_str(target->type));
    printf("name_type = %s\n", get_target_type_str(target->name_type));
    PRINT_TARGET_FIELD(target, cmd, "cmd");
    PRINT_TARGET_FIELD(target, src_pattern, "src");
    PRINT_TARGET_FIELD(target, deps_pattern, "deps");
    PRINT_TARGET_FIELD(target, ghost_deps_pattern, "ghost_deps");
    PRINT_TARGET_FIELD(target, compiler, "cc");
    PRINT_TARGET_FIELD(target, archiver, "ar");
    PRINT_TARGET_FIELD(target, cflags, "cflags");
    PRINT_TARGET_FIELD(target, ldflags, "ldflags");
    PRINT_TARGET_FIELD(target, arflags, "arflags");
    PRINT_TARGET_FIELD(target, incpath, "incpath");
#ifndef _WIN32
    _Static_assert (sizeof(Target) == 200, "Target structure configuration changed");
#endif
  }
}

static Str expand_value(BuildConfig *build_config, Str value) {
  StringBuilder sb = {0};
  u32 anchor = 0;
  u32 i = 0;

  while (i < value.len) {
    if (value.ptr[i] == '$') {
      if (anchor < i)
        DA_APPEND_MANY(sb, value.ptr + anchor, i - anchor);

      anchor = ++i;

      while (i < value.len && (isalnum(value.ptr[i]) || value.ptr[i] == '_'))
        ++i;

      Str target_var = { value.ptr + anchor, i - anchor };

      for (u32 j = build_config->vars.len; j > 0; --j) {
        if (str_eq(build_config->vars.items[j - 1].name, target_var)) {
          Str value = build_config->vars.items[j - 1].value;
          Str target_value = expand_value(build_config, value);
          DA_APPEND_MANY(sb, target_value.ptr, target_value.len);
          free(target_value.ptr);
          break;
        }
      }

      anchor = i;
    } else {
      ++i;
    }
  }

  if (anchor < value.len)
    DA_APPEND_MANY(sb, value.ptr + anchor, value.len - anchor);

#ifdef _WIN32
  for (u32 i = 0; i < sb.len; ++i)
    if (sb.items[i] == '/')
      sb.items[i] = PATH_SEP;
#endif

  return (Str) { sb.items, sb.len };
}

char *str_to_cstr(Str str) {
  char *cstr = malloc(str.len + 1);
  memcpy(cstr, str.ptr, str.len);
  cstr[str.len] = '\0';
  return cstr;
}

static bool file_exists_cstr(char *path) {
#ifdef _WIN32
  DWORD attribs = GetFileAttributes(path);
  return attribs != INVALID_FILE_ATTRIBUTES &&
         (attribs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
  return access(path, F_OK) == 0;
#endif
}

static bool file_exists(Str path) {
  char *path_cstr = str_to_cstr(path);
  bool result = file_exists_cstr(path_cstr);
  free(path_cstr);
  return result;
}

static bool dir_exists_cstr(char *path) {
#ifdef _WIN32
  DWORD attribs = GetFileAttributes(path);
  return attribs != INVALID_FILE_ATTRIBUTES &&
         (attribs & FILE_ATTRIBUTE_DIRECTORY);
#else
  DIR *dir = opendir(path);
  bool result = dir != NULL;
  if (result)
    closedir(dir);
  return result;
#endif
}

void list_directory_existing_rec_internal(Strs *result, Str path) {
#ifdef _WIN32
  char *path_prepared = malloc(path.len + 2);
  memcpy(path_prepared, path.ptr, path.len);
  strcpy(path_prepared + path.len, "\\*");
  WIN32_FIND_DATA find_data;
  HANDLE file = FindFirstFile(path_prepared, &find_data);

  do {
    char *entry_path = find_data.cFileName;
#else
  DIR *dir = opendir(path.ptr);
  if (!dir)
    return;

  struct dirent *entry;
  while ((entry = readdir(dir))) {
    char *entry_path = entry->d_name;
#endif
    Str full_path;
    full_path.len = path.len + (path.len > 0) + strlen(entry_path);
    full_path.ptr = malloc(full_path.len + 1);
    memcpy(full_path.ptr, path.ptr, path.len);
    if (path.len > 0)
      full_path.ptr[path.len] = PATH_SEP;
    strcpy(full_path.ptr + path.len + (path.len > 0), entry_path);
    full_path.ptr[full_path.len] = '\0';

    if (strcmp(entry_path, ".") != 0 &&
        strcmp(entry_path, "..") != 0) {
#ifdef _WIN32
      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
#endif
      list_directory_existing_rec_internal(result, full_path);
      if (file_exists(full_path))
        DA_APPEND(*result, full_path);
      else
        free(full_path.ptr);
    } else {
      free(full_path.ptr);
    }
#ifdef _WIN32
  } while (FindNextFile(file, &find_data) != 0);

  FindClose(file);
  free(path_prepared);
#else
  }

  closedir(dir);
#endif
}

static Strs list_directory_existing_rec(char *path) {
  Strs result = {0};
  list_directory_existing_rec_internal(&result, (Str) { path, strlen(path) });
  return result;
}

static bool glob_matches(Str glob, Str str) {
  u32 i = 0, j = 0;

  if (str.len >= 2 && str.ptr[0] == '.' && str.ptr[1] == PATH_SEP)
    j += 2;

  while (i < glob.len && j < str.len) {
    if (glob.ptr[i] == '*') {
      bool is_double_star = i + 1 < glob.len && glob.ptr[i + 1] == '*';
      Str new_glob = {
        glob.ptr + i + 1,
        glob.len - i - 1,
      };
      Str new_str = {
        str.ptr + j,
        str.len - j,
      };
      if (glob_matches(new_glob, new_str))
        return true;
      if (!is_double_star && str.ptr[j] == PATH_SEP)
        return false;
      ++j;
    } else if (glob.ptr[i] == ' ') {
      Str new_glob = {
        glob.ptr + i + 1,
        glob.len - i - 1,
      };
      if (glob_matches(new_glob, str))
        return true;
    } else {
      if (str.ptr[j] != glob.ptr[i])
        return false;
      ++i, ++j;
    }
  }

  return i == glob.len && j == str.len;
}

static void match_glob_internal(Strs *result, Str glob) {
  bool skip_prefix = glob.len < 2 ||
                     glob.ptr[0] != '.' ||
                     glob.ptr[1] != PATH_SEP;

  for (u32 i = 0; i < all_files_rec.len; ++i) {
    Str file = all_files_rec.items[i];
    if (skip_prefix) {
      file.ptr += 2;
      file.len -= 2;
    }
    if (glob_matches(glob, file))
      DA_APPEND(*result, file);
  }
}

static Strs match_glob(Str glob) {
  Strs result = {0};

  u32 anchor = 0;
  for (u32 i = 0; i < glob.len; ++i) {
    if (glob.ptr[i] == ' ') {
      if (anchor < i) {
        Str local_glob = {
          glob.ptr + anchor,
          i - anchor,
        };
        anchor = i + 1;
        match_glob_internal(&result, local_glob);
      }
    }
  }

  if (anchor < glob.len) {
    Str local_glob = {
      glob.ptr + anchor,
      glob.len - anchor,
    };
    match_glob_internal(&result, local_glob);
  }

  return result;
}

static Str get_target_full_file(Str file, Type type) {
  if (type == TypeExecutable) {
    Str ext = get_executable_extension();
    Str full_file;
    full_file.len = file.len + ext.len;
    full_file.ptr = malloc(full_file.len);
    memcpy(full_file.ptr, file.ptr, file.len);
    memcpy(full_file.ptr + file.len, ext.ptr, ext.len);
    return full_file;
  } else if (type == TypeStaticLib) {
    Str ext = get_static_lib_extension();
    Str full_file;
    full_file.len = 3 + file.len + ext.len;
    full_file.ptr = malloc(full_file.len);
    strcpy(full_file.ptr, "lib");
    memcpy(full_file.ptr + 3, file.ptr, file.len);
    memcpy(full_file.ptr + 3 + file.len, ext.ptr, ext.len);
    return full_file;
  } else if (type == TypeSharedLib) {
    Str ext = get_shared_lib_extension();
    Str full_file;
    full_file.len = 3 + file.len + ext.len;
    full_file.ptr = malloc(full_file.len);
    strcpy(full_file.ptr, "lib");
    memcpy(full_file.ptr + 3, file.ptr, file.len);
    memcpy(full_file.ptr + 3 + file.len, ext.ptr, ext.len);
    return full_file;
  } else if (type == TypeCustom) {
    Str full_file;
    full_file.len = file.len;
    full_file.ptr = malloc(full_file.len);
    memcpy(full_file.ptr, file.ptr, file.len);
    return full_file;
  }

  fprintf(stderr, "UNREACHABLE\n");
  exit(1);
}

static Strs split(Str str, char sep) {
  Strs result = {0};
  u32 anchor = 0;

  for (u32 i = 0; i < str.len; ++i) {
    if (str.ptr[i] == sep && anchor < i) {
      Str part = { str.ptr + anchor, i - anchor };
      DA_APPEND(result, part);
      anchor = i + 1;
    }
  }

  if (anchor < str.len) {
    Str part = { str.ptr + anchor, str.len - anchor };
    DA_APPEND(result, part);
  }

  return result;
}

static void sb_append_str(StringBuilder *sb, Str str) {
  DA_APPEND_MANY(*sb, str.ptr, str.len);
}

static void sb_append_char(StringBuilder *sb, char _char) {
  DA_APPEND(*sb, _char);
}

static void sb_append_strs(StringBuilder *sb, Strs *strs) {
  for (u32 i = 0; i < strs->len; ++i) {
    sb_append_char(sb, ' ');
    sb_append_str(sb, strs->items[i]);
  }
}

static bool mem_eq(void *a, void *b, u64 n) {
  for (u32 i = 0; i < n; ++i)
    if (((u8 *) a)[i] != ((u8 *) b)[i])
      return false;

  return true;
}

static Str get_header_full_path(Str path, Str includer, Str cflags) {
  while (includer.len > 0 && includer.ptr[includer.len - 1] != PATH_SEP)
    --includer.len;

  StringBuilder sb = {0};
  sb_append_str(&sb, includer);
  sb_append_str(&sb, path);
  sb_append_char(&sb, '\0');

  if (file_exists_cstr(sb.items))
    return (Str) { sb.items, sb.len - 1 };

  sb.len = 0;

  u32 i = 0;
  while (i < cflags.len) {
    if (cflags.ptr[i] == '-' && i + 1 < cflags.len && cflags.ptr[i + 1] == 'I') {
      i += 2;

      while (i < cflags.len && isspace(cflags.ptr[i]))
        ++i;

      u32 anchor = i;

      while (i < cflags.len && !isspace(cflags.ptr[i]))
        ++i;

      sb_append_str(&sb, (Str) { cflags.ptr + anchor, i - anchor });
      if (cflags.ptr[i - 1] != PATH_SEP)
        sb_append_char(&sb, PATH_SEP);
      sb_append_str(&sb, path);
      sb_append_char(&sb, '\0');

      if (file_exists_cstr(sb.items))
        return (Str) { sb.items, sb.len - 1 };
    }

    ++i;
  }

  free(sb.items);

  return (Str) { NULL, (u32) -1 };
}

void parse_header_deps_internal(Strs *result, Str src, Str cflags) {
  char *str_cstr = malloc(src.len + 1);
  memcpy(str_cstr, src.ptr, src.len);
  str_cstr[src.len] = '\0';
  Str content = read_file(str_cstr);
  free(str_cstr);
  if (content.len == (u32) -1)
    return;

  u32 i = 0;
  while (i < content.len) {
    if (i + 8 < content.len && mem_eq(content.ptr + i, "#include", 8)) {
      i += 8;

      while (i < content.len && isspace(content.ptr[i]))
        ++i;

      if (i < content.len && content.ptr[i] == '"') {
        u32 anchor = ++i;

        while (i < content.len && content.ptr[i] != '"')
          ++i;

        Str path = {
          content.ptr + anchor,
          i - anchor,
        };
        Str full_path = get_header_full_path(path, src, cflags);
        if (full_path.len != (u32) -1) {
          bool visited = false;

          for (u32 j = 0; j < result->len; ++j) {
            if (str_eq(result->items[j], full_path)) {
              visited = true;
              break;
            }
          }

          if (!visited) {
            DA_APPEND(*result, full_path);
            parse_header_deps_internal(result, full_path, cflags);
          }
        }
      }
    }

    ++i;
  }

  free(content.ptr);
}

static Strs parse_header_deps(Str src, Str cflags) {
  Strs result = {0};
  parse_header_deps_internal(&result, src, cflags);
  return result;
}

static TargetBuildInfo *get_target_build_info(BuildConfig *build_config, Target *target) {
  TargetBuildInfo *info = malloc(sizeof(TargetBuildInfo));
  memset(info, 0, sizeof(TargetBuildInfo));
  Str file_expanded = expand_value(build_config, target->file);
  info->file = get_target_full_file(file_expanded, target->name_type);
  free(file_expanded.ptr);
  info->type = target->type;
  info->cmd = expand_value(build_config, target->cmd);
  info->srcs_expanded = expand_value(build_config, target->src_pattern);
  info->deps_expanded = expand_value(build_config, target->deps_pattern);
  info->ghost_deps_expanded = expand_value(build_config, target->ghost_deps_pattern);
  info->srcs = match_glob(info->srcs_expanded);
  info->compiler = expand_value(build_config, target->compiler);
  info->archiver = expand_value(build_config, target->archiver);
  info->cflags = expand_value(build_config, target->cflags);
  info->ldflags = expand_value(build_config, target->ldflags);
  info->arflags = expand_value(build_config, target->arflags);
  info->incpath = expand_value(build_config, target->incpath);
  info->rebuild = config.rebuild_all_targets ||
                  (config.rebuild_main_target && target == build_config->targets.items);
  info->build_result = BuildResultFail;

  for (u32 i = 0; i < info->srcs.len; ++i) {
    Dep dep = { NULL, false };

    Strs header_deps = parse_header_deps(info->srcs.items[i], info->cflags);
    DA_APPEND(info->header_deps, header_deps);

    if (config.debug) {
      printf("[INFO] %.*s header dependencies:\n",
             info->srcs.items[i].len, info->srcs.items[i].ptr);
      for (u32 j = 0; j < header_deps.len; ++j)
        printf("  -> %.*s\n", header_deps.items[j].len, header_deps.items[j].ptr);
    }

    for (u32 j = 0; j < build_config->targets.len; ++j) {
      Str target_file = expand_value(build_config, build_config->targets.items[j].file);
      if (str_eq(target_file, info->srcs.items[i])) {
        dep.target = build_config->targets.items + j;
        free(target_file.ptr);
        break;
      }
      free(target_file.ptr);
    }

    if (dep.target)
      DA_APPEND(info->deps, dep);
  }

  Strs deps = split(info->deps_expanded, ' ');

  for (u32 i = 0; i < deps.len; ++i) {
    Dep dep = { NULL, false };

    for (u32 j = 0; j < build_config->targets.len; ++j) {
      Str target_file = expand_value(build_config, build_config->targets.items[j].file);
      if (str_eq(target_file, deps.items[i])) {
        dep.target = build_config->targets.items + j;
        free(target_file.ptr);
        break;
      }
      free(target_file.ptr);
    }

    if (dep.target)
      DA_APPEND(info->deps, dep);
  }

  if (deps.items)
    free(deps.items);

  Strs ghost_deps = split(info->ghost_deps_expanded, ' ');

  for (u32 i = 0; i < ghost_deps.len; ++i) {
    Dep dep = { NULL, true };

    for (u32 j = 0; j < build_config->targets.len; ++j) {
      Str target_file = expand_value(build_config, build_config->targets.items[j].file);
      if (str_eq(target_file, ghost_deps.items[i])) {
        dep.target = build_config->targets.items + j;
        free(target_file.ptr);
        break;
      }
      free(target_file.ptr);
    }

    if (dep.target)
      DA_APPEND(info->deps, dep);
  }

  if (ghost_deps.items)
    free(ghost_deps.items);

  return info;
}

static Str src_to_obj_path(TargetBuildInfo *target_info, Str src) {
  bool add_slash = target_info->incpath.len > 0 && target_info->incpath.ptr[target_info->incpath.len - 1] != PATH_SEP;
  Str extension = get_objective_extension();
  Str result;
  result.len = target_info->incpath.len + add_slash + src.len + extension.len;
  result.ptr = malloc(result.len);
  memcpy(result.ptr, target_info->incpath.ptr, target_info->incpath.len);
  if (add_slash)
    result.ptr[target_info->incpath.len] = PATH_SEP;
  memcpy(result.ptr + target_info->incpath.len + add_slash, src.ptr, src.len);
  memcpy(result.ptr + target_info->incpath.len + add_slash + src.len, extension.ptr, extension.len);
  return result;
}

static void make_directory(Str path, bool is_file_path) {
  u32 i = 0;

  while (i < path.len) {
    if (path.ptr[i] == PATH_SEP) {
      char prev = path.ptr[i];
      path.ptr[i] = '\0';
      if (!dir_exists_cstr(path.ptr)) {
        if (config.verbose)
          printf("[INFO] Creating directory %s\n", path.ptr);
#ifdef _WIN32
        CreateDirectoryA(path.ptr, NULL);
#else
        mkdir(path.ptr, 0777);
#endif
      }
      path.ptr[i] = prev;
    }

    ++i;
  }

  if (!is_file_path && path.ptr[path.len - 1] != PATH_SEP) {
    Str temp_path;
    temp_path.len = path.len;
    temp_path.ptr = malloc(temp_path.len + 1);
    memcpy(temp_path.ptr, path.ptr, temp_path.len);
    temp_path.ptr[temp_path.len] = '\0';
    if (!dir_exists_cstr(temp_path.ptr)) {
      if (config.verbose)
        printf("[INFO] Creating directory %s\n", temp_path.ptr);
#ifdef _WIN32
      CreateDirectoryA(temp_path.ptr, NULL);
#else
      mkdir(temp_path.ptr, 0777);
#endif
    }
    free(temp_path.ptr);
  }
}

static bool needs_rebuild(Str src, Str dest) {
#ifdef _WIN32
  BOOL result;
  FILETIME src_time, dest_time;
#else
  struct stat src_stat, dest_stat;
#endif
  StringBuilder sb = {0};

  sb_append_str(&sb, src);
  sb_append_char(&sb, '\0');
#ifdef _WIN32
  HANDLE src_file = CreateFile(sb.items, GENERIC_READ, 0, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  result = GetFileTime(src_file, NULL, NULL, &src_time);
  CloseHandle(src_file);
  if (!result) {
#else
  if (stat(sb.items, &src_stat) < 0) {
#endif
    free(sb.items);
    return true;
  }

  sb.len = 0;

  sb_append_str(&sb, dest);
  sb_append_char(&sb, '\0');
#ifdef _WIN32
  HANDLE dest_file = CreateFile(sb.items, GENERIC_READ, 0, NULL,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  result = GetFileTime(dest_file, NULL, NULL, &dest_time);
  CloseHandle(dest_file);
  if (!result) {
#else
  if (stat(sb.items, &dest_stat) < 0) {
#endif
    free(sb.items);
    return true;
  }

  free(sb.items);

#ifdef _WIN32
  ULARGE_INTEGER src_int, dest_int;
  src_int.u.LowPart = src_time.dwLowDateTime;
  src_int.u.HighPart = src_time.dwHighDateTime;
  dest_int.u.LowPart = dest_time.dwLowDateTime;
  dest_int.u.HighPart = dest_time.dwHighDateTime;
  return src_int.QuadPart > dest_int.QuadPart;
#else
  return src_stat.st_mtime > dest_stat.st_mtime;
#endif
}

static bool needs_rebuild_many_srcs(Strs *srcs, Str dest) {
  for (u32 i = 0; i < srcs->len; ++i)
    if (needs_rebuild(srcs->items[i], dest))
      return true;

  return false;
}

static bool needs_rebuild_deps(Deps *deps, Str dest) {
  for (u32 i = 0; i < deps->len; ++i)
    if (needs_rebuild(deps->items[i].target->info->file, dest))
      return true;

  return false;
}

static char *get_target_obj_build_cmd(TargetBuildInfo *info, u32 index) {
  Str obj_path = src_to_obj_path(info, info->srcs.items[index]);
  if (!info->rebuild &&
      !needs_rebuild(info->srcs.items[index], obj_path) &&
      !needs_rebuild_many_srcs(info->header_deps.items + index, obj_path)) {
    free(obj_path.ptr);
    return NULL;
  }

  make_directory(obj_path, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->compiler);
  if (info->cflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->cflags);
  }
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, choose_compiler_objective_output_prefix());
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  sb_append_str(&sb, obj_path);
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, info->srcs.items[index]);
  sb_append_char(&sb, '\0');

  free(obj_path.ptr);
  return sb.items;
}

static char *get_inc_executable_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild && !needs_rebuild_deps(&info->deps, info->file))
    return NULL;

  make_directory(info->file, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->compiler);
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, choose_compiler_executable_output_prefix());
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  sb_append_str(&sb, info->file);
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  for (u32 i = 0; i < info->srcs.len; ++i) {
    Str obj_path = src_to_obj_path(info, info->srcs.items[i]);
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, obj_path);
    free(obj_path.ptr);
  }
  for (u32 i = 0; i < info->deps.len; ++i) {
    if (!info->deps.items[i].is_ghost) {
      sb_append_char(&sb, ' ');
      sb_append_str(&sb, info->deps.items[i].target->info->file);
    }
  }
  if (info->ldflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->ldflags);
  }
  sb_append_char(&sb, '\0');

  return sb.items;
}

static char *get_full_executable_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild &&
      !needs_rebuild_many_srcs(&info->srcs, info->file) &&
      !needs_rebuild_deps(&info->deps, info->file))
    return NULL;

  make_directory(info->file, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->compiler);
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, choose_compiler_executable_output_prefix());
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  sb_append_str(&sb, info->file);
  if (get_quote_output_file())
    sb_append_char(&sb, '"');
  if (info->cflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->cflags);
  }
  if (info->srcs.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_strs(&sb, &info->srcs);
  }
  for (u32 i = 0; i < info->deps.len; ++i) {
    if (!info->deps.items[i].is_ghost) {
      sb_append_char(&sb, ' ');
      sb_append_str(&sb, info->deps.items[i].target->info->file);
    }
  }
  if (info->ldflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->ldflags);
  }
  sb_append_char(&sb, '\0');

  return sb.items;
}

static char *get_static_lib_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild && !needs_rebuild_deps(&info->deps, info->file))
    return NULL;

  Strs obj_paths = {0};
  for (u32 i = 0; i < info->srcs.len; ++i)
    DA_APPEND(obj_paths, src_to_obj_path(info, info->srcs.items[i]));

  make_directory(info->file, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->archiver);
  if (info->arflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->arflags);
  }
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, choose_archiver_output_prefix());
  sb_append_str(&sb, info->file);
  sb_append_strs(&sb, &obj_paths);
  sb_append_char(&sb, '\0');

  for (u32 i = 0; i < obj_paths.len; ++i)
    free(obj_paths.items[i].ptr);
  if (obj_paths.items)
    free(obj_paths.items);

  return sb.items;
}

static char *get_inc_shared_lib_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild && !needs_rebuild_deps(&info->deps, info->file))
    return NULL;

  Strs obj_paths = {0};
  for (u32 i = 0; i < info->srcs.len; ++i)
    DA_APPEND(obj_paths, src_to_obj_path(info, info->srcs.items[i]));

  if (!info->rebuild && !needs_rebuild_many_srcs(&obj_paths, info->file)) {
    for (u32 i = 0; i < obj_paths.len; ++i)
      free(obj_paths.items[i].ptr);
    if (obj_paths.items)
      free(obj_paths.items);

    return NULL;
  }

  StringBuilder sb = {0};
  sb_append_str(&sb, info->compiler);
  sb_append_char(&sb, ' ');
  u32 saved_len = sb.len;
  sb_append_str(&sb, choose_compiler_shared_lib_output_prefix());
  sb_append_str(&sb, info->file);
  make_directory((Str) { sb.items + saved_len, sb.len - saved_len }, true);
  sb_append_strs(&sb, &obj_paths);
  for (u32 i = 0; i < info->deps.len; ++i) {
    if (!info->deps.items[i].is_ghost) {
      sb_append_char(&sb, ' ');
      sb_append_str(&sb, info->deps.items[i].target->info->file);
    }
  }
  if (info->ldflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->ldflags);
  }
  sb_append_char(&sb, '\0');

  for (u32 i = 0; i < obj_paths.len; ++i)
    free(obj_paths.items[i].ptr);
  if (obj_paths.items)
    free(obj_paths.items);

  return sb.items;
}

static char *get_full_shared_lib_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild) {
    if (!needs_rebuild_many_srcs(&info->srcs, info->file))
      return NULL;
    if (!needs_rebuild_deps(&info->deps, info->file))
      return NULL;
  }

  make_directory(info->file, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->compiler);
  sb_append_char(&sb, ' ');
  sb_append_str(&sb, choose_compiler_shared_lib_output_prefix());
  sb_append_str(&sb, info->file);
  if (info->cflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->cflags);
  }
  if (info->srcs.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_strs(&sb, &info->srcs);
  }
  for (u32 i = 0; i < info->deps.len; ++i) {
    if (!info->deps.items[i].is_ghost) {
      sb_append_char(&sb, ' ');
      sb_append_str(&sb, info->deps.items[i].target->info->file);
    }
  }
  if (info->ldflags.len > 0) {
    sb_append_char(&sb, ' ');
    sb_append_str(&sb, info->ldflags);
  }
  sb_append_char(&sb, '\0');

  return sb.items;
}

static char *get_custom_target_build_cmd(TargetBuildInfo *info) {
  if (!info->rebuild) {
    if (!needs_rebuild_many_srcs(&info->srcs, info->file))
      return NULL;
    if (!needs_rebuild_deps(&info->deps, info->file))
      return NULL;
  }

  make_directory(info->file, true);

  StringBuilder sb = {0};
  sb_append_str(&sb, info->cmd);
  sb_append_char(&sb, '\0');

  return sb.items;
}

static void create_gitignore_file(Str prefix) {
  u32 prefix_len = 0;
  while (prefix_len < prefix.len && prefix.ptr[prefix_len] != PATH_SEP)
    ++prefix_len;

  char base_name[] = ".gitignore";
  Str full_path;
  full_path.len = prefix_len + sizeof(base_name);
  full_path.ptr = malloc(full_path.len + 1);
  memcpy(full_path.ptr, prefix.ptr, prefix_len);
  full_path.ptr[prefix_len] = PATH_SEP;
  strcpy(full_path.ptr + prefix_len + 1, base_name);
  full_path.ptr[full_path.len] = '\0';

  if (!file_exists_cstr(full_path.ptr)) {
    FILE *file = fopen(full_path.ptr, "w");
    if (file) {
      fwrite("*\n", 1, 2, file);
      fclose(file);
    }
  }

  free(full_path.ptr);
}

static BuildResult build(BuildConfig *build_config, Target *target) {
  TargetBuildInfo *info = target->info;
  if (!info) {
    info = get_target_build_info(build_config, target);
    target->info = info;
  }

  if (info->build_result != BuildResultFail)
    return info->build_result;

  for (u32 i = 0; i < info->deps.len; ++i) {
    BuildResult result = build(build_config, info->deps.items[i].target);
    if (result == BuildResultFail)
      return BuildResultFail;
    if (result == BuildResultOk)
      info->rebuild = true;
  }

  if (((info->type == TypeExecutable || info->type == TypeSharedLib) &&
       info->incpath.len > 0) ||
      info->type == TypeStaticLib) {
    make_directory(info->incpath, false);
    create_gitignore_file(info->incpath);

    CStrs src_build_cmds = {0};
    for (u32 i = 0; i < info->srcs.len; ++i) {
      char *cmd = get_target_obj_build_cmd(info, i);
      if (cmd)
        DA_APPEND(src_build_cmds, cmd);
    }

    if (src_build_cmds.len > 0) {
      printf("[INFO] Compiling sources for target %.*s\n", info->file.len, info->file.ptr);
      info->rebuild = true;
    }

    bool failed = false;
    for (u32 i = 0; i < src_build_cmds.len; ++i) {
      if (config.verbose)
        printf("[INFO] Running %s\n", src_build_cmds.items[i]);
      int result = system(src_build_cmds.items[i]);
      free(src_build_cmds.items[i]);
      if (result != 0)
        failed = true;
    }

    if (src_build_cmds.items)
      free(src_build_cmds.items);

    if (failed)
      return BuildResultFail;
  } else if (info->type == TypeCustom && info->srcs.len > 0) {
    for (u32 i = 0; i < info->srcs.len; ++i) {
      if (needs_rebuild(info->srcs.items[i], info->file)) {
        info->rebuild = true;
        break;
      }
    }
  }

  char *cmd;
  if (info->type == TypeExecutable) {
    if (info->incpath.len > 0)
      cmd = get_inc_executable_target_build_cmd(info);
    else
      cmd = get_full_executable_target_build_cmd(info);
  } else if (info->type == TypeStaticLib) {
    cmd = get_static_lib_target_build_cmd(info);
  } else if (info->type == TypeSharedLib) {
    if (info->incpath.len > 0)
      cmd = get_inc_shared_lib_target_build_cmd(info);
    else
      cmd = get_full_shared_lib_target_build_cmd(info);
  } else if (info->type == TypeCustom) {
    cmd = get_custom_target_build_cmd(info);
  }

  if (!cmd) {
    info->build_result = BuildResultUpToDate;
    return BuildResultUpToDate;
  }

  printf("[INFO] Compiling target %.*s\n", info->file.len, info->file.ptr);
  if (config.verbose)
    printf("[INFO] Running %s\n", cmd);
  BuildResult result = system(cmd) == 0 ? BuildResultOk : BuildResultFail;
  free(cmd);
  info->build_result = BuildResultOk;
  return result;
}

i32 main(i32 argc, char **argv) {
  if (!parse_args(argc, argv))
    return 1;

  if (config.help) {
    print_usage(argv[0]);
    return 0;
  }

#ifdef _WIN32
  SetCurrentDirectory(config.build_file_dir);
#else
  chdir(config.build_file_dir);
#endif

  BuildConfig build_config = {0};
  if (!parse_build_file(BUILD_FILE_NAME, &build_config)) {
    build_config_destroy(&build_config);
    return 1;
  }

  if (config.debug)
    dump_build_config(&build_config);

  if (build_config.targets.len == 0) {
    fprintf(stderr, "[ERROR] No targets provided, nothing to do\n");
    build_config_destroy(&build_config);
    return 1;
  }

  all_files_rec = list_directory_existing_rec(".");

  if (config.build_all) {
    for (u32 i = 0; i < build_config.targets.len; ++i) {
      if (build(&build_config, build_config.targets.items + i) == BuildResultFail) {
        build_config_destroy(&build_config);
        all_files_rec_destroy();
        return 1;
      }
    }
  } else {
    if (build(&build_config, build_config.targets.items) == BuildResultFail) {
      build_config_destroy(&build_config);
      all_files_rec_destroy();
      return 1;
    }
  }

  build_config_destroy(&build_config);
  all_files_rec_destroy();

  return 0;
}
