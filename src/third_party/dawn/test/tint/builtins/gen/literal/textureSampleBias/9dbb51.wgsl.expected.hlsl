Texture2DArray<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSampleBias_9dbb51() {
  float4 res = arg_0.SampleBias(arg_1, float3(0.0f, 0.0f, float(1)), 1.0f, (0).xx);
}

void fragment_main() {
  textureSampleBias_9dbb51();
  return;
}
