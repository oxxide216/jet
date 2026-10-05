#include "shape-renderer.h"

static VikAttr shape_attrs[] = {
  VikAttrVec2,
  VikAttrVec3,
};

static VikAttr circle_attrs[] = {
  VikAttrVec2,
  VikAttrVec2,
  VikAttrVec3,
};

ShapeRenderer sr_make(VikInstance *instance,
                      VikExecutor *executor,
                      Str shape_vert_bc,
                      Str shape_frag_bc,
                      Str circle_vert_bc,
                      Str circle_frag_bc) {
  ShapeRenderer sr = {0};
  sr.instance = instance;
  sr.executor = executor;

  sr.ubo = vik_make_buffer(instance, sizeof(ShapeUBO), VikBufferKindUBO);

  VikShader *shape_shader = vik_make_shader_vf(instance, shape_vert_bc, shape_frag_bc);
  sr.shape_pipeline = vik_make_pipeline(instance, shape_shader,
                                        shape_attrs, ARRAY_LEN(shape_attrs),
                                        &sr.ubo, 1,
                                        NULL, 0);
  vik_delete_shader(shape_shader);

  VikShader *circle_shader = vik_make_shader_vf(instance, circle_vert_bc, circle_frag_bc);
  sr.circle_pipeline = vik_make_pipeline(instance, circle_shader,
                                         circle_attrs, ARRAY_LEN(circle_attrs),
                                         &sr.ubo, 1,
                                         NULL, 0);
  vik_delete_shader(circle_shader);

  return sr;
}

void sr_resize(ShapeRenderer *sr, f32 width, f32 height) {
  sr->ubo_data.screen_width = width;
  sr->ubo_data.screen_height = height;
  sr->is_ubo_data_dirty = true;
}

void sr_begin_frame(ShapeRenderer *sr) {
  sr->shape_vertices.len = 0;
  sr->shape_indices.len = 0;
  sr->circle_vertices.len = 0;
  sr->circle_indices.len = 0;
}

void sr_draw_rect(ShapeRenderer *sr,
                  f32 x, f32 y,
                  f32 width, f32 height,
                  f32 r, f32 g, f32 b) {
  u32 indices[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices); ++i) {
    indices[i] += sr->shape_vertices.len;
    DA_APPEND(sr->shape_indices, indices[i]);
  }

  ShapeVertex vertices[] = {
    { x,         y,          r, g, b },
    { x + width, y,          r, g, b },
    { x,         y + height, r, g, b },
    { x + width, y + height, r, g, b },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices); ++i)
    DA_APPEND(sr->shape_vertices, vertices[i]);
}

void sr_draw_rounded_rect(ShapeRenderer *sr,
                          f32 x, f32 y,
                          f32 width, f32 height,
                          f32 radius, f32 r, f32 g, f32 b) {
  f32 ix = x + radius;
  f32 iy = y + radius;
  f32 iwidth = width - radius * 2.0;
  f32 iheight = height - radius * 2.0;

  u32 indices0[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices0); ++i) {
    indices0[i] += sr->shape_vertices.len;
    DA_APPEND(sr->shape_indices, indices0[i]);
  }

  ShapeVertex vertices0[] = {
    { x,         iy,           r, g, b },
    { x + width, iy,           r, g, b },
    { x,         iy + iheight, r, g, b },
    { x + width, iy + iheight, r, g, b },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices0); ++i)
    DA_APPEND(sr->shape_vertices, vertices0[i]);

  u32 indices1[] = { 0, 1, 2, 2, 1, 3 };
  for (u32 i = 0; i < ARRAY_LEN(indices1); ++i) {
    indices1[i] += sr->shape_vertices.len;
    DA_APPEND(sr->shape_indices, indices1[i]);
  }

  ShapeVertex vertices1[] = {
    { ix,          y,          r, g, b },
    { ix + iwidth, y,          r, g, b },
    { ix,          y + height, r, g, b },
    { ix + iwidth, y + height, r, g, b },
  };
  for (u32 i = 0; i < ARRAY_LEN(vertices1); ++i)
    DA_APPEND(sr->shape_vertices, vertices1[i]);

  f32 x_offsets[2] = { x, ix + iwidth - radius };
  f32 y_offsets[2] = { y, iy + iheight - radius };
  static f32 us[4][4] = {
    { 0.5, -0.5,  0.5, -0.5 },
    { 0.5, -0.5,  0.5, -0.5 },
    { -0.5, 0.5, -0.5,  0.5 },
    { -0.5, 0.5, -0.5,  0.5 },
  };
  static f32 vs[4][4] = {
    {  0.5,  0.5, -0.5, -0.5 },
    { -0.5, -0.5,  0.5,  0.5 },
    {  0.5,  0.5, -0.5, -0.5 },
    { -0.5, -0.5,  0.5,  0.5 },
  };

  f32 d = radius * 2.0;

  for (u32 i = 0; i < 4; ++i) {
    u32 indices2[] = { 0, 1, 2, 2, 1, 3 };
    for (u32 j = 0; j < ARRAY_LEN(indices2); ++j) {
      indices2[j] += sr->circle_vertices.len;
      DA_APPEND(sr->circle_indices, indices2[j]);
    }

    f32 ox = x_offsets[i / 2];
    f32 oy = y_offsets[i % 2];
    f32 *u = us[i];
    f32 *v = vs[i];

    CircleVertex vertices2[] = {
      { ox,     oy,     u[0], v[0], r, g, b },
      { ox + d, oy,     u[1], v[1], r, g, b },
      { ox,     oy + d, u[2], v[2], r, g, b },
      { ox + d, oy + d, u[3], v[3], r, g, b },
    };
    for (u32 j = 0; j < ARRAY_LEN(vertices2); ++j)
      DA_APPEND(sr->circle_vertices, vertices2[j]);
  }
}

void sr_end_frame(ShapeRenderer *sr) {
  if (sr->is_ubo_data_dirty) {
    vik_set_buffer_data(sr->ubo, &sr->ubo_data);
    sr->is_ubo_data_dirty = false;
  }

  if (sr->shape_mesh) {
    vik_delete_mesh(sr->shape_mesh);
    sr->shape_mesh = NULL;
  }

  if (sr->circle_mesh) {
    vik_delete_mesh(sr->circle_mesh);
    sr->circle_mesh = NULL;
  }

  if (sr->shape_vertices.len > 0) {
    sr->shape_mesh = vik_make_mesh(sr->instance,
                                   sr->shape_vertices.items, sr->shape_vertices.len,
                                   sr->shape_indices.items, sr->shape_indices.len);

    vik_cmd_use_pipeline(sr->executor, sr->shape_pipeline);
    vik_cmd_draw(sr->executor, sr->shape_mesh, 1);
  }

  if (sr->circle_vertices.len > 0) {
    sr->circle_mesh = vik_make_mesh(sr->instance,
                                    sr->circle_vertices.items, sr->circle_vertices.len,
                                    sr->circle_indices.items, sr->circle_indices.len);

    vik_cmd_use_pipeline(sr->executor, sr->circle_pipeline);
    vik_cmd_draw(sr->executor, sr->circle_mesh, 1);
  }
}

void sr_delete(ShapeRenderer *sr) {
  if (sr->circle_mesh)
    vik_delete_mesh(sr->circle_mesh);
  if (sr->shape_mesh)
    vik_delete_mesh(sr->shape_mesh);
  vik_delete_pipeline(sr->circle_pipeline);
  vik_delete_pipeline(sr->shape_pipeline);
  vik_delete_buffer(sr->ubo);

  if (sr->shape_vertices.items) {
    free(sr->shape_vertices.items);
    free(sr->shape_indices.items);
  }

  if (sr->circle_vertices.items) {
    free(sr->circle_vertices.items);
    free(sr->circle_indices.items);
  }
}
