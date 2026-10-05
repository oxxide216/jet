#include "shl/shl-defs.h"
#include "viking/viking.h"
#include "winx/event.h"
#include "../../build/assets.c"
#include "text-renderer.h"
#include "shape-renderer.h"
#include "buffer.h"
#include "config.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

typedef enum {
  JetModeEditor = 0,
  JetModeCommandPalette,
} JetMode;

i32 main(void) {
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

  Buffer editor_buffer = buffer_make();
  Buffer palette_buffer = buffer_make();
  Buffer *current_buffer = &editor_buffer;

  JetMode mode = JetModeEditor;

  f32 font_scale = 24.0;

  bool is_running = true;
  bool is_ctrl_pressed = false;
  bool is_alt_pressed = false;

  while (is_running) {
    WinxEvent event;
    while ((event = winx_get_event(window, false)).kind != WinxEventKindNone) {
      is_running = event.kind != WinxEventKindQuit;
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
        case WinxKeyCodeLeftControl: {
          is_ctrl_pressed = true;
        } break;

        case WinxKeyCodeLeftAlt: {
          is_alt_pressed = true;
        } break;

        case WinxKeyCodeEnter: {
          if (!is_ctrl_pressed && mode == JetModeEditor)
            buffer_insert_new_line(current_buffer);
        } break;

        case WinxKeyCodeTab: {
          if (!is_ctrl_pressed && mode == JetModeEditor)
            for (u32 i = 0; i < SPACES_PER_TAB; ++i)
              buffer_insert(current_buffer, ' ');
        } break;

        case WinxKeyCodeBackspace: {
          if (is_ctrl_pressed)
            buffer_remove_word_before_cursor(current_buffer);
          else
            buffer_remove_before_cursor(current_buffer);
        } break;

        case WinxKeyCodeDelete: {
          if (is_ctrl_pressed)
            buffer_remove_word_at_cursor(current_buffer);
          else
            buffer_remove_at_cursor(current_buffer);
        } break;

        case WinxKeyCodeLeft: {
          if (is_ctrl_pressed)
            buffer_move_left_word(current_buffer);
          else
            buffer_move_left(current_buffer);
        } break;

        case WinxKeyCodeRight: {
          if (is_ctrl_pressed)
            buffer_move_right_word(current_buffer);
          else
            buffer_move_right(current_buffer);
        } break;

        case WinxKeyCodeDown: {
          if (mode == JetModeEditor) {
            if (is_ctrl_pressed)
              buffer_move_down_paragraph(current_buffer);
            else
              buffer_move_down(current_buffer);
          }
        } break;

        case WinxKeyCodeUp: {
          if (mode == JetModeEditor) {
            if (is_ctrl_pressed)
              buffer_move_up_paragraph(current_buffer);
            else
              buffer_move_up(current_buffer);
          }
        } break;

        case WinxKeyCodeEqual: {
          if (is_ctrl_pressed && font_scale < MAX_FONT_SCALE)
            font_scale += 1.0;
        } break;

        case WinxKeyCodeMinus: {
          if (is_ctrl_pressed && font_scale > MIN_FONT_SCALE)
            font_scale -= 1.0;
        } break;

        case WinxKeyCodeP: {
          if (is_ctrl_pressed) {
            if (mode == JetModeEditor) {
              palette_buffer.lines.items[0].len = 0;
              palette_buffer.cursor_col = 0;
              current_buffer = &palette_buffer;
              mode = JetModeCommandPalette;
            } else if (mode == JetModeCommandPalette) {
              current_buffer = &editor_buffer;
              mode = JetModeEditor;
            }
          }
        } break;

        case WinxKeyCodeA: {
          if (is_ctrl_pressed)
            buffer_goto_line_begin(current_buffer);
          else if (is_alt_pressed)
            buffer_goto_buffer_begin(current_buffer);
        } break;

        case WinxKeyCodeE: {
          if (is_ctrl_pressed)
            buffer_goto_line_end(current_buffer);
          else if (is_alt_pressed)
            buffer_goto_buffer_end(current_buffer);
        } break;

        case WinxKeyCodeEscape: {
          if (mode == JetModeCommandPalette) {
            current_buffer = &editor_buffer;
            mode = JetModeEditor;
          }
        } break;

        default: break;
        }
      } break;

      case WinxEventKindKeyRelease: {
        if (event.as.key.key_code == WinxKeyCodeLeftControl)
          is_ctrl_pressed = false;
        else if (event.as.key.key_code == WinxKeyCodeLeftAlt)
          is_alt_pressed = false;
      } break;

      case WinxEventKindChar: {
        if (!is_ctrl_pressed && !is_alt_pressed)
          buffer_insert(current_buffer, event.as._char._char);
      } break;

      default: break;
      }
    }

    if (!vik_begin_frame(executor, BG_COLOR, 1.0)) {
      winx_draw(window);
      continue;
    }
    tr_begin_frame(&tr, font_scale,
                   editor_buffer.cursor_row,
                   editor_buffer.cursor_col,
                   editor_buffer.cursor_row,
                   editor_buffer.cursor_col + 1);
    tr_begin_frame(&ptr, font_scale,
                   palette_buffer.cursor_row,
                   palette_buffer.cursor_col,
                   palette_buffer.cursor_row,
                   palette_buffer.cursor_col + 1);
    sr_begin_frame(&sr);
    tr_set_bg_color(&tr, BG_COLOR);
    tr_set_fg_color(&tr, FG_COLOR);
    for (u32 i = 0; i < editor_buffer.lines.len; ++i) {
      Line *line = editor_buffer.lines.items + i;
      tr_draw_line(&tr, line->items, line->len,
                   BUFFER_PADDING,
                   BUFFER_PADDING + font_scale * i);
    }
    if (mode == JetModeCommandPalette) {
      Line *line = palette_buffer.lines.items;
      sr_draw_rounded_rect(&sr,
                           window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                           window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5,
                           window->width * PALETTE_WIDTH_FACTOR,
                           window->height * PALETTE_HEIGHT_FACTOR,
                           PALETTE_BORDER_RADIUS, FG_COLOR);
      sr_draw_rounded_rect(&sr,
                           window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + PALETTE_BORDER_WIDTH,
                           window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + PALETTE_BORDER_WIDTH,
                           window->width * PALETTE_WIDTH_FACTOR - PALETTE_BORDER_WIDTH * 2.0,
                           window->height * PALETTE_HEIGHT_FACTOR - PALETTE_BORDER_WIDTH * 2.0,
                           PALETTE_BORDER_RADIUS, BG_COLOR);
      sr_draw_rect(&sr,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING * 2.0 + PALETTE_BORDER_WIDTH + font_scale,
                   window->width * PALETTE_WIDTH_FACTOR,
                   PALETTE_BORDER_WIDTH,
                   FG_COLOR);
      tr_draw_text(&tr, line->items, line->len,
                   window->width * (1.0 - PALETTE_WIDTH_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH,
                   window->height * (1.0 - PALETTE_HEIGHT_FACTOR) * 0.5 + BUFFER_PADDING + PALETTE_BORDER_WIDTH);
    }
    sr_end_frame(&sr);
    tr_end_frame(&ptr);
    tr_end_frame(&tr);
    vik_end_frame(executor);

    winx_draw(window);
  }

  buffer_delete(&palette_buffer);
  buffer_delete(&editor_buffer);

  sr_delete(&sr);
  tr_delete(&ptr);
  tr_delete(&tr);
  vik_delete_executor(executor);
  vik_delete_instance(instance);

  winx_destroy_window(window);
  winx_cleanup(winx);

  return 0;
}
