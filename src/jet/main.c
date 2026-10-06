// TODO: scroll

#include "shl/shl-defs.h"
#include "viking/viking.h"
#include "winx/event.h"
#include "../../build/assets.c"
#include "text-renderer.h"
#include "shape-renderer.h"
#include "editor.h"
#include "buffer.h"
#include "provider.h"
// Customize your editor here!
#include "config.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

#define CURRENT_BUFFER()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].buffer

#define CURRENT_FILE_PATH()                                             \
  editor.main_buffers.items[editor.current_main_buffer_index].file_path

i32 main(i32 argc, char **argv) {
  Winx *winx = winx_init();
  WinxWindow *window = winx_init_window(winx, STR_LIT("Jet"),
                                        1600, 900,
                                        WinxGraphicsModeVulkan,
                                        NULL);
  window->target_fps = winx_get_refresh_rate(window);

  VikInstance *instance = vik_make_instance(window, VikRequestFlagsNone, false);
  VikExecutor *executor = vik_make_executor(instance);

  TextRenderer tr = tr_make(instance, executor,
                            fonts_MonaspaceNeon_Regular_otf,
                            build_shaders_text_vert_spv,
                            build_shaders_text_frag_spv,
                            build_shaders_sel_vert_spv,
                            build_shaders_sel_frag_spv);
  tr_resize(&tr, window->width, window->height);

  TextRenderer ptr = tr_make(instance, executor,
                             fonts_MonaspaceNeon_Regular_otf,
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

  MainBuffer main_buffer = { buffer_make(), NULL };
  DA_APPEND(editor.main_buffers, main_buffer);
  editor.palette_buffer = buffer_make();
  editor.current_buffer = &editor.main_buffers.items[0].buffer;

  editor.mode = JetModeEditor;

  editor.font_scale = DEFAULT_FONT_SCALE;

  if (argc > 1) {
    buffer_read_file(editor.current_buffer, argv[1]);
    editor.main_buffers.items[0].file_path = strdup(argv[1]);

    for (u32 i = 2; i < (u32) argc; ++i) {
      MainBuffer main_buffer = { buffer_make(), NULL };
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

  while (is_running) {
    WinxEvent event;
    while ((event = winx_get_event(window, false)).kind != WinxEventKindNone) {
      is_running &= event.kind != WinxEventKindQuit;
      if (!is_running)
        break;

      switch (event.kind) {
      case WinxEventKindResize: {
        tr_resize(&tr, event.as.resize.width, event.as.resize.height);
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
              buffer_insert_new_line(editor.current_buffer);
            } else if (editor.mode == JetModeCommandPalette) {
              if (editor.selected_option < editor.options.len) {
                if (editor.provider->execute(&editor, editor.selected_option)) {
                  editor.current_buffer = &editor.main_buffers.items[editor.current_main_buffer_index].buffer;
                  editor.mode = JetModeEditor;
                }
              }
            }
          }
        } break;

        case WinxKeyCodeTab: {
          if (!is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              for (u32 i = 0; i < SPACES_PER_TAB; ++i)
                buffer_insert(editor.current_buffer, ' ');
            } else if (editor.mode == JetModeCommandPalette) {
              if (editor.selected_option < editor.options.len) {
                buffer_delete_line(editor.current_buffer);
                WideStr *option = editor.options.items + editor.selected_option;
                for (u32 i = 0; i < option->len; ++i)
                  buffer_insert(editor.current_buffer, option->ptr[i]);
                editor.selected_option = 0;
              }
            }
          }
        } break;

        case WinxKeyCodeBackspace: {
          if (is_ctrl_pressed)
            buffer_remove_word_before_cursor(editor.current_buffer);
          else
            buffer_remove_before_cursor(editor.current_buffer);
        } break;

        case WinxKeyCodeDelete: {
          if (is_ctrl_pressed)
            buffer_remove_word_at_cursor(editor.current_buffer);
          else
            buffer_remove_at_cursor(editor.current_buffer);
        } break;

        case WinxKeyCodeLeft: {
          if (is_ctrl_pressed) {
            if (is_alt_pressed) {
              if (editor.mode == JetModeEditor) {
                if (editor.current_main_buffer_index > 0)
                  --editor.current_main_buffer_index;
                else
                  editor.current_main_buffer_index = editor.main_buffers.len - 1;
              }
            } else {
              buffer_move_left_word(editor.current_buffer);
            }
          } else {
            buffer_move_left(editor.current_buffer);
          }
        } break;

        case WinxKeyCodeRight: {
          if (is_ctrl_pressed) {
            if (is_alt_pressed) {
              if (editor.mode == JetModeEditor) {
                if (editor.current_main_buffer_index + 1 < editor.main_buffers.len)
                  ++editor.current_main_buffer_index;
                else
                  editor.current_main_buffer_index = 0;
              }
            } else {
              buffer_move_right_word(editor.current_buffer);
            }
          } else {
            buffer_move_right(editor.current_buffer);
          }
        } break;

        case WinxKeyCodeDown: {
          if (editor.mode == JetModeEditor) {
            if (is_ctrl_pressed)
              buffer_move_down_paragraph(editor.current_buffer);
            else
              buffer_move_down(editor.current_buffer);
          } else if (editor.mode == JetModeCommandPalette) {
            if (editor.selected_option + 1 < editor.options.len)
              ++editor.selected_option;
            else
              editor.selected_option = 0;
          }
        } break;

        case WinxKeyCodeUp: {
          if (editor.mode == JetModeEditor) {
            if (is_ctrl_pressed)
              buffer_move_up_paragraph(editor.current_buffer);
            else
              buffer_move_up(editor.current_buffer);
          } else if (editor.mode == JetModeCommandPalette) {
            if (editor.selected_option > 0)
              --editor.selected_option;
            else if (editor.options.len > 0)
              editor.selected_option = editor.options.len - 1;
          }
        } break;

        case WinxKeyCodeEqual: {
          if (is_ctrl_pressed && editor.font_scale < MAX_FONT_SCALE)
            editor.font_scale += 1.0;
        } break;

        case WinxKeyCodeMinus: {
          if (is_ctrl_pressed && editor.font_scale > MIN_FONT_SCALE)
            editor.font_scale -= 1.0;
        } break;

        case WinxKeyCodeP: {
          if (is_ctrl_pressed) {
            if (editor.mode == JetModeEditor) {
              editor.current_buffer = &editor.palette_buffer;
              editor.provider = &command_provider;
              editor.selected_option = 0;
              editor.mode = JetModeCommandPalette;
              buffer_delete_line(editor.current_buffer);
            } else if (editor.mode == JetModeCommandPalette) {
              editor.current_buffer = &editor.main_buffers.items[editor.current_main_buffer_index].buffer;
              editor.mode = JetModeEditor;
            }
          }
        } break;

        case WinxKeyCodeA: {
          if (is_ctrl_pressed)
            buffer_goto_line_begin(editor.current_buffer);
          else if (is_alt_pressed)
            buffer_goto_buffer_begin(editor.current_buffer);
        } break;

        case WinxKeyCodeE: {
          if (is_ctrl_pressed)
            buffer_goto_line_end(editor.current_buffer);
          else if (is_alt_pressed)
            buffer_goto_buffer_end(editor.current_buffer);
        } break;

        case WinxKeyCodeO: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            editor.current_buffer = &editor.palette_buffer;
            editor.provider = &open_file_provider;
            editor.selected_option = 0;
            editor.mode = JetModeCommandPalette;
            buffer_delete_line(editor.current_buffer);
          }
        } break;

        case WinxKeyCodeS: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            if (is_shift_pressed || !CURRENT_FILE_PATH()) {
              editor.current_buffer = &editor.palette_buffer;
              editor.provider = &save_file_provider;
              editor.selected_option = 0;
              editor.mode = JetModeCommandPalette;
              buffer_delete_line(editor.current_buffer);
            } else {
              buffer_write_file(editor.current_buffer, CURRENT_FILE_PATH());
            }
          }
        } break;

        case WinxKeyCodeN: {
          if (is_ctrl_pressed && editor.mode == JetModeEditor) {
            editor.current_main_buffer_index = editor.main_buffers.len;
            MainBuffer main_buffer = { buffer_make(), NULL };
            DA_APPEND(editor.main_buffers, main_buffer);
          }
        } break;

        case WinxKeyCodeQ: {
          if (is_ctrl_pressed)
            is_running = false;
        } break;

        case WinxKeyCodeK: {
          buffer_delete(&CURRENT_BUFFER());
          if (CURRENT_FILE_PATH())
            free(CURRENT_FILE_PATH());
          DA_REMOVE_AT(editor.main_buffers, editor.current_main_buffer_index);
          if (editor.main_buffers.len == 0) {
            MainBuffer main_buffer = { buffer_make(), NULL };
            DA_APPEND(editor.main_buffers, main_buffer);
          }
          if (editor.current_main_buffer_index > 0)
            --editor.current_main_buffer_index;
        } break;

        case WinxKeyCodeEscape: {
          if (editor.mode == JetModeCommandPalette) {
            editor.current_buffer = &editor.main_buffers.items[editor.current_main_buffer_index].buffer;
            editor.mode = JetModeEditor;
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
        if (!is_ctrl_pressed && !is_alt_pressed)
          buffer_insert(editor.current_buffer, event.as._char._char);
      } break;

      default: break;
      }
    }

    if (!vik_begin_frame(executor, BG_COLOR, 1.0)) {
      winx_draw(window);
      continue;
    }
    tr_begin_frame(&ptr, editor.font_scale,
                   editor.palette_buffer.cursor_row,
                   editor.palette_buffer.cursor_col,
                   editor.palette_buffer.cursor_row,
                   editor.palette_buffer.cursor_col + 1);
    sr_begin_frame(&sr);
    tr_begin_frame(&tr, editor.font_scale,
                   CURRENT_BUFFER().cursor_row,
                   CURRENT_BUFFER().cursor_col,
                   CURRENT_BUFFER().cursor_row,
                   CURRENT_BUFFER().cursor_col + 1);
    for (u32 i = 0; i < CURRENT_BUFFER().lines.len; ++i) {
      Line *line = CURRENT_BUFFER().lines.items + i;
      tr_draw_line(&tr, line->items, line->len,
                   BUFFER_PADDING,
                   BUFFER_PADDING + editor.font_scale * i);
    }
    if (editor.mode == JetModeCommandPalette) {
      WideStr line = buffer_get_current_line(editor.current_buffer);

      tr_set_bg_color(&ptr, BG_COLOR);
      tr_set_fg_color(&ptr, FG_COLOR);

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
      sr_draw_rect(&sr,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING * 2.0 + PALETTE_BORDER_WIDTH + editor.font_scale,
                   window->width * PALETTE_WIDTH_FACTOR,
                   PALETTE_BORDER_WIDTH,
                   FG_COLOR, PALETTE_ALPHA);
      tr_draw_line(&ptr, line.ptr, line.len,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH);

      editor.provider->free_opts(editor.options);
      editor.options = editor.provider->get_opts(line);

      // TODO: scroll here too
      for (u32 i = 0; i < editor.options.len; ++i) {
        WideStr *option = editor.options.items + i;
        f32 x = window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH;
        f32 y = window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING * 3.0 + PALETTE_BORDER_WIDTH + editor.font_scale * (i + 1);

        if (window->height - y - editor.font_scale < window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH)
          break;

        if (i == editor.selected_option) {
          tr_set_bg_color(&ptr, FG_COLOR);
          tr_set_fg_color(&ptr, BG_COLOR);

          sr_draw_rect(&sr,
                       window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                       y,
                       window->width * PALETTE_WIDTH_FACTOR,
                       editor.font_scale,
                       FG_COLOR, PALETTE_ALPHA);
        } else {
          tr_set_bg_color(&ptr, BG_COLOR);
          tr_set_fg_color(&ptr, FG_COLOR);
        }

        tr_draw_text(&ptr, option->ptr, option->len, x, y);
      }
    }
    tr_end_frame(&tr);
    sr_end_frame(&sr);
    tr_end_frame(&ptr);
    vik_end_frame(executor);

    winx_draw(window);
  }

  buffer_delete(&editor.palette_buffer);
  for (u32 i = 0; i < editor.main_buffers.len; ++i) {
    buffer_delete(&editor.main_buffers.items[i].buffer);
    if (editor.main_buffers.items[i].file_path)
      free(editor.main_buffers.items[i].file_path);
  }
  if (editor.main_buffers.items)
    free(editor.main_buffers.items);

  sr_delete(&sr);
  tr_delete(&ptr);
  tr_delete(&tr);
  vik_delete_executor(executor);
  vik_delete_instance(instance);

  winx_destroy_window(window);
  winx_cleanup(winx);

  return 0;
}
