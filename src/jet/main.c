#include "shl/shl-defs.h"
#include "viking/viking.h"
#include "winx/event.h"
#include "../../build/shaders.c"
#include "text-renderer.h"
#include "buffer.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define SHL_STR_IMPLEMENTATION
#include "shl/shl-str.h"

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
                            build_shaders_text_vert_spv,
                            build_shaders_text_frag_spv);
  tr_resize(&tr, window->width, window->height);

  Buffer buffer = buffer_make();

  bool is_running = true;

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
        case WinxKeyCodeEnter: {
          buffer_insert_new_line(&buffer);
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

        default: break;
        }
      } break;

      case WinxEventKindChar: {
        buffer_insert(&buffer, event.as._char._char);
      } break;

      default: break;
      }
    }

    if (!vik_begin_frame(executor, 0.0, 0.0, 0.0, 1.0)) {
      winx_draw(window);
      continue;
    }
    tr_begin_frame(&tr);
    for (u32 i = 0; i < buffer.lines.len; ++i) {
      Line *line = buffer.lines.items + i;
      tr_draw_text(&tr, line->items, line->len, 15.0, 15.0 * (i + 1), 1.0, 1.0, 1.0);
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
