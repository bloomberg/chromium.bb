#version 310 es

void distance_cfed73() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float res = distance(arg_0, arg_1);
}

vec4 vertex_main() {
  distance_cfed73();
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

void distance_cfed73() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float res = distance(arg_0, arg_1);
}

void fragment_main() {
  distance_cfed73();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void distance_cfed73() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float res = distance(arg_0, arg_1);
}

void compute_main() {
  distance_cfed73();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
