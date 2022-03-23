#version 310 es

void max_e8192f() {
  ivec2 res = max(ivec2(0, 0), ivec2(0, 0));
}

vec4 vertex_main() {
  max_e8192f();
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

void max_e8192f() {
  ivec2 res = max(ivec2(0, 0), ivec2(0, 0));
}

void fragment_main() {
  max_e8192f();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void max_e8192f() {
  ivec2 res = max(ivec2(0, 0), ivec2(0, 0));
}

void compute_main() {
  max_e8192f();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
