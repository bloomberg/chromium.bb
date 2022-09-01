#version 310 es

layout(location = 0) flat out int loc0_1;
layout(location = 1) flat out uint loc1_1;
layout(location = 2) out float loc2_1;
layout(location = 3) out vec4 loc3_1;
struct VertexOutputs {
  int loc0;
  uint loc1;
  float loc2;
  vec4 loc3;
  vec4 position;
};

VertexOutputs tint_symbol() {
  VertexOutputs tint_symbol_1 = VertexOutputs(1, 1u, 1.0f, vec4(1.0f, 2.0f, 3.0f, 4.0f), vec4(0.0f));
  return tint_symbol_1;
}

void main() {
  gl_PointSize = 1.0;
  VertexOutputs inner_result = tint_symbol();
  loc0_1 = inner_result.loc0;
  loc1_1 = inner_result.loc1;
  loc2_1 = inner_result.loc2;
  loc3_1 = inner_result.loc3;
  gl_Position = inner_result.position;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
