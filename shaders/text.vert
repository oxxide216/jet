#version 450

struct TextSSBOEntry {
  vec2 position;
  vec2 size;
  vec2 top_left_uv;
  vec2 bottom_right_uv;
  vec3 color;
};

layout(binding = 0) uniform TextUBO {
  vec2 size;
} u_screen;

layout(binding = 1) buffer TextSSBO {
  TextSSBOEntry data[];
} u_glyphs;

layout(location = 0) in uint i_dummy;

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec3 o_color;

void main() {
  TextSSBOEntry entry = u_glyphs.data[gl_InstanceIndex];
  vec2 uv = vec2(gl_VertexIndex % 2, gl_VertexIndex / 2);
  vec2 offset = uv * entry.size;
  gl_Position = vec4((entry.position + offset) / u_screen.size * 2.0 - 1.0,
                     0.0, 1.0);
  uv.x = uv.x > 0.5 ? entry.bottom_right_uv.x : entry.top_left_uv.x;
  uv.y = uv.y > 0.5 ? entry.bottom_right_uv.y : entry.top_left_uv.y;
  o_uv = uv;
  o_color = entry.color;
}
