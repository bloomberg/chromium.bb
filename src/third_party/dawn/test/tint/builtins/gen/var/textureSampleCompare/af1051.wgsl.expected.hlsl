Texture2DArray arg_0 : register(t0, space1);
SamplerComparisonState arg_1 : register(s1, space1);

void textureSampleCompare_af1051() {
  float2 arg_2 = (0.0f).xx;
  int arg_3 = 1;
  float arg_4 = 1.0f;
  float res = arg_0.SampleCmp(arg_1, float3(arg_2, float(arg_3)), arg_4, (0).xx);
}

void fragment_main() {
  textureSampleCompare_af1051();
  return;
}
