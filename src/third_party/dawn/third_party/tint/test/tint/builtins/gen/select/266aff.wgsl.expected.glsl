#version 310 es

void select_266aff() {
  vec2 res = mix(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), bvec2(false, false));
}

vec4 vertex_main() {
  select_266aff();
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

void select_266aff() {
  vec2 res = mix(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), bvec2(false, false));
}

void fragment_main() {
  select_266aff();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void select_266aff() {
  vec2 res = mix(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), bvec2(false, false));
}

void compute_main() {
  select_266aff();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
