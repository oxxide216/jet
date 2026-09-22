#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include "viking/viking.h"

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
  TextUBO      ubo_data;
  TextSSBO     ssbo_data;
  u32          max_ssbo_data_len;
  VikInstance *instance;
  VikExecutor *executor;
  VikBuffer   *ubo;
  VikBuffer   *ssbo;
  VikPipeline *pipeline;
  VikMesh     *mesh;
} TextRenderer;

TextRenderer tr_make(VikInstance *instance,
                     VikExecutor *executor,
                     Str vert_bc, Str frag_bc);
void         tr_resize(TextRenderer *tr, f32 width, f32 height);
void         tr_begin_frame(TextRenderer *tr);
void         tr_draw_text(TextRenderer *tr, u32 *text, u32 text_len,
                          f32 x, f32 y, f32 r, f32 g, f32 b);
void         tr_end_frame(TextRenderer *tr);
void         tr_delete(TextRenderer *tr);

#endif // TEXT_RENDERER_H
