#version 310 es

struct u_block {
  uvec3 inner;
};

layout(binding = 0) uniform u_block_1 {
  uvec3 inner;
} u;

void tint_symbol() {
  uvec3 x = u.inner;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
