cbuffer cbuffer_ubo : register(b0, space0) {
  uint4 ubo[1];
};

struct S {
  int data[64];
};

RWByteAddressBuffer result : register(u1, space0);

void x(inout S p) {
  {
    int tint_symbol_2[64] = p.data;
    tint_symbol_2[asint(ubo[0].x)] = 1;
    p.data = tint_symbol_2;
  }
}

[numthreads(1, 1, 1)]
void f() {
  S s = (S)0;
  x(s);
  result.Store(0u, asuint(s.data[3]));
  return;
}
