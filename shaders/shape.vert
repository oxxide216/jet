#version 450

layout(binding = 0) uniform ShapeUBO {
  vec2 screen_size;
} u_ubo;

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec4 i_color;

layout(location = 0) out vec4 o_color;

void main() {
  gl_Position = vec4(i_position / u_ubo.screen_size * 2.0 - 1.0,
                     0.0, 1.0);
  o_color = i_color;
}
