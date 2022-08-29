#version 310 es

int tint_first_leading_bit(int v) {
  uint x = ((v < 0) ? uint(~(v)) : uint(v));
  uint b16 = (bool((x & 4294901760u)) ? 16u : 0u);
  x = (x >> b16);
  uint b8 = (bool((x & 65280u)) ? 8u : 0u);
  x = (x >> b8);
  uint b4 = (bool((x & 240u)) ? 4u : 0u);
  x = (x >> b4);
  uint b2 = (bool((x & 12u)) ? 2u : 0u);
  x = (x >> b2);
  uint b1 = (bool((x & 2u)) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 4294967295u : 0u);
  return int((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  int res = tint_first_leading_bit(arg_0);
}

vec4 vertex_main() {
  firstLeadingBit_57a1a3();
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

int tint_first_leading_bit(int v) {
  uint x = ((v < 0) ? uint(~(v)) : uint(v));
  uint b16 = (bool((x & 4294901760u)) ? 16u : 0u);
  x = (x >> b16);
  uint b8 = (bool((x & 65280u)) ? 8u : 0u);
  x = (x >> b8);
  uint b4 = (bool((x & 240u)) ? 4u : 0u);
  x = (x >> b4);
  uint b2 = (bool((x & 12u)) ? 2u : 0u);
  x = (x >> b2);
  uint b1 = (bool((x & 2u)) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 4294967295u : 0u);
  return int((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  int res = tint_first_leading_bit(arg_0);
}

void fragment_main() {
  firstLeadingBit_57a1a3();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

int tint_first_leading_bit(int v) {
  uint x = ((v < 0) ? uint(~(v)) : uint(v));
  uint b16 = (bool((x & 4294901760u)) ? 16u : 0u);
  x = (x >> b16);
  uint b8 = (bool((x & 65280u)) ? 8u : 0u);
  x = (x >> b8);
  uint b4 = (bool((x & 240u)) ? 4u : 0u);
  x = (x >> b4);
  uint b2 = (bool((x & 12u)) ? 2u : 0u);
  x = (x >> b2);
  uint b1 = (bool((x & 2u)) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 4294967295u : 0u);
  return int((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_57a1a3() {
  int arg_0 = 1;
  int res = tint_first_leading_bit(arg_0);
}

void compute_main() {
  firstLeadingBit_57a1a3();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
