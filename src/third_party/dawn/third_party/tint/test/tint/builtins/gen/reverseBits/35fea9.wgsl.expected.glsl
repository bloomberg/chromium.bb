#version 310 es

void reverseBits_35fea9() {
  uvec4 res = bitfieldReverse(uvec4(0u, 0u, 0u, 0u));
}

vec4 vertex_main() {
  reverseBits_35fea9();
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

void reverseBits_35fea9() {
  uvec4 res = bitfieldReverse(uvec4(0u, 0u, 0u, 0u));
}

void fragment_main() {
  reverseBits_35fea9();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

void reverseBits_35fea9() {
  uvec4 res = bitfieldReverse(uvec4(0u, 0u, 0u, 0u));
}

void compute_main() {
  reverseBits_35fea9();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
