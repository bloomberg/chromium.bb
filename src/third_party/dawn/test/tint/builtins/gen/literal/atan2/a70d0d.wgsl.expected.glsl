#version 310 es

void atan2_a70d0d() {
  vec3 res = atan(vec3(0.0f), vec3(0.0f));
}

vec4 vertex_main() {
  atan2_a70d0d();
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

void atan2_a70d0d() {
  vec3 res = atan(vec3(0.0f), vec3(0.0f));
}

void fragment_main() {
  atan2_a70d0d();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void atan2_a70d0d() {
  vec3 res = atan(vec3(0.0f), vec3(0.0f));
}

void compute_main() {
  atan2_a70d0d();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
