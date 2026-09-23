#include "shl/shl-defs.h"
#include "viking/viking.h"
#include "winx/event.h"
#include "../../build/assets.c"
#include "text-renderer.h"
#include "buffer.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

#define MAX_FONT_SCALE 240.0
#define MIN_FONT_SCALE 8.0

i32 main(void) {
  Winx *winx = winx_init();
  WinxWindow *window = winx_init_window(winx, STR_LIT("Jet"),
                                        1600, 900,
                                        WinxGraphicsModeVulkan,
                                        NULL);
  window->target_fps = winx_get_refresh_rate(window);

  VikInstance *instance = vik_make_instance(window, VikRequestFlagsNone);
  VikExecutor *executor = vik_make_executor(instance);

  TextRenderer tr = tr_make(instance, executor,
                            fonts_MonaspaceNeon_Regular_otf,
                            build_shaders_text_vert_spv,
                            build_shaders_text_frag_spv,
                            build_shaders_sel_vert_spv,
                            build_shaders_sel_frag_spv);
  tr_resize(&tr, window->width, window->height);

  Buffer buffer = buffer_make();

  f32 font_scale = 24.0;

  bool is_running = true;
  bool is_ctrl_pressed = false;

  while (is_running) {
    WinxEvent event;
    while ((event = winx_get_event(window, false)).kind != WinxEventKindNone) {
      is_running = event.kind != WinxEventKindQuit;
      if (!is_running)
        break;

      switch (event.kind) {
      case WinxEventKindResize: {
        tr_resize(&tr, event.as.resize.width, event.as.resize.height);
      } break;

      case WinxEventKindKeyPress:
      case WinxEventKindKeyHold: {
        switch (event.as.key.key_code) {
        case WinxKeyCodeLeftControl: {
          is_ctrl_pressed = true;
        } break;

        case WinxKeyCodeEnter: {
          if (!is_ctrl_pressed)
            buffer_insert_new_line(&buffer);
        } break;

        case WinxKeyCodeTab: {
          if (!is_ctrl_pressed)
            for (u32 i = 0; i < 2; ++i)
              buffer_insert(&buffer, ' ');
        } break;

        case WinxKeyCodeBackspace: {
          buffer_remove_before_cursor(&buffer);
        } break;

        case WinxKeyCodeDelete: {
          buffer_remove_at_cursor(&buffer);
        } break;

        case WinxKeyCodeLeft: {
          buffer_move_left(&buffer);
        } break;

        case WinxKeyCodeRight: {
          buffer_move_right(&buffer);
        } break;

        case WinxKeyCodeDown: {
          buffer_move_down(&buffer);
        } break;

        case WinxKeyCodeUp: {
          buffer_move_up(&buffer);
        } break;

        case WinxKeyCodeEqual: {
          if (is_ctrl_pressed && font_scale < MAX_FONT_SCALE)
            font_scale += 1.0;
        } break;

        case WinxKeyCodeMinus: {
          if (is_ctrl_pressed && font_scale > MIN_FONT_SCALE)
            font_scale -= 1.0;
        } break;

        default: break;
        }
      } break;

      case WinxEventKindKeyRelease: {
        if (event.as.key.key_code == WinxKeyCodeLeftControl)
          is_ctrl_pressed = false;
      } break;

      case WinxEventKindChar: {
        if (!is_ctrl_pressed)
          buffer_insert(&buffer, event.as._char._char);
      } break;

      default: break;
      }
    }

    if (!vik_begin_frame(executor, 0.0, 0.0, 0.0, 1.0)) {
      winx_draw(window);
      continue;
    }
    tr_begin_frame(&tr, font_scale,
                   buffer.cursor_row, buffer.cursor_col,
                   buffer.cursor_row, buffer.cursor_col + 1);
    tr_set_bg_color(&tr, 0.0, 0.0, 0.0);
    tr_set_fg_color(&tr, 1.0, 1.0, 1.0);
    for (u32 i = 0; i < buffer.lines.len; ++i) {
      Line *line = buffer.lines.items + i;
      tr_draw_line(&tr, line->items, line->len,
                   10.0, 10.0 + font_scale * i);
    }
    tr_end_frame(&tr);
    vik_end_frame(executor);

    winx_draw(window);
  }

  buffer_delete(&buffer);

  tr_delete(&tr);
  vik_delete_executor(executor);
  vik_delete_instance(instance);

  winx_destroy_window(window);
  winx_cleanup(winx);

  return 0;
}
