cbuffer cbuffer_u : register(b0, space0) {
  uint4 u[1];
};

[numthreads(1, 1, 1)]
void main() {
  const float4 x = asfloat(u[0]);
  return;
}
