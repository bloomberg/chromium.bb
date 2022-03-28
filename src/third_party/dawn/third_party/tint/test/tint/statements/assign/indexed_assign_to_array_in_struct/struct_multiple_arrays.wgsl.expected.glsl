#version 310 es

struct Uniforms {
  uint i;
};

struct InnerS {
  int v;
};

struct OuterS {
  InnerS a1[8];
  InnerS a2[8];
};

layout(binding = 4) uniform Uniforms_1 {
  uint i;
} uniforms;

void tint_symbol() {
  InnerS v = InnerS(0);
  OuterS s1 = OuterS(InnerS[8](InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0)), InnerS[8](InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0)));
  s1.a1[uniforms.i] = v;
  s1.a2[uniforms.i] = v;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
