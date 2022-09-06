#version 310 es

uint tint_int_dot(uvec2 a, uvec2 b) {
  return a[0]*b[0] + a[1]*b[1];
}

void dot_97c7ee() {
  uint res = tint_int_dot(uvec2(0u), uvec2(0u));
}

vec4 vertex_main() {
  dot_97c7ee();
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

uint tint_int_dot(uvec2 a, uvec2 b) {
  return a[0]*b[0] + a[1]*b[1];
}

void dot_97c7ee() {
  uint res = tint_int_dot(uvec2(0u), uvec2(0u));
}

void fragment_main() {
  dot_97c7ee();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uint tint_int_dot(uvec2 a, uvec2 b) {
  return a[0]*b[0] + a[1]*b[1];
}

void dot_97c7ee() {
  uint res = tint_int_dot(uvec2(0u), uvec2(0u));
}

void compute_main() {
  dot_97c7ee();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
