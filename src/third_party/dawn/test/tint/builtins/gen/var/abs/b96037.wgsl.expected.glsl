#version 310 es

void abs_b96037() {
  float arg_0 = 1.0f;
  float res = abs(arg_0);
}

vec4 vertex_main() {
  abs_b96037();
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

void abs_b96037() {
  float arg_0 = 1.0f;
  float res = abs(arg_0);
}

void fragment_main() {
  abs_b96037();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void abs_b96037() {
  float arg_0 = 1.0f;
  float res = abs(arg_0);
}

void compute_main() {
  abs_b96037();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
