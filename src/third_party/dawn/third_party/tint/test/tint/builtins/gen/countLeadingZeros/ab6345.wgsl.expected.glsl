#version 310 es

uvec3 tint_count_leading_zeros(uvec3 v) {
  uvec3 x = uvec3(v);
  uvec3 b16 = mix(uvec3(0u), uvec3(16u), lessThanEqual(x, uvec3(65535u)));
  x = (x << b16);
  uvec3 b8 = mix(uvec3(0u), uvec3(8u), lessThanEqual(x, uvec3(16777215u)));
  x = (x << b8);
  uvec3 b4 = mix(uvec3(0u), uvec3(4u), lessThanEqual(x, uvec3(268435455u)));
  x = (x << b4);
  uvec3 b2 = mix(uvec3(0u), uvec3(2u), lessThanEqual(x, uvec3(1073741823u)));
  x = (x << b2);
  uvec3 b1 = mix(uvec3(0u), uvec3(1u), lessThanEqual(x, uvec3(2147483647u)));
  uvec3 is_zero = mix(uvec3(0u), uvec3(1u), equal(x, uvec3(0u)));
  return uvec3((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_ab6345() {
  uvec3 res = tint_count_leading_zeros(uvec3(0u, 0u, 0u));
}

vec4 vertex_main() {
  countLeadingZeros_ab6345();
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

uvec3 tint_count_leading_zeros(uvec3 v) {
  uvec3 x = uvec3(v);
  uvec3 b16 = mix(uvec3(0u), uvec3(16u), lessThanEqual(x, uvec3(65535u)));
  x = (x << b16);
  uvec3 b8 = mix(uvec3(0u), uvec3(8u), lessThanEqual(x, uvec3(16777215u)));
  x = (x << b8);
  uvec3 b4 = mix(uvec3(0u), uvec3(4u), lessThanEqual(x, uvec3(268435455u)));
  x = (x << b4);
  uvec3 b2 = mix(uvec3(0u), uvec3(2u), lessThanEqual(x, uvec3(1073741823u)));
  x = (x << b2);
  uvec3 b1 = mix(uvec3(0u), uvec3(1u), lessThanEqual(x, uvec3(2147483647u)));
  uvec3 is_zero = mix(uvec3(0u), uvec3(1u), equal(x, uvec3(0u)));
  return uvec3((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_ab6345() {
  uvec3 res = tint_count_leading_zeros(uvec3(0u, 0u, 0u));
}

void fragment_main() {
  countLeadingZeros_ab6345();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uvec3 tint_count_leading_zeros(uvec3 v) {
  uvec3 x = uvec3(v);
  uvec3 b16 = mix(uvec3(0u), uvec3(16u), lessThanEqual(x, uvec3(65535u)));
  x = (x << b16);
  uvec3 b8 = mix(uvec3(0u), uvec3(8u), lessThanEqual(x, uvec3(16777215u)));
  x = (x << b8);
  uvec3 b4 = mix(uvec3(0u), uvec3(4u), lessThanEqual(x, uvec3(268435455u)));
  x = (x << b4);
  uvec3 b2 = mix(uvec3(0u), uvec3(2u), lessThanEqual(x, uvec3(1073741823u)));
  x = (x << b2);
  uvec3 b1 = mix(uvec3(0u), uvec3(1u), lessThanEqual(x, uvec3(2147483647u)));
  uvec3 is_zero = mix(uvec3(0u), uvec3(1u), equal(x, uvec3(0u)));
  return uvec3((((((b16 | b8) | b4) | b2) | b1) + is_zero));
}

void countLeadingZeros_ab6345() {
  uvec3 res = tint_count_leading_zeros(uvec3(0u, 0u, 0u));
}

void compute_main() {
  countLeadingZeros_ab6345();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
