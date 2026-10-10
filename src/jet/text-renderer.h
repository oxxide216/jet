#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "winx/winx.h"
#include "viking/viking.h"
#include "stb_truetype.h"
#include "config.h"

typedef struct {
  u32 _char;
  f32 x_offset;
  f32 y_offset;
  f32 w, h;
  f32 tl_u, tl_v;
  f32 br_u, br_v;
} Glyph;

typedef Da(Glyph) Glyphs;

typedef struct {
  stbtt_fontinfo  font;
  Glyphs          glyphs_cache[MAX_FONT_SCALE - MIN_FONT_SCALE + 1];
  u8             *data;
  u32             cursor_x;
  VikInstance    *instance;
  VikImage       *image;
  u64             image_generation;
  bool            is_glyphs_cache_dirty;
} Atlas;

typedef struct {
  f32 screen_width;
  f32 screen_height;
} TextUBO;

typedef struct {
  f32 x, y;
  f32 w, h;
  f32 tl_u, tl_v;
  f32 br_u, br_v;
  f32 r, g, b;
  u8  p[4];
} TextSSBOEntry;

typedef Da(TextSSBOEntry) TextSSBO;

typedef struct {
  f32 x, y;
  f32 w, h;
  f32 r, g, b;
  u8  p[4];
} SelSSBOEntry;

typedef Da(SelSSBOEntry) SelSSBO;

typedef struct {
  Atlas          *atlas;
  u64             last_atlas_image_generation;
  TextUBO         ubo_data;
  TextSSBO        text_ssbo_data;
  SelSSBO         sel_ssbo_data;
  u32             max_text_ssbo_data_len;
  u32             max_sel_ssbo_data_len;
  WinxWindow     *window;
  VikInstance    *instance;
  VikExecutor    *executor;
  VikBuffer      *ubo;
  VikBuffer      *text_ssbo;
  VikBuffer      *sel_ssbo;
  VikPipeline    *text_pipeline;
  VikPipeline    *sel_pipeline;
  VikMesh        *mesh;
  f32             bg_r, bg_g, bg_b;
  f32             fg_r, fg_g, fg_b;
  f32             sel_r, sel_g, sel_b;
  u32             scale;
  u32             sel_begin_row;
  u32             sel_begin_col;
  u32             sel_end_row;
  u32             sel_end_col;
  f32             x_lower_limit;
  f32             x_higher_limit;
  u32             line_index;
  bool            is_glyphs_cache_dirty;
  bool            is_ubo_data_dirty;
} TextRenderer;

Atlas atlas_make(VikInstance *instance, Str font);
void  atlas_ensure_is_actual(Atlas *atlas);
void  atlas_delete(Atlas *atlas);

TextRenderer tr_make(WinxWindow *window,
                     VikInstance *instance,
                     VikExecutor *executor,
                     Atlas *atlas,
                     Str text_vert_bc,
                     Str text_frag_bc,
                     Str sel_vert_bc,
                     Str sel_frag_bc);
void         tr_resize(TextRenderer *tr, f32 width, f32 height);
void         tr_begin_frame(TextRenderer *tr, u32 scale,
                            u32 sel_begin_row, u32 sel_begin_col,
                            u32 sel_end_row, u32 sel_end_col);
void         tr_set_bg_color(TextRenderer *tr, f32 r, f32 g, f32 b);
void         tr_set_fg_color(TextRenderer *tr, f32 r, f32 g, f32 b);
void         tr_set_sel_color(TextRenderer *tr, f32 r, f32 g, f32 b);
f32          tr_measure_text(TextRenderer *tr, u32 *text, u32 text_len);
f32          tr_draw_line(TextRenderer *tr, u32 *text, u32 text_len,
                          f32 x, f32 y, f32 x_limit, f32 *out_x);
void         tr_draw_text(TextRenderer *tr, u32 *text, u32 text_len, f32 x, f32 y);
void         tr_end_frame(TextRenderer *tr);
void         tr_delete(TextRenderer *tr);

#endif // TEXT_RENDERER_H
