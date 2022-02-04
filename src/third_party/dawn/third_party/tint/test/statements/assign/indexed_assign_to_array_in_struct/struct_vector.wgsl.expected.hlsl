void set_float3(inout float3 vec, int idx, float val) {
  vec = (idx.xxx == int3(0, 1, 2)) ? val.xxx : vec;
}

struct OuterS {
  float3 v1;
};

cbuffer cbuffer_uniforms : register(b4, space1) {
  uint4 uniforms[1];
};

[numthreads(1, 1, 1)]
void main() {
  OuterS s1 = (OuterS)0;
  set_float3(s1.v1, uniforms[0].x, 1.0f);
  return;
}
