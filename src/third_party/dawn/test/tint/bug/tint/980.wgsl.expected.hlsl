void set_float3(inout float3 vec, int idx, float val) {
  vec = (idx.xxx == int3(0, 1, 2)) ? val.xxx : vec;
}

float3 Bad(uint index, float3 rd) {
  float3 normal = (0.0f).xxx;
  set_float3(normal, index, -(sign(rd[index])));
  return normalize(normal);
}

RWByteAddressBuffer io : register(u0, space0);

struct tint_symbol_1 {
  uint idx : SV_GroupIndex;
};

void main_inner(uint idx) {
  const float3 tint_symbol_2 = Bad(io.Load(12u), asfloat(io.Load3(0u)));
  io.Store3(0u, asuint(tint_symbol_2));
}

[numthreads(1, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.idx);
  return;
}
