#version 310 es

void select_b04721() {
  uvec3 arg_0 = uvec3(0u);
  uvec3 arg_1 = uvec3(0u);
  bool arg_2 = false;
  uvec3 res = (arg_2 ? arg_1 : arg_0);
}

vec4 vertex_main() {
  select_b04721();
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

void select_b04721() {
  uvec3 arg_0 = uvec3(0u);
  uvec3 arg_1 = uvec3(0u);
  bool arg_2 = false;
  uvec3 res = (arg_2 ? arg_1 : arg_0);
}

void fragment_main() {
  select_b04721();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_b04721() {
  uvec3 arg_0 = uvec3(0u);
  uvec3 arg_1 = uvec3(0u);
  bool arg_2 = false;
  uvec3 res = (arg_2 ? arg_1 : arg_0);
}

void compute_main() {
  select_b04721();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
