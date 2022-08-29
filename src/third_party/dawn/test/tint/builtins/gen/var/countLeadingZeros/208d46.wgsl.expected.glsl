#version 310 es

uint tint_count_leading_zeros(uint v) {
  uint x = uint(v);
  uint b16 = ((x <= 65535u) ? 16u : 0u);
  x = (x << b16);
  uint b8 = ((x <= 16777215u) ? 8u : 0u);
  x = (x << b8);
  uint b4 = ((x <= 268435455u) ? 4u : 0u);
  x = (x << b4);
  uint b2 = ((x <= 1073741823u) ? 2u : 0u);
  x = (x << b2);
  uint b1 = ((x <= 2147483647u) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 1u : 0u);
  return uint((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_208d46() {
  uint arg_0 = 1u;
  uint res = tint_count_leading_zeros(arg_0);
}

vec4 vertex_main() {
  countLeadingZeros_208d46();
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

uint tint_count_leading_zeros(uint v) {
  uint x = uint(v);
  uint b16 = ((x <= 65535u) ? 16u : 0u);
  x = (x << b16);
  uint b8 = ((x <= 16777215u) ? 8u : 0u);
  x = (x << b8);
  uint b4 = ((x <= 268435455u) ? 4u : 0u);
  x = (x << b4);
  uint b2 = ((x <= 1073741823u) ? 2u : 0u);
  x = (x << b2);
  uint b1 = ((x <= 2147483647u) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 1u : 0u);
  return uint((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_208d46() {
  uint arg_0 = 1u;
  uint res = tint_count_leading_zeros(arg_0);
}

void fragment_main() {
  countLeadingZeros_208d46();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uint tint_count_leading_zeros(uint v) {
  uint x = uint(v);
  uint b16 = ((x <= 65535u) ? 16u : 0u);
  x = (x << b16);
  uint b8 = ((x <= 16777215u) ? 8u : 0u);
  x = (x << b8);
  uint b4 = ((x <= 268435455u) ? 4u : 0u);
  x = (x << b4);
  uint b2 = ((x <= 1073741823u) ? 2u : 0u);
  x = (x << b2);
  uint b1 = ((x <= 2147483647u) ? 1u : 0u);
  uint is_zero = ((x == 0u) ? 1u : 0u);
  return uint((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_208d46() {
  uint arg_0 = 1u;
  uint res = tint_count_leading_zeros(arg_0);
}

void compute_main() {
  countLeadingZeros_208d46();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
