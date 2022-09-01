#version 310 es

void select_bb8aae() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  bvec4 arg_2 = bvec4(false);
  vec4 res = mix(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  select_bb8aae();
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

void select_bb8aae() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  bvec4 arg_2 = bvec4(false);
  vec4 res = mix(arg_0, arg_1, arg_2);
}

void fragment_main() {
  select_bb8aae();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_bb8aae() {
  vec4 arg_0 = vec4(0.0f);
  vec4 arg_1 = vec4(0.0f);
  bvec4 arg_2 = bvec4(false);
  vec4 res = mix(arg_0, arg_1, arg_2);
}

void compute_main() {
  select_bb8aae();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
