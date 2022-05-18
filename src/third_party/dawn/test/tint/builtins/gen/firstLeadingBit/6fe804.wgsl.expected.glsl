#version 310 es

uvec2 tint_first_leading_bit(uvec2 v) {
  uvec2 x = v;
  uvec2 b16 = mix(uvec2(0u), uvec2(16u), bvec2((x & uvec2(4294901760u))));
  x = (x >> b16);
  uvec2 b8 = mix(uvec2(0u), uvec2(8u), bvec2((x & uvec2(65280u))));
  x = (x >> b8);
  uvec2 b4 = mix(uvec2(0u), uvec2(4u), bvec2((x & uvec2(240u))));
  x = (x >> b4);
  uvec2 b2 = mix(uvec2(0u), uvec2(2u), bvec2((x & uvec2(12u))));
  x = (x >> b2);
  uvec2 b1 = mix(uvec2(0u), uvec2(1u), bvec2((x & uvec2(2u))));
  uvec2 is_zero = mix(uvec2(0u), uvec2(4294967295u), equal(x, uvec2(0u)));
  return uvec2((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_6fe804() {
  uvec2 res = tint_first_leading_bit(uvec2(0u, 0u));
}

vec4 vertex_main() {
  firstLeadingBit_6fe804();
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

uvec2 tint_first_leading_bit(uvec2 v) {
  uvec2 x = v;
  uvec2 b16 = mix(uvec2(0u), uvec2(16u), bvec2((x & uvec2(4294901760u))));
  x = (x >> b16);
  uvec2 b8 = mix(uvec2(0u), uvec2(8u), bvec2((x & uvec2(65280u))));
  x = (x >> b8);
  uvec2 b4 = mix(uvec2(0u), uvec2(4u), bvec2((x & uvec2(240u))));
  x = (x >> b4);
  uvec2 b2 = mix(uvec2(0u), uvec2(2u), bvec2((x & uvec2(12u))));
  x = (x >> b2);
  uvec2 b1 = mix(uvec2(0u), uvec2(1u), bvec2((x & uvec2(2u))));
  uvec2 is_zero = mix(uvec2(0u), uvec2(4294967295u), equal(x, uvec2(0u)));
  return uvec2((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_6fe804() {
  uvec2 res = tint_first_leading_bit(uvec2(0u, 0u));
}

void fragment_main() {
  firstLeadingBit_6fe804();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uvec2 tint_first_leading_bit(uvec2 v) {
  uvec2 x = v;
  uvec2 b16 = mix(uvec2(0u), uvec2(16u), bvec2((x & uvec2(4294901760u))));
  x = (x >> b16);
  uvec2 b8 = mix(uvec2(0u), uvec2(8u), bvec2((x & uvec2(65280u))));
  x = (x >> b8);
  uvec2 b4 = mix(uvec2(0u), uvec2(4u), bvec2((x & uvec2(240u))));
  x = (x >> b4);
  uvec2 b2 = mix(uvec2(0u), uvec2(2u), bvec2((x & uvec2(12u))));
  x = (x >> b2);
  uvec2 b1 = mix(uvec2(0u), uvec2(1u), bvec2((x & uvec2(2u))));
  uvec2 is_zero = mix(uvec2(0u), uvec2(4294967295u), equal(x, uvec2(0u)));
  return uvec2((((((b16 | b8) | b4) | b2) | b1) | is_zero));
}

void firstLeadingBit_6fe804() {
  uvec2 res = tint_first_leading_bit(uvec2(0u, 0u));
}

void compute_main() {
  firstLeadingBit_6fe804();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
