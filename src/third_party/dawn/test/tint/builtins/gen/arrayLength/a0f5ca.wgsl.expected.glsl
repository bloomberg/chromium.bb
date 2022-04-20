#version 310 es

layout(binding = 1, std430) buffer SB_RO_1 {
  float arg_0[];
} sb_ro;
void arrayLength_a0f5ca() {
  uint res = uint(sb_ro.arg_0.length());
}

vec4 vertex_main() {
  arrayLength_a0f5ca();
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
}

void main() {
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

layout(binding = 1, std430) buffer SB_RO_1 {
  float arg_0[];
} sb_ro;
void arrayLength_a0f5ca() {
  uint res = uint(sb_ro.arg_0.length());
}

void fragment_main() {
  arrayLength_a0f5ca();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 1, std430) buffer SB_RO_1 {
  float arg_0[];
} sb_ro;
void arrayLength_a0f5ca() {
  uint res = uint(sb_ro.arg_0.length());
}

void compute_main() {
  arrayLength_a0f5ca();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
