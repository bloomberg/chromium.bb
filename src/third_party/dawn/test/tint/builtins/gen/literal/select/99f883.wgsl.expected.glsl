#version 310 es

void select_99f883() {
  uint res = (false ? 1u : 1u);
}

vec4 vertex_main() {
  select_99f883();
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

void select_99f883() {
  uint res = (false ? 1u : 1u);
}

void fragment_main() {
  select_99f883();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_99f883() {
  uint res = (false ? 1u : 1u);
}

void compute_main() {
  select_99f883();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
