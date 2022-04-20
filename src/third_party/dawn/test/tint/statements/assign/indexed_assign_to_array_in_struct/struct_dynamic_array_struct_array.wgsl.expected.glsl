#version 310 es

struct Uniforms {
  uint i;
  uint j;
};

struct InnerS {
  int v;
};

struct S1 {
  InnerS a2[8];
};

layout(binding = 4) uniform Uniforms_1 {
  uint i;
  uint j;
} uniforms;

layout(binding = 0, std430) buffer OuterS_1 {
  S1 a1[];
} s;
void tint_symbol() {
  InnerS v = InnerS(0);
  s.a1[uniforms.i].a2[uniforms.j] = v;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
