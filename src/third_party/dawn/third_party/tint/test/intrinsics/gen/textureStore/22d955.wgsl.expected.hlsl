RWTexture2DArray<uint4> arg_0 : register(u0, space1);

void textureStore_22d955() {
  arg_0[int3(0, 0, 1)] = uint4(0u, 0u, 0u, 0u);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_22d955();
  return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_22d955();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_22d955();
  return;
}
