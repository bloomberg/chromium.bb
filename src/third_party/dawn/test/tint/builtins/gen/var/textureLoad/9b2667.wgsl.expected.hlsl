Texture2DArray arg_0 : register(t0, space1);

void textureLoad_9b2667() {
  int2 arg_1 = (0).xx;
  int arg_2 = 1;
  int arg_3 = 0;
  float res = arg_0.Load(int4(int3(arg_1, arg_2), arg_3)).x;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureLoad_9b2667();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureLoad_9b2667();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureLoad_9b2667();
  return;
}
