#version 310 es

layout(binding = 0, std430) buffer SB_RW_1 {
  uint arg_0[];
} sb_rw;
void arrayLength_eb510f() {
  uint res = uint(sb_rw.arg_0.length());
}

vec4 vertex_main() {
  arrayLength_eb510f();
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

layout(binding = 0, std430) buffer SB_RW_1 {
  uint arg_0[];
} sb_rw;
void arrayLength_eb510f() {
  uint res = uint(sb_rw.arg_0.length());
}

void fragment_main() {
  arrayLength_eb510f();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

layout(binding = 0, std430) buffer SB_RW_1 {
  uint arg_0[];
} sb_rw;
void arrayLength_eb510f() {
  uint res = uint(sb_rw.arg_0.length());
}

void compute_main() {
  arrayLength_eb510f();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
