Texture2D<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSampleGrad_521263() {
  float2 arg_2 = (0.0f).xx;
  float2 arg_3 = (0.0f).xx;
  float2 arg_4 = (0.0f).xx;
  float4 res = arg_0.SampleGrad(arg_1, arg_2, arg_3, arg_4);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureSampleGrad_521263();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureSampleGrad_521263();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureSampleGrad_521263();
  return;
}
