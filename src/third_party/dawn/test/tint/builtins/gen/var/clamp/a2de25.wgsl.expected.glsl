#version 310 es

void clamp_a2de25() {
  uint arg_0 = 1u;
  uint arg_1 = 1u;
  uint arg_2 = 1u;
  uint res = clamp(arg_0, arg_1, arg_2);
}

vec4 vertex_main() {
  clamp_a2de25();
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

void clamp_a2de25() {
  uint arg_0 = 1u;
  uint arg_1 = 1u;
  uint arg_2 = 1u;
  uint res = clamp(arg_0, arg_1, arg_2);
}

void fragment_main() {
  clamp_a2de25();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void clamp_a2de25() {
  uint arg_0 = 1u;
  uint arg_1 = 1u;
  uint arg_2 = 1u;
  uint res = clamp(arg_0, arg_1, arg_2);
}

void compute_main() {
  clamp_a2de25();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
