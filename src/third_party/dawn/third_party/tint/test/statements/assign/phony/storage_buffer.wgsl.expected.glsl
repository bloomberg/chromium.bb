#version 310 es

struct S {
  int i;
};

layout(binding = 0, std430) buffer S_1 {
  int i;
} s;
void tint_symbol() {
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
