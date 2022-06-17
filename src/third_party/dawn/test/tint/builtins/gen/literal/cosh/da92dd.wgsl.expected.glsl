#version 310 es

void cosh_da92dd() {
  float res = cosh(1.0f);
}

vec4 vertex_main() {
  cosh_da92dd();
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

void cosh_da92dd() {
  float res = cosh(1.0f);
}

void fragment_main() {
  cosh_da92dd();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void cosh_da92dd() {
  float res = cosh(1.0f);
}

void compute_main() {
  cosh_da92dd();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
