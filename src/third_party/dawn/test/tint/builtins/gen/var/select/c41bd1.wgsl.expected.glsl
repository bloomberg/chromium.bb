#version 310 es

void select_c41bd1() {
  bvec4 arg_0 = bvec4(false);
  bvec4 arg_1 = bvec4(false);
  bool arg_2 = false;
  bvec4 res = (arg_2 ? arg_1 : arg_0);
}

vec4 vertex_main() {
  select_c41bd1();
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

void select_c41bd1() {
  bvec4 arg_0 = bvec4(false);
  bvec4 arg_1 = bvec4(false);
  bool arg_2 = false;
  bvec4 res = (arg_2 ? arg_1 : arg_0);
}

void fragment_main() {
  select_c41bd1();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_c41bd1() {
  bvec4 arg_0 = bvec4(false);
  bvec4 arg_1 = bvec4(false);
  bool arg_2 = false;
  bvec4 res = (arg_2 ? arg_1 : arg_0);
}

void compute_main() {
  select_c41bd1();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
