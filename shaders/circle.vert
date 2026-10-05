#version 450

layout(binding = 0) uniform SelUBO {
  vec2 screen_size;
} u_ubo;

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec2 i_uv;
layout(location = 2) in vec3 i_color;

layout(location = 0) out vec2 o_uv;
layout(location = 1) out vec3 o_color;

void main() {
  gl_Position = vec4(i_position / u_ubo.screen_size * 2.0 - 1.0,
                     0.0, 1.0);
  o_uv = i_uv;
  o_color = i_color;
}
