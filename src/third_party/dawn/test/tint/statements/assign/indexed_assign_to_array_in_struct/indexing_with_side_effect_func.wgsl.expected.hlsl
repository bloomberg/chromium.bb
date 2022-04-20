struct InnerS {
  int v;
};
struct S1 {
  InnerS a2[8];
};
struct OuterS {
  S1 a1[8];
};

static uint nextIndex = 0u;

uint getNextIndex() {
  nextIndex = (nextIndex + 1u);
  return nextIndex;
}

cbuffer cbuffer_uniforms : register(b4, space1) {
  uint4 uniforms[1];
};

[numthreads(1, 1, 1)]
void main() {
  InnerS v = (InnerS)0;
  OuterS s = (OuterS)0;
  {
    S1 tint_symbol_1[8] = s.a1;
    const uint tint_symbol_4 = getNextIndex();
    const uint tint_symbol_2_save = tint_symbol_4;
    InnerS tint_symbol_3[8] = tint_symbol_1[tint_symbol_2_save].a2;
    tint_symbol_3[uniforms[0].y] = v;
    tint_symbol_1[tint_symbol_2_save].a2 = tint_symbol_3;
    s.a1 = tint_symbol_1;
  }
  return;
}
