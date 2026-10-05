#version 450

struct SelSSBOEntry {
  vec2 position;
  vec2 size;
  vec3 color;
};

layout(binding = 0) uniform SelUBO {
  vec2 screen_size;
} u_ubo;

layout(binding = 1) buffer SelSSBO {
  SelSSBOEntry regions[];
} u_ssbo;

layout(location = 0) in uint i_dummy;

layout(location = 0) out vec3 o_color;

void main() {
  SelSSBOEntry entry = u_ssbo.regions[gl_InstanceIndex];
  vec2 uv = vec2(gl_VertexIndex % 2, gl_VertexIndex / 2);
  vec2 offset = uv * entry.size;
  gl_Position = vec4((entry.position + offset) / u_ubo.screen_size * 2.0 - 1.0,
                     0.0, 1.0);
  o_color = entry.color;
}
