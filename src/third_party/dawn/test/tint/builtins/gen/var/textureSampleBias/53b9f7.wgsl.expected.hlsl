TextureCube<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSampleBias_53b9f7() {
  float3 arg_2 = (0.0f).xxx;
  float arg_3 = 1.0f;
  float4 res = arg_0.SampleBias(arg_1, arg_2, arg_3);
}

void fragment_main() {
  textureSampleBias_53b9f7();
  return;
}
