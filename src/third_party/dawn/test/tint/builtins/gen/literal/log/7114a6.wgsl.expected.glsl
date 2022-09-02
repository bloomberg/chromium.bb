#version 310 es

void log_7114a6() {
  float res = log(1.0f);
}

vec4 vertex_main() {
  log_7114a6();
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

void log_7114a6() {
  float res = log(1.0f);
}

void fragment_main() {
  log_7114a6();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void log_7114a6() {
  float res = log(1.0f);
}

void compute_main() {
  log_7114a6();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
