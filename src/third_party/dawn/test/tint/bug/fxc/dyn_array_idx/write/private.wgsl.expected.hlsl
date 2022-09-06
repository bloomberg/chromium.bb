cbuffer cbuffer_ubo : register(b0, space0) {
  uint4 ubo[1];
};

struct S {
  int data[64];
};

RWByteAddressBuffer result : register(u1, space0);
static S s = (S)0;

[numthreads(1, 1, 1)]
void f() {
  {
    int tint_symbol_2[64] = s.data;
    tint_symbol_2[asint(ubo[0].x)] = 1;
    s.data = tint_symbol_2;
  }
  result.Store(0u, asuint(s.data[3]));
  return;
}
