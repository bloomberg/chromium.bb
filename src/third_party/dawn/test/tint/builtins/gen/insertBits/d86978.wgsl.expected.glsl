#version 310 es

ivec4 tint_insert_bits(ivec4 v, ivec4 n, uint offset, uint count) {
  uint s = min(offset, 32u);
  uint e = min(32u, (s + count));
  return bitfieldInsert(v, n, int(s), int((e - s)));
}

void insertBits_d86978() {
  ivec4 res = tint_insert_bits(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), 1u, 1u);
}

vec4 vertex_main() {
  insertBits_d86978();
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

ivec4 tint_insert_bits(ivec4 v, ivec4 n, uint offset, uint count) {
  uint s = min(offset, 32u);
  uint e = min(32u, (s + count));
  return bitfieldInsert(v, n, int(s), int((e - s)));
}

void insertBits_d86978() {
  ivec4 res = tint_insert_bits(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), 1u, 1u);
}

void fragment_main() {
  insertBits_d86978();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

ivec4 tint_insert_bits(ivec4 v, ivec4 n, uint offset, uint count) {
  uint s = min(offset, 32u);
  uint e = min(32u, (s + count));
  return bitfieldInsert(v, n, int(s), int((e - s)));
}

void insertBits_d86978() {
  ivec4 res = tint_insert_bits(ivec4(0, 0, 0, 0), ivec4(0, 0, 0, 0), 1u, 1u);
}

void compute_main() {
  insertBits_d86978();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
