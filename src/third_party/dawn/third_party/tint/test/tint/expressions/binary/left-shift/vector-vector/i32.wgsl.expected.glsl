#version 310 es

void f() {
  ivec3 a = ivec3(1, 2, 3);
  uvec3 b = uvec3(4u, 5u, 6u);
  ivec3 r = (a << b);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  f();
  return;
}
