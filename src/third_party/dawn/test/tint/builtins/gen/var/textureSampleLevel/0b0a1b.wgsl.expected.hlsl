Texture2D<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSampleLevel_0b0a1b() {
  float2 arg_2 = (0.0f).xx;
  float arg_3 = 1.0f;
  float4 res = arg_0.SampleLevel(arg_1, arg_2, arg_3, (0).xx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureSampleLevel_0b0a1b();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureSampleLevel_0b0a1b();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureSampleLevel_0b0a1b();
  return;
}
