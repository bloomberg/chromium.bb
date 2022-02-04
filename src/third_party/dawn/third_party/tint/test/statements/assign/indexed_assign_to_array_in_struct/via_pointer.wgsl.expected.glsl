#version 310 es
precision mediump float;

struct Uniforms {
  uint i;
};
struct InnerS {
  int v;
};
struct OuterS {
  InnerS a1[8];
};

layout (binding = 4) uniform Uniforms_1 {
  uint i;
} uniforms;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void tint_symbol() {
  InnerS v = InnerS(0);
  OuterS s1 = OuterS(InnerS[8](InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0), InnerS(0)));
  uint p_save = uniforms.i;
  s1.a1[p_save] = v;
  return;
}
void main() {
  tint_symbol();
}


