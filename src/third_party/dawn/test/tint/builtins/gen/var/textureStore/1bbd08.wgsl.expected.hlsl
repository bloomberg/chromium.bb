RWTexture3D<float4> arg_0 : register(u0, space1);

void textureStore_1bbd08() {
  int3 arg_1 = (0).xxx;
  float4 arg_2 = (0.0f).xxxx;
  arg_0[arg_1] = arg_2;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_1bbd08();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_1bbd08();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_1bbd08();
  return;
}
