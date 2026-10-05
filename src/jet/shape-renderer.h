#ifndef SHAPE_RENDERER_H
#define SHAPE_RENDERER_H

#include "viking/viking.h"

typedef struct {
  f32 screen_width;
  f32 screen_height;
} ShapeUBO;

typedef struct {
  f32 x, y;
  f32 r, g, b, a;
} ShapeVertex;

typedef Da(ShapeVertex) ShapeVertices;

typedef struct {
  f32 x, y;
  f32 u, v;
  f32 r, g, b, a;
} CircleVertex;

typedef Da(CircleVertex) CircleVertices;

typedef Da(u32) Indices;

typedef struct {
  ShapeUBO        ubo_data;
  ShapeVertices   shape_vertices;
  Indices         shape_indices;
  CircleVertices  circle_vertices;
  Indices         circle_indices;
  VikInstance    *instance;
  VikExecutor    *executor;
  VikBuffer      *ubo;
  VikPipeline    *shape_pipeline;
  VikPipeline    *circle_pipeline;
  VikMesh        *shape_mesh;
  VikMesh        *circle_mesh;
  bool            is_ubo_data_dirty;
} ShapeRenderer;

ShapeRenderer sr_make(VikInstance *instance,
                      VikExecutor *executor,
                      Str shape_vert_bc,
                      Str shape_frag_bc,
                      Str circle_vert_bc,
                      Str circle_frag_bc);
void          sr_resize(ShapeRenderer *sr, f32 width, f32 height);
void          sr_begin_frame(ShapeRenderer *sr);
void          sr_draw_rect(ShapeRenderer *sr,
                           f32 x, f32 y,
                           f32 width, f32 height,
                           f32 r, f32 g, f32 b, f32 a);
void          sr_draw_rounded_rect(ShapeRenderer *sr,
                                   f32 x, f32 y,
                                   f32 width, f32 height, f32 radius,
                                   f32 r, f32 g, f32 b, f32 a,
                                   bool is_shadow);
void          sr_end_frame(ShapeRenderer *sr);
void          sr_delete(ShapeRenderer *sr);

#endif // SHAPE_RENDERER_H
