#version 310 es

void select_c4a4ef() {
  uvec4 arg_0 = uvec4(0u);
  uvec4 arg_1 = uvec4(0u);
  bvec4 arg_2 = bvec4(false);
  uvec4 res = mix(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  select_c4a4ef();
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

void select_c4a4ef() {
  uvec4 arg_0 = uvec4(0u);
  uvec4 arg_1 = uvec4(0u);
  bvec4 arg_2 = bvec4(false);
  uvec4 res = mix(arg_0, arg_1, arg_2);
}

void fragment_main() {
  select_c4a4ef();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_c4a4ef() {
  uvec4 arg_0 = uvec4(0u);
  uvec4 arg_1 = uvec4(0u);
  bvec4 arg_2 = bvec4(false);
  uvec4 res = mix(arg_0, arg_1, arg_2);
}

void compute_main() {
  select_c4a4ef();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
