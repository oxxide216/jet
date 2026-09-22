#include "text-renderer.h"

typedef struct {
  u32 dummy;
} TextVertex;

static VikAttr attrs[] = {
  VikAttrUInt,
};

static TextVertex vertices[4] = {0};

static u32 indices[] = { 0, 1, 2, 2, 1, 3 };

TextRenderer tr_make(VikInstance *instance,
                     VikExecutor *executor,
                     Str vert_bc, Str frag_bc) {
  TextRenderer tr = {0};
  tr.instance = instance;
  tr.executor = executor;

  VikShader *shader = vik_make_shader_vf(instance, vert_bc, frag_bc);
  tr.ubo = vik_make_buffer(instance, sizeof(TextUBO), VikBufferKindUBO);
  tr.ssbo = vik_make_buffer(instance, 1, VikBufferKindSSBO);
  VikBuffer *buffers[] = { tr.ubo, tr.ssbo, };
  tr.pipeline = vik_make_pipeline(instance, shader,
                                  attrs, ARRAY_LEN(attrs),
                                  buffers, ARRAY_LEN(buffers),
                                  NULL, 0);
  vik_delete_shader(shader);

  tr.mesh = vik_make_mesh(instance,
                          vertices, ARRAY_LEN(vertices),
                          indices, ARRAY_LEN(indices));

  return tr;
}

void tr_resize(TextRenderer *tr, f32 width, f32 height) {
  tr->ubo_data.screen_width = width;
  tr->ubo_data.screen_height = height;
  vik_set_buffer_data(tr->ubo, &tr->ubo_data);
}

void tr_begin_frame(TextRenderer *tr) {
  tr->ssbo_data.len = 0;
}

void tr_draw_text(TextRenderer *tr, u32 *text, u32 text_len,
                  f32 x, f32 y, f32 r, f32 g, f32 b) {
  for (u32 i = 0; i < text_len; ++i) {
    TextSSBOEntry entry = {
      x + i * 10.0, y,
      10.0, 10.0,
      0.0, 0.0,
      1.0, 1.0,
      r, g, b,
      {},
    };
    DA_APPEND(tr->ssbo_data, entry);
  }
}

void tr_end_frame(TextRenderer *tr) {
  if (tr->ssbo_data.len > tr->max_ssbo_data_len) {
    vik_delete_buffer(tr->ssbo);
    tr->ssbo = vik_make_buffer(tr->instance,
                               tr->ssbo_data.len *
                               sizeof(*tr->ssbo_data.items),
                               VikBufferKindSSBO);
    VikBuffer *buffers[] = { tr->ubo, tr->ssbo, };
    vik_use_resources(tr->pipeline, buffers, ARRAY_LEN(buffers), NULL, 0);

    tr->max_ssbo_data_len = tr->ssbo_data.len;
  }

  if (tr->ssbo_data.len > 0)
    vik_set_buffer_data(tr->ssbo, tr->ssbo_data.items);

  vik_cmd_use_pipeline(tr->executor, tr->pipeline);
  vik_cmd_draw(tr->executor, tr->mesh, tr->ssbo_data.len);
}

void tr_delete(TextRenderer *tr) {
  vik_delete_mesh(tr->mesh);
  vik_delete_pipeline(tr->pipeline);
  vik_delete_buffer(tr->ssbo);
  vik_delete_buffer(tr->ubo);

  if (tr->ssbo_data.items)
    free(tr->ssbo_data.items);
}
