RWTexture2D<float4> arg_0 : register(u0, space1);

void textureStore_731349() {
  arg_0[(0).xx] = (0.0f).xxxx;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_731349();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_731349();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_731349();
  return;
}
