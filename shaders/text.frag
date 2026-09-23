#version 450

layout(binding = 2) uniform sampler2D u_atlas;

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec3 i_color;

layout(location = 0) out vec4 o_color;

void main() {
  o_color = vec4(i_color, texture(u_atlas, i_uv).r);
}
