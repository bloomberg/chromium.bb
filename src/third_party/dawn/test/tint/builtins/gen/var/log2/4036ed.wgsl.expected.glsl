#version 310 es

void log2_4036ed() {
  float arg_0 = 1.0f;
  float res = log2(arg_0);
}

vec4 vertex_main() {
  log2_4036ed();
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

void log2_4036ed() {
  float arg_0 = 1.0f;
  float res = log2(arg_0);
}

void fragment_main() {
  log2_4036ed();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void log2_4036ed() {
  float arg_0 = 1.0f;
  float res = log2(arg_0);
}

void compute_main() {
  log2_4036ed();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
