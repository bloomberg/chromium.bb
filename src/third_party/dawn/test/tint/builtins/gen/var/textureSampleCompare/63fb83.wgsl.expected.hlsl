TextureCube arg_0 : register(t0, space1);
SamplerComparisonState arg_1 : register(s1, space1);

void textureSampleCompare_63fb83() {
  float3 arg_2 = (0.0f).xxx;
  float arg_3 = 1.0f;
  float res = arg_0.SampleCmp(arg_1, arg_2, arg_3);
}

void fragment_main() {
  textureSampleCompare_63fb83();
  return;
}
