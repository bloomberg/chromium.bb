Texture1D<uint4> arg_0 : register(t0, space1);

void textureLoad_1b8588() {
  int arg_1 = 1;
  int arg_2 = 0;
  uint4 res = arg_0.Load(int2(arg_1, arg_2));
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureLoad_1b8588();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureLoad_1b8588();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureLoad_1b8588();
  return;
}
