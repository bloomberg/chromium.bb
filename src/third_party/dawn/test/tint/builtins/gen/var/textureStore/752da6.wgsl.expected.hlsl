RWTexture2D<int4> arg_0 : register(u0, space1);

void textureStore_752da6() {
  int2 arg_1 = (0).xx;
  int4 arg_2 = (0).xxxx;
  arg_0[arg_1] = arg_2;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureStore_752da6();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureStore_752da6();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureStore_752da6();
  return;
}
