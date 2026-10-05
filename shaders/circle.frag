#version 450

#define FADE 0.05

layout(location = 0) in vec2 i_uv;
layout(location = 1) in vec3 i_color;

layout(location = 0) out vec4 o_color;

void main() {
  if (i_uv.x < 0.0 || i_uv.y < 0.0)
    discard;
  float len = length(i_uv);
  o_color = vec4(i_color, smoothstep(0.5 + FADE, 0.5, len));
}
