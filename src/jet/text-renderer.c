#include <assert.h>

#include "text-renderer.h"
#include "config.h"

#define ATLAS_PADDING 5
#define ATLAS_WIDTH   32768
#define ATLAS_HEIGHT  MAX_FONT_SCALE

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
                     Str font,
                     Str text_vert_bc,
                     Str text_frag_bc,
                     Str sel_vert_bc,
                     Str sel_frag_bc) {
  TextRenderer tr = {0};
  tr.instance = instance;
  tr.executor = executor;

  i32 offset = stbtt_GetFontOffsetForIndex((u8 *) font.ptr, 0);
  stbtt_InitFont(&tr.font, (u8 *) font.ptr, offset);

  tr.ubo = vik_make_buffer(instance, sizeof(TextUBO), VikBufferKindUBO);
  tr.atlas_data = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(*tr.atlas_data));
  tr.atlas = vik_make_image_ex(instance, tr.atlas_data,
                               ATLAS_WIDTH, ATLAS_HEIGHT,
                               VikImageFormatR8,
                               VikImageFilterLinear);

  VikShader *text_shader = vik_make_shader_vf(instance, text_vert_bc, text_frag_bc);
  tr.text_ssbo = vik_make_buffer(instance, 1, VikBufferKindSSBO);
  VikBuffer *text_buffers[] = { tr.ubo, tr.text_ssbo, };
  tr.text_pipeline = vik_make_pipeline(instance, text_shader,
                                       attrs, ARRAY_LEN(attrs),
                                       text_buffers, ARRAY_LEN(text_buffers),
                                       &tr.atlas, 1);
  vik_delete_shader(text_shader);

  VikShader *sel_shader = vik_make_shader_vf(instance, sel_vert_bc, sel_frag_bc);
  tr.sel_ssbo = vik_make_buffer(instance, 1, VikBufferKindSSBO);
  VikBuffer *sel_buffers[] = { tr.ubo, tr.sel_ssbo, };
  tr.sel_pipeline = vik_make_pipeline(instance, sel_shader,
                                      attrs, ARRAY_LEN(attrs),
                                      sel_buffers, ARRAY_LEN(sel_buffers),
                                      NULL, 0);
  vik_delete_shader(sel_shader);

  tr.mesh = vik_make_mesh(instance,
                          vertices, ARRAY_LEN(vertices),
                          indices, ARRAY_LEN(indices));

  return tr;
}

void tr_resize(TextRenderer *tr, f32 width, f32 height) {
  tr->ubo_data.screen_width = width;
  tr->ubo_data.screen_height = height;
  tr->is_ubo_data_dirty = true;
}

void tr_begin_frame(TextRenderer *tr, f32 scale,
                    u32 sel_begin_row, u32 sel_begin_col,
                    u32 sel_end_row, u32 sel_end_col) {
  tr->text_ssbo_data.len = 0;
  tr->sel_ssbo_data.len = 0;
  tr->is_glyphs_cache_dirty = false;

  tr->scale = scale;

  tr->sel_begin_row = sel_begin_row;
  tr->sel_begin_col = sel_begin_col;
  tr->sel_end_row = sel_end_row;
  tr->sel_end_col = sel_end_col;

  tr->line_index = 0;
}

static void get_char_data(TextRenderer *tr, u32 _char, f32 scale,
                          f32 *out_x, f32 *out_y,
                          f32 *out_width, f32 *out_height,
                          f32 *out_tl_u, f32 *out_tl_v,
                          f32 *out_br_u, f32 *out_br_v) {
  for (u32 i = 0; i < tr->glyphs_cache.len; ++i) {
    Glyph *glyph = tr->glyphs_cache.items + i;
    if (glyph->_char == _char && glyph->scale == scale) {
      *out_x += glyph->x_offset;
      *out_y += glyph->y_offset;
      *out_width = glyph->w;
      *out_height = glyph->h;
      *out_tl_u = glyph->tl_u;
      *out_tl_v = glyph->tl_v;
      *out_br_u = glyph->br_u;
      *out_br_v = glyph->br_v;
      return;
    }
  }

  f32 scale_y = stbtt_ScaleForPixelHeight(&tr->font, scale);
  i32 width, height;
  u8 *bitmap = stbtt_GetCodepointBitmap(&tr->font, 0.0, scale_y, _char,
                                        &width, &height, NULL, NULL);

  // Resetting in case we zoomed in/out too much
  if (tr->atlas_cursor_x + width + ATLAS_PADDING > ATLAS_WIDTH) {
    tr->atlas_cursor_x = 0;

    memset(tr->atlas_data, 0, ATLAS_WIDTH * ATLAS_HEIGHT * sizeof(*tr->atlas_data));

    tr->glyphs_cache.len = 0;
  }

  // If this fails, increase atlas size
  assert(tr->atlas_cursor_x + width + ATLAS_PADDING <= ATLAS_WIDTH);
  assert(height <= ATLAS_HEIGHT);

  tr->atlas_cursor_x += ATLAS_PADDING;

  for (u32 y = 0; y < (u32) height; ++y)
    for (u32 x = 0; x < (u32) width; ++x)
      tr->atlas_data[y * ATLAS_WIDTH + x + tr->atlas_cursor_x] =
        bitmap[y * width + x];

  *out_width = (f32) width;
  *out_height = (f32) height;

  *out_tl_u = (f32) tr->atlas_cursor_x / ATLAS_WIDTH;
  *out_tl_v = 0.0;

  tr->atlas_cursor_x += width;

  *out_br_u = (f32) tr->atlas_cursor_x / ATLAS_WIDTH;
  *out_br_v = (f32) height / ATLAS_HEIGHT;

  free(bitmap);

  i32 advance, lsb;
  stbtt_GetCodepointHMetrics(&tr->font, _char, &advance, &lsb);

  i32 ascent;
  stbtt_GetFontVMetrics(&tr->font, &ascent, NULL, NULL);

  int y0;
  stbtt_GetCodepointBitmapBox(&tr->font, _char, 0.0, scale_y, NULL, &y0, NULL, NULL);

  f32 x_offset = advance * scale_y;
  f32 y_offset = ascent * scale_y + y0;

  *out_x += x_offset;
  *out_y += y_offset;

  Glyph glyph = {
    _char,
    scale,
    x_offset, y_offset,
    *out_width, *out_height,
    *out_tl_u, *out_tl_v,
    *out_br_u, *out_br_v,
  };
  DA_APPEND(tr->glyphs_cache, glyph);
  tr->is_glyphs_cache_dirty = true;
}

void tr_set_bg_color(TextRenderer *tr, f32 r, f32 g, f32 b) {
  tr->bg_r = r;
  tr->bg_g = g;
  tr->bg_b = b;
}

void tr_set_fg_color(TextRenderer *tr, f32 r, f32 g, f32 b) {
  tr->fg_r = r;
  tr->fg_g = g;
  tr->fg_b = b;
}

void tr_draw_line(TextRenderer *tr, u32 *text, u32 text_len, f32 x, f32 y) {
  f32 sel_begin_x = x;
  f32 sel_end_x = x;

  for (u32 i = 0; i < text_len; ++i) {
    bool is_selected =
      (tr->line_index == tr->sel_begin_row &&
       tr->line_index == tr->sel_end_row &&
       i >= tr->sel_begin_col &&
       i < tr->sel_end_col) ||
      (tr->line_index > tr->sel_begin_row &&
       tr->line_index < tr->sel_end_row) ||
      (tr->line_index == tr->sel_begin_row &&
       tr->line_index != tr->sel_end_row &&
       i >= tr->sel_begin_col) ||
      (tr->line_index == tr->sel_end_row &&
       tr->line_index != tr->sel_begin_row &&
       i < tr->sel_end_col);
    f32 r = is_selected ? tr->bg_r : tr->fg_r;
    f32 g = is_selected ? tr->bg_g : tr->fg_g;
    f32 b = is_selected ? tr->bg_b : tr->fg_b;
    TextSSBOEntry entry = {
      x, y,       // Modified by get_char_data
      10.0, 10.0, // Overwritten by get_char_data
      0.0, 0.0,   // Overwritten by get_char_data
      1.0, 1.0,   // Overwritten by get_char_data
      r, g, b,
      {},
    };
    get_char_data(tr, text[i], tr->scale,
                  &x, &entry.y,
                  &entry.w, &entry.h,
                  &entry.tl_u, &entry.tl_v,
                  &entry.br_u, &entry.br_v);
    DA_APPEND(tr->text_ssbo_data, entry);

    if (tr->line_index == tr->sel_begin_row && i < tr->sel_begin_col)
      sel_begin_x = x;
    if (tr->line_index != tr->sel_end_row || i < tr->sel_end_col)
      sel_end_x = x;
  }

  if (tr->line_index >= tr->sel_begin_row &&
      tr->line_index <= tr->sel_end_row) {
    SelSSBOEntry entry = {
      sel_begin_x, y,
      sel_end_x - sel_begin_x, tr->scale,
      tr->fg_r, tr->fg_g, tr->fg_b,
      {},
    };

    if (entry.w == 0.0) {
      f32 scale_y = stbtt_ScaleForPixelHeight(&tr->font, tr->scale);
      int advance;
      stbtt_GetCodepointHMetrics(&tr->font, ' ', &advance, NULL);
      entry.w = advance * scale_y;
    }

    DA_APPEND(tr->sel_ssbo_data, entry);
  }

  ++tr->line_index;
}

void tr_draw_text(TextRenderer *tr, u32 *text, u32 text_len, f32 x, f32 y) {
  for (u32 i = 0; i < text_len; ++i) {
    TextSSBOEntry entry = {
      x, y,       // Modified by get_char_data
      10.0, 10.0, // Overwritten by get_char_data
      0.0, 0.0,   // Overwritten by get_char_data
      1.0, 1.0,   // Overwritten by get_char_data
      tr->fg_r, tr->fg_g, tr->fg_b,
      {},
    };
    get_char_data(tr, text[i], tr->scale,
                  &x, &entry.y,
                  &entry.w, &entry.h,
                  &entry.tl_u, &entry.tl_v,
                  &entry.br_u, &entry.br_v);
    DA_APPEND(tr->text_ssbo_data, entry);
  }
}

void tr_end_frame(TextRenderer *tr) {
  if (tr->is_ubo_data_dirty) {
    vik_set_buffer_data(tr->ubo, &tr->ubo_data);
    tr->is_ubo_data_dirty = false;
  }

  bool should_recreate_text_ssbo = tr->text_ssbo_data.len > tr->max_text_ssbo_data_len;
  if (should_recreate_text_ssbo) {
    vik_delete_buffer(tr->text_ssbo);
    tr->text_ssbo = vik_make_buffer(tr->instance,
                                    tr->text_ssbo_data.len *
                                    sizeof(*tr->text_ssbo_data.items),
                                    VikBufferKindSSBO);
    tr->max_text_ssbo_data_len = tr->text_ssbo_data.len;
  }

  if (tr->text_ssbo_data.len > 0)
    vik_set_buffer_data(tr->text_ssbo, tr->text_ssbo_data.items);

  bool should_recreate_sel_ssbo = tr->sel_ssbo_data.len > tr->max_sel_ssbo_data_len;
  if (should_recreate_sel_ssbo) {
    vik_delete_buffer(tr->sel_ssbo);
    tr->sel_ssbo = vik_make_buffer(tr->instance,
                                   tr->sel_ssbo_data.len *
                                   sizeof(*tr->sel_ssbo_data.items),
                                   VikBufferKindSSBO);
    tr->max_sel_ssbo_data_len = tr->sel_ssbo_data.len;
  }

  if (tr->sel_ssbo_data.len > 0)
    vik_set_buffer_data(tr->sel_ssbo, tr->sel_ssbo_data.items);

  if (tr->is_glyphs_cache_dirty) {
    vik_delete_image(tr->atlas);
    tr->atlas = vik_make_image_ex(tr->instance, tr->atlas_data,
                                  ATLAS_WIDTH, ATLAS_HEIGHT,
                                  VikImageFormatR8,
                                  VikImageFilterLinear);
  }

  if (should_recreate_text_ssbo || tr->is_glyphs_cache_dirty) {
    VikBuffer *buffers[] = { tr->ubo, tr->text_ssbo, };
    vik_use_resources(tr->text_pipeline, buffers, ARRAY_LEN(buffers), &tr->atlas, 1);

    tr->is_glyphs_cache_dirty = false;
  }

  if (should_recreate_sel_ssbo) {
    VikBuffer *buffers[] = { tr->ubo, tr->sel_ssbo, };
    vik_use_resources(tr->sel_pipeline, buffers, ARRAY_LEN(buffers), NULL, 0);
  }

  vik_cmd_use_pipeline(tr->executor, tr->sel_pipeline);
  vik_cmd_draw(tr->executor, tr->mesh, tr->sel_ssbo_data.len);
  vik_cmd_use_pipeline(tr->executor, tr->text_pipeline);
  vik_cmd_draw(tr->executor, tr->mesh, tr->text_ssbo_data.len);
}

void tr_delete(TextRenderer *tr) {
  vik_delete_mesh(tr->mesh);
  vik_delete_pipeline(tr->sel_pipeline);
  vik_delete_pipeline(tr->text_pipeline);
  vik_delete_image(tr->atlas);
  vik_delete_buffer(tr->sel_ssbo);
  vik_delete_buffer(tr->text_ssbo);
  vik_delete_buffer(tr->ubo);

  free(tr->atlas_data);
  if (tr->sel_ssbo_data.items)
    free(tr->sel_ssbo_data.items);
  if (tr->text_ssbo_data.items)
    free(tr->text_ssbo_data.items);
  if (tr->glyphs_cache.items)
    free(tr->glyphs_cache.items);
}
