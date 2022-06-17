#version 310 es

layout(location = 0) in int loc0_1;
layout(location = 1) in uint loc1_1;
layout(location = 2) in float loc2_1;
layout(location = 3) in vec4 loc3_1;
struct VertexInputs0 {
  uint vertex_index;
  int loc0;
};

struct VertexInputs1 {
  float loc2;
  vec4 loc3;
};

vec4 tint_symbol(VertexInputs0 inputs0, uint loc1, uint instance_index, VertexInputs1 inputs1) {
  uint foo = (inputs0.vertex_index + instance_index);
  int i = inputs0.loc0;
  uint u = loc1;
  float f = inputs1.loc2;
  vec4 v = inputs1.loc3;
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  VertexInputs0 tint_symbol_1 = VertexInputs0(uint(gl_VertexID), loc0_1);
  VertexInputs1 tint_symbol_2 = VertexInputs1(loc2_1, loc3_1);
  vec4 inner_result = tint_symbol(tint_symbol_1, loc1_1, uint(gl_InstanceID), tint_symbol_2);
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
