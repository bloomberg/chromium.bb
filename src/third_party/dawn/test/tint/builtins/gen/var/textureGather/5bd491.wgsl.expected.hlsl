Texture2D<uint4> arg_1 : register(t1, space1);
SamplerState arg_2 : register(s2, space1);

void textureGather_5bd491() {
  float2 arg_3 = (0.0f).xx;
  uint4 res = arg_1.GatherGreen(arg_2, arg_3);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureGather_5bd491();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureGather_5bd491();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureGather_5bd491();
  return;
}
