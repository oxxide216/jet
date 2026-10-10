#include <wchar.h>

#include "shl/shl-defs.h"
#include "shl/shl-log.h"
#include "viking/viking.h"
#include "winx/event.h"
#include "cns/cns.h"
#include "../../build/assets.c"
#include "text-renderer.h"
#include "shape-renderer.h"
#include "editor.h"
#include "buffer.h"
#include "provider.h"
#include "platform.h"
// Customize your editor here!
#include "config.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

#define CURRENT_BUFFER()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].buffer

#define CURRENT_SCROLL()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].scroll

#define CURRENT_FILE_PATH()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].file_path

#define CURRENT_ABS_FILE_PATH()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].abs_file_path

#define CURRENT_ERRORS()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].errors

#define CURRENT_WARNINGS()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].warnings

#define CURRENT_INFOS()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].infos

static u32 get_max_visual_line_len(WideStr line, WinxWindow *window, TextRenderer *tr) {
  f32 editor_width = window->width - BUFFER_PADDING * 2.0;
  f32 line_width = tr_measure_text(tr, line.ptr, line.len);
  u32 space = U' ';
  f32 space_width = tr_measure_text(tr, &space, 1);
  u32 spaces = 0;
  while (line_width + spaces * space_width < editor_width)
    ++spaces;
  while (line.len > 0 &&
         tr_measure_text(tr, line.ptr, line.len) + spaces * space_width > editor_width)
    --line.len;
  return line.len + spaces;
}

static void get_buffer_selection_bounds(Buffer *buffer,
                                        u32 *min_row, u32 *min_col,
                                        u32 *max_row, u32 *max_col) {
  if (buffer->is_selecting) {
    if (buffer->cursor_row > buffer->anchor_row ||
        (buffer->cursor_row == buffer->anchor_row &&
         buffer->cursor_col >= buffer->anchor_col)) {
      *min_row = buffer->anchor_row;
      *min_col = buffer->anchor_col;
      *max_row = buffer->cursor_row;
      *max_col = buffer->cursor_col;
    } else {
      *min_row = buffer->cursor_row;
      *min_col = buffer->cursor_col;
      *max_row = buffer->anchor_row;
      *max_col = buffer->anchor_col;
    }
  } else {
    *min_row = buffer->cursor_row;
    *min_col = buffer->cursor_col;
    *max_row = buffer->cursor_row;
    *max_col = buffer->cursor_col + 1;
  }
}

static CnsResult connected(CnsCtx *ctx, CnsConnection *connection) {
  Editor *editor = cns_get_user_data(ctx);
  editor->jwrap = connection;

  return CnsResultOk;
}

static CnsResult data(CnsCtx *ctx, CnsConnection *connection, unsigned char *data, unsigned long data_len) {
  (void) ctx;
  (void) connection;

  if (data_len < sizeof(u32))
    return CnsResultOk;

  u32 entries_len = *(u32 *) data;
  data += sizeof(u32);
  data_len -= sizeof(u32);

  Editor *editor = cns_get_user_data(ctx);
  editor_clear_entries(editor);
  editor->entry_cursor = (u32) -1;

  u32 len = data_len;
  Message message;
  for (u32 i = 0; i < entries_len; ++i) {
    decode_message(&message, &data, &len);
    if (message.kind == MessageKindEntry) {
      switch (message.as.entry.kind) {
      case EntryKindError: DA_APPEND(editor->errors,   message.as.entry); break;
      case EntryKindWarn:  DA_APPEND(editor->warnings, message.as.entry); break;
      case EntryKindInfo:  DA_APPEND(editor->infos,    message.as.entry); break;
      }
    }
  }

  for (u32 i = 0; i < editor->main_buffers.len; ++i)
    main_buffer_rebuild_entries(editor->main_buffers.items + i, editor);

  return CnsResultOk;
}

static void disconnected(CnsCtx *ctx, CnsConnection *connection) {
  (void) connection;

  Editor *editor = cns_get_user_data(ctx);
  editor->jwrap = NULL;
}

i32 main(i32 argc, char **argv) {
  Winx *winx = winx_init();
  WinxWindow *window = winx_init_window(winx, STR_LIT("Jet"),
                                        1600, 900,
                                        WinxGraphicsModeVulkan,
                                        NULL);
  window->target_fps = winx_get_refresh_rate(window);

  VikInstance *instance = vik_make_instance(window, VikRequestFlagsNone, false);
  VikExecutor *executor = vik_make_executor(instance);

  Atlas atlas = atlas_make(instance, fonts_MonaspaceNeon_Regular_otf);

  TextRenderer tr = tr_make(window, instance, executor,
                            &atlas,
                            build_shaders_text_vert_spv,
                            build_shaders_text_frag_spv,
                            build_shaders_sel_vert_spv,
                            build_shaders_sel_frag_spv);
  tr_resize(&tr, window->width, window->height);

  TextRenderer str = tr_make(window, instance, executor,
                             &atlas,
                             build_shaders_text_vert_spv,
                             build_shaders_text_frag_spv,
                             build_shaders_sel_vert_spv,
                             build_shaders_sel_frag_spv);
  tr_resize(&str, window->width, window->height);

  TextRenderer ptr = tr_make(window, instance, executor,
                             &atlas,
                             build_shaders_text_vert_spv,
                             build_shaders_text_frag_spv,
                             build_shaders_sel_vert_spv,
                             build_shaders_sel_frag_spv);
  tr_resize(&ptr, window->width, window->height);

  ShapeRenderer sr = sr_make(instance, executor,
                            build_shaders_shape_vert_spv,
                            build_shaders_shape_frag_spv,
                            build_shaders_circle_vert_spv,
                            build_shaders_circle_frag_spv);
  sr_resize(&sr, window->width, window->height);

  Editor editor = {0};

  MainBuffer main_buffer = main_buffer_make(NULL);
  DA_APPEND(editor.main_buffers, main_buffer);
  editor.palette_buffer = buffer_make();
  editor.mode = JetModeEditor;
  editor.font_scale = DEFAULT_FONT_SCALE;

  if (argc > 1) {
    buffer_read_file(editor_current_buffer(&editor), argv[1]);
    editor.main_buffers.items[0].file_path = strdup(argv[1]);
    editor_build_completions(&editor, 0);

    for (u32 i = 2; i < (u32) argc; ++i) {
      MainBuffer main_buffer = main_buffer_make(NULL);
      buffer_read_file(&main_buffer.buffer, argv[i]);
      main_buffer.file_path = strdup(argv[i]);
      DA_APPEND(editor.main_buffers, main_buffer);
    }
  }

  bool is_running = true;
  bool is_shift_pressed = false;
  bool is_ctrl_pressed = false;
  bool is_alt_pressed = false;

  tr_set_bg_color(&tr, BG_COLOR);
  tr_set_fg_color(&tr, FG_COLOR);
  tr_set_sel_color(&tr, ACC_COLOR);

  tr_set_sel_color(&ptr, ACC_COLOR);

  // Network initialization for jwrap integration
  CnsCtx *cns = cns_create();
  cns_set_user_data(cns, &editor);

  char socket_path[512];
  snprintf(socket_path, sizeof(socket_path), "%s%u",
           get_socket_path_prefix(), get_process_id());

  CnsListenInfo listen_info = {
    .proto = CnsProtoUnix,
    .receive_timeout = 1,
    .connected_cb = connected,
    .data_cb = data,
    .disconnected_cb = disconnected,
  };
  CnsError cns_error = cns_unix_listen(cns, socket_path, &listen_info);
  bool is_server_active = cns_error == CnsErrorOk;
  if (!is_server_active)
    WARN("Failed to create server: %s\n", cns_get_error_str(cns_error));

  while (is_running) {
    WinxEvent event;
    while ((event = winx_get_event(window, false)).kind != WinxEventKindNone) {
      is_running &= event.kind != WinxEventKindQuit;
      if (!is_running)
        break;

      switch (event.kind) {
      case WinxEventKindResize: {
        tr_resize(&tr, event.as.resize.width, event.as.resize.height);
        tr_resize(&str, event.as.resize.width, event.as.resize.height);
        tr_resize(&ptr, event.as.resize.width, event.as.resize.height);
        sr_resize(&sr, event.as.resize.width, event.as.resize.height);
      } break;

      case WinxEventKindKeyPress:
      case WinxEventKindKeyHold: {
        switch (event.as.key.key_code) {
        case WinxKeyCodeLeftShift: {
          is_shift_pressed = true;
        } break;

        case WinxKeyCodeLeftControl: {
          is_ctrl_pressed = true;
        } break;

        case WinxKeyCodeLeftAlt: {
          is_alt_pressed = true;
        } break;

        case WinxKeyCodeEnter: {
          if (!is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              if (editor.completing &&
                  editor.completions_scroll < editor.actual_completions.len) {
                WideStr line = buffer_get_current_line(&CURRENT_BUFFER());
                WideStr word = buffer_get_word_before_cursor(&CURRENT_BUFFER());
                Completion *completion =
                  editor.actual_completions.items + editor.completions_scroll;
                while (CURRENT_BUFFER().cursor_col < line.len &&
                       is_part_of_word(line.ptr[CURRENT_BUFFER().cursor_col]))
                  ++CURRENT_BUFFER().cursor_col;
                for (u32 i = 0; i < word.len; ++i)
                  buffer_remove_before_cursor(&CURRENT_BUFFER());
                for (u32 i = 0; i < completion->wsb.len; ++i)
                  buffer_insert(&CURRENT_BUFFER(), completion->wsb.items[i]);
                editor.completing = false;
              } else {
                editor_update_inline_errors_before_action(&editor, ActionAddLine);
                editor_update_completions_before_action(&editor, ActionAddLine, 0);
                buffer_insert_new_line(&CURRENT_BUFFER());
              }
            } else if (editor.mode == JetModeCommandPalette) {
              if (editor.selected_option < editor.options.len) {
                if (editor.provider->execute(&editor, editor.options, editor.selected_option)) {
                  editor.mode = JetModeEditor;
                } else {
                  buffer_remove_line(&editor.palette_buffer);
                }
              }
            }
          }
        } break;

        case WinxKeyCodeTab: {
          if (!is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              if (editor.completing &&
                  editor.completions_scroll < editor.actual_completions.len) {
                if (is_shift_pressed) {
                  if (editor.completions_scroll > 0)
                    --editor.completions_scroll;
                  else if (editor.actual_completions.len == 0)
                    editor.completions_scroll = 0;
                  else
                    editor.completions_scroll = editor.actual_completions.len - 1;
                } else {
                  if (editor.completions_scroll + 1 < editor.actual_completions.len)
                    ++editor.completions_scroll;
                  else
                    editor.completions_scroll = 0;
                }
              } else {
                for (u32 i = 0; i < SPACES_PER_TAB; ++i)
                  buffer_insert(editor_current_buffer(&editor), ' ');
              }
            } else if (editor.mode == JetModeCommandPalette) {
              if (editor.selected_option < editor.options.len) {
                buffer_remove_line(editor_current_buffer(&editor));
                PaletteOption *option = editor.options.items + editor.selected_option;
                for (u32 i = 0; i < option->name.len; ++i)
                  buffer_insert(editor_current_buffer(&editor), option->name.ptr[i]);
                editor.selected_option = 0;
              }
            }
          }
        } break;

        case WinxKeyCodeBackspace: {
          if (CURRENT_BUFFER().cursor_col == 0) {
            editor_update_inline_errors_before_action(&editor, ActionRemoveLineBeforeCursor);
            editor_update_completions_before_action(&editor, ActionRemoveLineBeforeCursor, 0);
          } else {
            u32 amount;
            if (is_ctrl_pressed)
              amount =
                buffer_get_chars_amount_for_remove_word_before_cursor(&CURRENT_BUFFER());
            else
              amount = 1;

            editor_update_inline_errors_before_action(&editor, ActionRemoveBeforeCursor);
            editor_update_completions_before_action(&editor, ActionRemoveBeforeCursor, amount);
          }

          if (is_ctrl_pressed)
            buffer_remove_word_before_cursor(editor_current_buffer(&editor));
          else
            buffer_remove_before_cursor(editor_current_buffer(&editor));

          editor_update_current_row_completions_begin(&editor);
        } break;

        case WinxKeyCodeDelete: {
          WideStr line = buffer_get_current_line(&CURRENT_BUFFER());
          if (CURRENT_BUFFER().cursor_col == line.len) {
            editor_update_inline_errors_before_action(&editor, ActionRemoveLineAfterCursor);
            editor_update_completions_before_action(&editor, ActionRemoveLineAfterCursor, 0);
          } else {
            u32 amount;
            if (is_ctrl_pressed)
              amount =
                buffer_get_chars_amount_for_remove_word_at_cursor(&CURRENT_BUFFER());
            else
              amount = 1;

            editor_update_inline_errors_before_action(&editor, ActionRemoveAfterCursor);
            editor_update_completions_before_action(&editor, ActionRemoveAfterCursor, amount);
          }

          if (is_ctrl_pressed)
            buffer_remove_word_at_cursor(editor_current_buffer(&editor));
          else
            buffer_remove_at_cursor(editor_current_buffer(&editor));
        } break;

        case WinxKeyCodeLeft: {
          if (is_ctrl_pressed) {
            if (is_alt_pressed) {
              if (editor.mode == JetModeEditor) {
                if (editor.current_main_buffer_index > 0)
                  --editor.current_main_buffer_index;
                else
                  editor.current_main_buffer_index = editor.main_buffers.len - 1;
                editor.completing = false;
              }
            } else {
              buffer_move_left_word(editor_current_buffer(&editor), is_shift_pressed);
            }
          } else {
            buffer_move_left(editor_current_buffer(&editor), is_shift_pressed);
          }

          editor.completing = false;
        } break;

        case WinxKeyCodeRight: {
          if (is_ctrl_pressed) {
            if (is_alt_pressed) {
              if (editor.mode == JetModeEditor) {
                if (editor.current_main_buffer_index + 1 < editor.main_buffers.len)
                  ++editor.current_main_buffer_index;
                else
                  editor.current_main_buffer_index = 0;
                editor.completing = false;
              }
            } else {
              buffer_move_right_word(editor_current_buffer(&editor), is_shift_pressed);
            }
          } else {
            buffer_move_right(editor_current_buffer(&editor), is_shift_pressed);
          }

          editor.completing = false;
        } break;

        case WinxKeyCodeDown: {
          if (editor.mode == JetModeEditor) {
            if (is_ctrl_pressed) {
              if (is_alt_pressed) {
                u32 len = editor.errors.len + editor.warnings.len + editor.infos.len;
                if (editor.entry_cursor == (u32) -1) {
                  editor.entry_cursor = 0;
                  if (editor.entry_cursor < len)
                    editor_go_to_entry(&editor, editor_get_entry(&editor, editor.entry_cursor));
                } else if (editor.entry_cursor + 1 < len) {
                  ++editor.entry_cursor;
                  editor_go_to_entry(&editor, editor_get_entry(&editor, editor.entry_cursor));
                }
              } else {
                buffer_move_down_paragraph(editor_current_buffer(&editor), is_shift_pressed);
              }
            } else {
              if (editor.completing &&
                  editor.completions_scroll < editor.actual_completions.len) {
                if (editor.completions_scroll + 1 < editor.actual_completions.len)
                  ++editor.completions_scroll;
                else
                  editor.completions_scroll = 0;
              } else {
                WideStr line = buffer_get_current_line(&CURRENT_BUFFER());
                u32 max_visual_line_len = get_max_visual_line_len(line, window, &tr);
                buffer_move_down(editor_current_buffer(&editor),
                                 is_shift_pressed,
                                 max_visual_line_len);
              }
            }
          } else if (editor.mode == JetModeCommandPalette) {
            if (editor.selected_option + 1 < editor.options.len)
              ++editor.selected_option;
            else
              editor.selected_option = 0;
          }
        } break;

        case WinxKeyCodeUp: {
          if (editor.mode == JetModeEditor) {
            if (is_ctrl_pressed) {
              if (is_alt_pressed) {
                if (editor.entry_cursor == (u32) -1) {
                  u32 len = editor.errors.len + editor.warnings.len + editor.infos.len;
                  editor.entry_cursor = len == 0 ? 0 : len - 1;
                  if (editor.entry_cursor < len)
                    editor_go_to_entry(&editor, editor_get_entry(&editor, editor.entry_cursor));
                } else if (editor.entry_cursor > 0) {
                  --editor.entry_cursor;
                  editor_go_to_entry(&editor, editor_get_entry(&editor, editor.entry_cursor));
                }
              } else {
                buffer_move_up_paragraph(editor_current_buffer(&editor), is_shift_pressed);
              }
            } else {
              if (editor.completing &&
                  editor.completions_scroll < editor.actual_completions.len) {
                if (editor.completions_scroll > 0)
                  --editor.completions_scroll;
                else if (editor.actual_completions.len == 0)
                  editor.completions_scroll = 0;
                else
                  editor.completions_scroll = editor.actual_completions.len - 1;
              } else {
                WideStr line = buffer_get_current_line(&CURRENT_BUFFER());
                u32 max_visual_line_len = get_max_visual_line_len(line, window, &tr);
                buffer_move_up(editor_current_buffer(&editor),
                               is_shift_pressed,
                               max_visual_line_len);
              }
            }
          } else if (editor.mode == JetModeCommandPalette) {
            if (editor.selected_option > 0)
              --editor.selected_option;
            else if (editor.options.len > 0)
              editor.selected_option = editor.options.len - 1;
          }
        } break;

        case WinxKeyCodeEqual: {
          if (is_ctrl_pressed && editor.font_scale < MAX_FONT_SCALE)
            ++editor.font_scale;
        } break;

        case WinxKeyCodeMinus: {
          if (is_ctrl_pressed && editor.font_scale > MIN_FONT_SCALE)
            --editor.font_scale;
        } break;

        case WinxKeyCodeP: {
          if (is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              editor.provider = &command_provider;
              editor.selected_option = 0;
              editor.mode = JetModeCommandPalette;
              editor.completing = false;
              buffer_remove_line(editor_current_buffer(&editor));
            } else if (editor.mode == JetModeCommandPalette) {
              editor.mode = JetModeEditor;
            }
          }
        } break;

        case WinxKeyCodeA: {
          if (is_ctrl_pressed)
            buffer_goto_line_begin(editor_current_buffer(&editor), is_shift_pressed);
          else if (is_alt_pressed)
            buffer_goto_buffer_begin(editor_current_buffer(&editor), is_shift_pressed);
        } break;

        case WinxKeyCodeE: {
          if (is_ctrl_pressed)
            buffer_goto_line_end(editor_current_buffer(&editor), is_shift_pressed);
          else if (is_alt_pressed)
            buffer_goto_buffer_end(editor_current_buffer(&editor), is_shift_pressed);
        } break;

        case WinxKeyCodeW: {
          if (is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              editor.provider = &error_provider;
              editor.selected_option = editor.entry_cursor;
              editor.mode = JetModeCommandPalette;
              editor.completing = false;
              buffer_remove_line(editor_current_buffer(&editor));
            } else if (editor.mode == JetModeCommandPalette) {
              editor.mode = JetModeEditor;
            }
          }
        } break;

        case WinxKeyCodeO: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            editor.provider = &open_file_provider;
            editor.selected_option = 0;
            editor.mode = JetModeCommandPalette;
            editor.completing = false;
            buffer_remove_line(editor_current_buffer(&editor));
          }
        } break;

        case WinxKeyCodeS: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor && CURRENT_BUFFER().is_dirty) {
            if (is_shift_pressed || !CURRENT_FILE_PATH()) {
              editor.provider = &save_file_provider;
              editor.selected_option = 0;
              editor.mode = JetModeCommandPalette;
              editor.completing = false;
              buffer_remove_line(editor_current_buffer(&editor));
            } else {
              buffer_write_file(editor_current_buffer(&editor), CURRENT_FILE_PATH());

              if (editor.jwrap) {
                ByteBuffer buffer = {0};
                Message message = { MessageKindRerun, {} };
                encode_message(&buffer, &message);
                cns_unix_send(editor.jwrap, buffer.items, buffer.len);
                free(buffer.items);
              }
            }
          }
        } break;

        case WinxKeyCodeN: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            editor.current_main_buffer_index = editor.main_buffers.len;
            MainBuffer main_buffer = main_buffer_make(NULL);
            DA_APPEND(editor.main_buffers, main_buffer);
          }
        } break;

        case WinxKeyCodeQ: {
          if (is_ctrl_pressed)
            is_running = false;
        } break;

        case WinxKeyCodeK: {
          if (is_ctrl_pressed) {
            editor_remove_invalidated_completions(&editor, CURRENT_ABS_FILE_PATH());
            buffer_delete(&CURRENT_BUFFER());
            if (CURRENT_FILE_PATH())
              free(CURRENT_FILE_PATH());
            if (CURRENT_ABS_FILE_PATH().ptr)
              free(CURRENT_ABS_FILE_PATH().ptr);
            if (CURRENT_ERRORS().items)
              free(CURRENT_ERRORS().items);
            if (CURRENT_WARNINGS().items)
              free(CURRENT_WARNINGS().items);
            if (CURRENT_INFOS().items)
              free(CURRENT_INFOS().items);
            DA_REMOVE_AT(editor.main_buffers, editor.current_main_buffer_index);
            if (editor.main_buffers.len == 0) {
              MainBuffer main_buffer = main_buffer_make(NULL);
              DA_APPEND(editor.main_buffers, main_buffer);
            }
            if (editor.current_main_buffer_index > 0)
              --editor.current_main_buffer_index;
          }
        } break;

        case WinxKeyCodeEscape: {
          if (editor.mode == JetModeEditor)
            editor.completing = false;
          else if (editor.mode == JetModeCommandPalette)
            editor.mode = JetModeEditor;
        } break;

        case WinxKeyCodeSpace: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            editor.completing = true;
            editor.completions_scroll = 0;
          }
        } break;

        default: break;
        }
      } break;

      case WinxEventKindKeyRelease: {
        if (event.as.key.key_code == WinxKeyCodeLeftShift)
          is_shift_pressed = false;
        else if (event.as.key.key_code == WinxKeyCodeLeftControl)
          is_ctrl_pressed = false;
        else if (event.as.key.key_code == WinxKeyCodeLeftAlt)
          is_alt_pressed = false;
      } break;

      case WinxEventKindChar: {
        if (!is_ctrl_pressed && !is_alt_pressed) {
          editor_update_inline_errors_before_action(&editor, ActionAdd);
          editor_update_completions_before_action(&editor, ActionAdd,
                                                  event.as._char._char);

          buffer_insert(editor_current_buffer(&editor), event.as._char._char);
#if AUTOCOMPLETION
          if (editor.mode == JetModeEditor) {
            if (!editor.completing)
              editor.completions_scroll = 0;
            editor.completing = true;
          }
#endif
        }
      } break;

      default: break;
      }
    }

    editor_update_current_row_completions_begin(&editor);

    if (CURRENT_BUFFER().is_selecting)
      editor.completing = false;

    // Update actual completions
    if (editor.completing) {
      WideStr word = buffer_get_word_before_cursor(&CURRENT_BUFFER());
      if (word.len >= MINIMAL_COMPLETION_PREFIX_LENGTH) {
        editor.actual_completions.len = 0;
        for (u32 i = 0; i < editor.completions.len; ++i) {
          Completion *completion = editor.completions.items + i;
          WideStr completion_wstr = { completion->wsb.items, completion->wsb.len };
          if (wide_str_begins_with(completion_wstr, word) &&
              completion_wstr.len > word.len) {
            bool already_exists = false;
            for (u32 j = 0; j < editor.actual_completions.len; ++j) {
              Completion *actual_completion = editor.actual_completions.items + j;
              WideStr actual_completion_wstr =
                { actual_completion->wsb.items, actual_completion->wsb.len };
              if (wide_str_eq(actual_completion_wstr, completion_wstr)) {
                already_exists = true;
                break;
              }
            }
            if (!already_exists)
              DA_APPEND(editor.actual_completions, *completion);
          }
        }

        if (editor.completions_scroll >= editor.actual_completions.len)
          editor.completions_scroll = 0;
      }
    }

    cns_step(cns, 1);

    if (!vik_begin_frame(executor, BG_COLOR, 1.0)) {
      winx_draw(window);
      continue;
    }
    {
      u32 min_row, min_col, max_row, max_col;
      get_buffer_selection_bounds(&editor.palette_buffer,
                                  &min_row, &min_col,
                                  &max_row, &max_col);
      tr_begin_frame(&ptr, editor.font_scale,
                     min_row, min_col,
                     max_row, max_col);
    }
    sr_begin_frame(&sr);
    tr_begin_frame(&str, editor.font_scale, 0, 0, 0, 0);
    {
      u32 min_row, min_col, max_row, max_col;
      get_buffer_selection_bounds(&CURRENT_BUFFER(),
                                  &min_row, &min_col,
                                  &max_row, &max_col);
      tr_begin_frame(&tr, editor.font_scale,
                     min_row, min_col,
                     max_row, max_col);
    }

    // Rendering main buffer
    {
      tr_set_bg_color(&tr, BG_COLOR);
      tr_set_fg_color(&tr, FG_COLOR);

      tr.x_lower_limit = 0.0;
      tr.x_higher_limit = window->width;

      if (CURRENT_SCROLL() > CURRENT_BUFFER().cursor_row)
        CURRENT_SCROLL() = CURRENT_BUFFER().cursor_row;

      tr.line_index = CURRENT_SCROLL();

      u32 space = U' ';
      f32 space_width = tr_measure_text(&tr, &space, 1);

      u32 visible_lines = 0;

      f32 y = BUFFER_PADDING;
      for (u32 i = CURRENT_SCROLL(); i < CURRENT_BUFFER().lines.len; ++i) {
        WideStr line = buffer_get_line(&CURRENT_BUFFER(), i);
        f32 x;
        f32 new_y = tr_draw_line(&tr, line.ptr, line.len,
                                 BUFFER_PADDING, y,
                                 window->width - BUFFER_PADDING * 2.0,
                                 &x);
        if (new_y >= window->height - (f32) editor.font_scale - BUFFER_PADDING * 3.0)
          break;
        y = new_y;
        ++visible_lines;

        // Inline error reporting
        if (i < CURRENT_ERRORS().len && CURRENT_ERRORS().items[i]) {
          Str *message = &CURRENT_ERRORS().items[i]->message;
          u32 *text = malloc(message->len * sizeof(*text));
          for (u32 j = 0; j < message->len; ++j)
            text[j] = message->ptr[j];
          tr_set_fg_color(&tr, ERROR_COLOR);
          tr_draw_text(&tr, text,
                       message->len, x + space_width * INLINE_ERROR_OFFSET_MULTIPLIER,
                       new_y - (f32) editor.font_scale);
          tr_set_fg_color(&tr, FG_COLOR);
          free(text);
        } else if (i < CURRENT_WARNINGS().len && CURRENT_WARNINGS().items[i]) {
          Str *message = &CURRENT_WARNINGS().items[i]->message;
          u32 *text = malloc(message->len * sizeof(*text));
          for (u32 j = 0; j < message->len; ++j)
            text[j] = message->ptr[j];
          tr_set_fg_color(&tr, WARN_COLOR);
          tr_draw_text(&tr, text,
                       message->len, x + space_width * INLINE_ERROR_OFFSET_MULTIPLIER,
                       new_y - (f32) editor.font_scale);
          tr_set_fg_color(&tr, FG_COLOR);
          free(text);
        } else if (i < CURRENT_INFOS().len && CURRENT_INFOS().items[i]) {
          Str *message = &CURRENT_INFOS().items[i]->message;
          u32 *text = malloc(message->len * sizeof(*text));
          for (u32 j = 0; j < message->len; ++j)
            text[j] = message->ptr[j];
          tr_set_fg_color(&tr, INFO_COLOR);
          tr_draw_text(&tr, text,
                       message->len, x + space_width * INLINE_ERROR_OFFSET_MULTIPLIER,
                       new_y - (f32) editor.font_scale);
          tr_set_fg_color(&tr, FG_COLOR);
          free(text);
        }
      }

      if (CURRENT_SCROLL() + visible_lines < CURRENT_BUFFER().cursor_row + 1)
        CURRENT_SCROLL() = CURRENT_BUFFER().cursor_row - visible_lines + 1;
    }

    if (editor.mode == JetModeEditor) {
      if (editor.completing && !CURRENT_BUFFER().is_selecting) {
        WideStr word = buffer_get_word_before_cursor(&CURRENT_BUFFER());
        if (word.len >= MINIMAL_COMPLETION_PREFIX_LENGTH) {
          if (editor.actual_completions.len > 0) {
            // Rendering completion options

            if (editor.completions_scroll >= editor.actual_completions.len)
              editor.completions_scroll = editor.actual_completions.len;

            f32 x = tr.sel_end_x;
            f32 y = tr.sel_end_y + (f32) editor.font_scale;
            f32 width =
              MAX_COMPLETIONS_WINDOW_WIDTH +
              COMPLETIONS_WINDOW_BORDER_WIDTH * 2.0;
            f32 height =
              (f32) COMPLETIONS_WINDOW_CAPACITY *
              (f32) editor.font_scale +
              BUFFER_PADDING +
              COMPLETIONS_WINDOW_BORDER_WIDTH * 2.0;

            bool y_inverse =
              y + height >
              window->height - ((f32) editor.font_scale + BUFFER_PADDING);

            if (y_inverse)
              y -= height + (f32) editor.font_scale;

            sr_draw_rect(&sr, x, y, width, height, FG_COLOR, 1.0);

            x += COMPLETIONS_WINDOW_BORDER_WIDTH;
            y += COMPLETIONS_WINDOW_BORDER_WIDTH;
            width -= COMPLETIONS_WINDOW_BORDER_WIDTH * 2.0;
            height -= COMPLETIONS_WINDOW_BORDER_WIDTH * 2.0;

            sr_draw_rect(&sr, x, y, width, height, BG_COLOR, 1.0);

            f32 y_limit = y + height;

            y += BUFFER_PADDING * 0.5;

            sr_draw_rect(&sr, x, y, width, (f32) editor.font_scale, FG_COLOR, 1.0);

            x += BUFFER_PADDING * 0.5;

            ptr.x_lower_limit = x;
            ptr.x_higher_limit = x + width;

            tr_set_bg_color(&ptr, FG_COLOR);
            tr_set_fg_color(&ptr, BG_COLOR);

            for (u32 i = editor.completions_scroll;
                 i < editor.actual_completions.len && y <= y_limit;
                 ++i) {
              Completion *completion = editor.actual_completions.items + i;
              tr_draw_text(&ptr, completion->wsb.items, completion->wsb.len, x, y);
              y += (f32) editor.font_scale;

              if (i == editor.completions_scroll) {
                tr_set_bg_color(&ptr, BG_COLOR);
                tr_set_fg_color(&ptr, FG_COLOR);
              }
            }
          }
        }
      }
    } else if (editor.mode == JetModeCommandPalette) {
      // Rendering command palette
      WideStr line = buffer_get_current_line(editor_current_buffer(&editor));

      tr_set_bg_color(&ptr, BG_COLOR);
      tr_set_fg_color(&ptr, FG_COLOR);

      ptr.x_lower_limit =
        window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 +
        PALETTE_BORDER_WIDTH + BUFFER_PADDING;
      ptr.x_higher_limit =
        ptr.x_lower_limit +
        window->width * PALETTE_WIDTH_FACTOR -
        PALETTE_BORDER_WIDTH * 2.0 - BUFFER_PADDING * 2.0;

      // Shadow
      sr_draw_rounded_rect(&sr,
                           window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + PALETTE_SHADOW_OFFSET,
                           window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + PALETTE_SHADOW_OFFSET,
                           window->width * PALETTE_WIDTH_FACTOR,
                           window->height * PALETTE_HEIGHT_FACTOR,
                           PALETTE_BORDER_RADIUS, SHADOW_COLOR, true);
      // Border
      sr_draw_rounded_rect(&sr,
                           window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                           window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5,
                           window->width * PALETTE_WIDTH_FACTOR,
                           window->height * PALETTE_HEIGHT_FACTOR,
                           PALETTE_BORDER_RADIUS, FG_COLOR,
                           PALETTE_ALPHA == 1.0 ? PALETTE_ALPHA : PALETTE_ALPHA / 2.0,
                           false);
      // Inner
      sr_draw_rounded_rect(&sr,
                           window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + PALETTE_BORDER_WIDTH,
                           window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + PALETTE_BORDER_WIDTH,
                           window->width * PALETTE_WIDTH_FACTOR - PALETTE_BORDER_WIDTH * 2.0,
                           window->height * PALETTE_HEIGHT_FACTOR - PALETTE_BORDER_WIDTH * 2.0,
                           PALETTE_BORDER_RADIUS - PALETTE_BORDER_WIDTH, BG_COLOR, PALETTE_ALPHA,
                           false);
      // Separator
      sr_draw_rect(&sr,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING * 2.0 + PALETTE_BORDER_WIDTH + (f32) editor.font_scale,
                   window->width * PALETTE_WIDTH_FACTOR,
                   PALETTE_BORDER_WIDTH,
                   FG_COLOR, PALETTE_ALPHA);

      f32 line0_width =
        tr_measure_text(&ptr, line.ptr, editor.palette_buffer.cursor_col);

      f32 line1_width;
      if (editor.palette_buffer.cursor_col < line.len) {
        line1_width =
          tr_measure_text(&ptr, line.ptr, editor.palette_buffer.cursor_col + 1);
      } else {
        u32 space = U' ';
        line1_width =
          tr_measure_text(&ptr, line.ptr, editor.palette_buffer.cursor_col) +
          tr_measure_text(&ptr, &space, 1);
      }

      if (line0_width - editor.palette_scroll_x < 0.0) {
        editor.palette_scroll_x = line0_width;
      } else if (line1_width - editor.palette_scroll_x >
                 window->width * PALETTE_WIDTH_FACTOR -
                 PALETTE_BORDER_WIDTH * 2.0 -
                 BUFFER_PADDING * 2.0) {
        editor.palette_scroll_x =
          line1_width -
          (window->width * PALETTE_WIDTH_FACTOR -
           PALETTE_BORDER_WIDTH * 2.0 -
           BUFFER_PADDING * 2.0);
      }

      tr_draw_line(&ptr, line.ptr, line.len,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH - editor.palette_scroll_x,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH,
                   INFINITY, NULL);

      editor.provider->free_opts(editor.options);
      editor.options = editor.provider->get_opts(&editor, line);

      if (editor.palette_scroll_y > editor.selected_option)
        editor.palette_scroll_y = editor.selected_option;

      for (u32 i = editor.palette_scroll_y; i < editor.options.len; ++i) {
        PaletteOption *option = editor.options.items + i;
        f32 x = window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH;
        f32 y = window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING * 3.0 + PALETTE_BORDER_WIDTH + (f32) editor.font_scale * (i + 1);

        if (window->height - y - (f32) editor.font_scale < window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH)
          break;

        if (i == editor.selected_option) {
          tr_set_bg_color(&ptr, option->fg_r, option->fg_g, option->fg_b);
          tr_set_fg_color(&ptr, option->bg_r, option->bg_g, option->bg_b);

          sr_draw_rect(&sr,
                       window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + PALETTE_BORDER_WIDTH,
                       y,
                       window->width * PALETTE_WIDTH_FACTOR - PALETTE_BORDER_WIDTH * 2.0,
                       (f32) editor.font_scale,
                       ACC_COLOR, PALETTE_ALPHA);
        } else {
          tr_set_bg_color(&ptr, option->bg_r, option->bg_g, option->bg_b);
          tr_set_fg_color(&ptr, option->fg_r, option->fg_g, option->fg_b);
        }

        tr_draw_text(&ptr, option->name.ptr, option->name.len, x, y);
      }
    }

    // Rendering status bar
    {
      str.x_lower_limit = 0.0;
      str.x_higher_limit = window->width;

      tr_set_bg_color(&str, ALT_BG_COLOR);
      tr_set_fg_color(&str, ALT_FG_COLOR);

      f32 y = window->height - ((f32) editor.font_scale + BUFFER_PADDING);

      sr_draw_rect(&sr,
                   0.0,
                   y,
                   window->width,
                   (f32) editor.font_scale + BUFFER_PADDING,
                   ALT_BG_COLOR, 1.0);

      y += BUFFER_PADDING * 0.5;

      u32 buffer[512];
      static_assert(sizeof(u32) == sizeof(wchar_t));
      u32 len = swprintf((i32 *) buffer, ARRAY_LEN(buffer), L"%u:%u",
                         CURRENT_BUFFER().cursor_row + 1,
                         CURRENT_BUFFER().cursor_col + 1);
      tr_draw_text(&str, buffer, len, BUFFER_PADDING, y);

      if (editor.jwrap ||
          editor.errors.len > 0 ||
          editor.warnings.len > 0 ||
          editor.infos.len > 0) {
        len = 0;
        if (editor.jwrap) {
          buffer[len++] = U'✔';
          buffer[len++] = U' ';
        }
        u32 len0 = len;
        static_assert(sizeof(u32) == sizeof(wchar_t));
        len += swprintf((i32 *) buffer + len, ARRAY_LEN(buffer) - len,
                        L"%u", editor.errors.len);
        u32 len1 = len;
        buffer[len++] = U':';
        u32 len2 = len;
        static_assert(sizeof(u32) == sizeof(wchar_t));
        len += swprintf((i32 *) buffer + len, ARRAY_LEN(buffer) - len,
                        L"%u", editor.warnings.len);
        u32 len3 = len;
        buffer[len++] = U':';
        u32 len4 = len;
        static_assert(sizeof(u32) == sizeof(wchar_t));
        len += swprintf((i32 *) buffer + len, ARRAY_LEN(buffer) - len,
                        L"%u", editor.infos.len);
        u32 len5 = len;

        f32 x = (window->width - tr_measure_text(&str, buffer, len)) * 0.5 - BUFFER_PADDING;
        tr_draw_text(&str, buffer, len0, x, y);
        tr_set_fg_color(&str, ERROR_COLOR);
        x += tr_measure_text(&str, buffer, len0);
        tr_draw_text(&str, buffer + len0, len1 - len0, x, y);
        tr_set_fg_color(&str, ALT_FG_COLOR);
        x += tr_measure_text(&str, buffer + len0, len2 - len1);
        tr_draw_text(&str, buffer + len1, len2 - len1, x, y);
        tr_set_fg_color(&str, WARN_COLOR);
        x += tr_measure_text(&str, buffer + len0, len3 - len2);
        tr_draw_text(&str, buffer + len2, len3 - len2, x, y);
        tr_set_fg_color(&str, ALT_FG_COLOR);
        x += tr_measure_text(&str, buffer + len0, len4 - len3);
        tr_draw_text(&str, buffer + len3, len4 - len3, x, y);
        tr_set_fg_color(&str, INFO_COLOR);
        x += tr_measure_text(&str, buffer + len0, len5 - len4);
        tr_draw_text(&str, buffer + len4, len5 - len4, x, y);
      }

      char *file_path = CURRENT_FILE_PATH();
      if (!file_path)
        file_path = "<no file>";
      len = 0;
      while (*file_path)
        buffer[len++] = *file_path++;
      if (CURRENT_BUFFER().is_dirty)
        buffer[len++] = U'*';
      tr_set_fg_color(&str, ALT_FG_COLOR);
      f32 x = window->width - tr_measure_text(&str, buffer, len) - BUFFER_PADDING;
      tr_draw_text(&str, buffer, len, x, y);
    }

    atlas_ensure_is_actual(&atlas);
    tr_end_frame(&tr);
    sr_end_frame(&sr);
    tr_end_frame(&str);
    tr_end_frame(&ptr);
    vik_end_frame(executor);

    winx_draw(window);
  }

  cns_destroy(cns);

  for (u32 i = 0; i < editor.completions.len; ++i)
    free(editor.completions.items[i].wsb.items);
  if (editor.completions.items)
    free(editor.completions.items);
  if (editor.actual_completions.items)
    free(editor.actual_completions.items);
  buffer_delete(&editor.palette_buffer);
  for (u32 i = 0; i < editor.main_buffers.len; ++i) {
    buffer_delete(&editor.main_buffers.items[i].buffer);
    if (editor.main_buffers.items[i].file_path)
      free(editor.main_buffers.items[i].file_path);
    if (editor.main_buffers.items[i].errors.items)
      free(editor.main_buffers.items[i].errors.items);
    if (editor.main_buffers.items[i].warnings.items)
      free(editor.main_buffers.items[i].warnings.items);
    if (editor.main_buffers.items[i].infos.items)
      free(editor.main_buffers.items[i].infos.items);
  }
  if (editor.main_buffers.items)
    free(editor.main_buffers.items);
  editor_clear_entries(&editor);

  sr_delete(&sr);
  tr_delete(&ptr);
  tr_delete(&str);
  tr_delete(&tr);
  atlas_delete(&atlas);
  vik_delete_executor(executor);
  vik_delete_instance(instance);

  winx_destroy_window(window);
  winx_cleanup(winx);

  return 0;
}
