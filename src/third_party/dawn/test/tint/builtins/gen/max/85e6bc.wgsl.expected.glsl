#version 310 es

void max_85e6bc() {
  ivec4 res = max(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0));
}

vec4 vertex_main() {
  max_85e6bc();
  return vec4(0.0f, 0.0f, 0.0f, 0.0f);
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

void max_85e6bc() {
  ivec4 res = max(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0));
}

void fragment_main() {
  max_85e6bc();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void max_85e6bc() {
  ivec4 res = max(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0));
}

void compute_main() {
  max_85e6bc();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
