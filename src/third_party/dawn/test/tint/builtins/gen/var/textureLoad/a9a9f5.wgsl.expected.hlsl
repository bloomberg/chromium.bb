Texture3D<uint4> arg_0 : register(t0, space1);

void textureLoad_a9a9f5() {
  int3 arg_1 = (0).xxx;
  int arg_2 = 0;
  uint4 res = arg_0.Load(int4(arg_1, arg_2));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureLoad_a9a9f5();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureLoad_a9a9f5();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureLoad_a9a9f5();
  return;
}
