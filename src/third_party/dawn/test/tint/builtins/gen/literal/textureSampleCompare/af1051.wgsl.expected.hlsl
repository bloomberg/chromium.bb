Texture2DArray arg_0 : register(t0, space1);
SamplerComparisonState arg_1 : register(s1, space1);

void textureSampleCompare_af1051() {
  float res = arg_0.SampleCmp(arg_1, float3(0.0f, 0.0f, float(1)), 1.0f, (0).xx);
}

void fragment_main() {
  textureSampleCompare_af1051();
  return;
}
