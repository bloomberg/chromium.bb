TextureCubeArray arg_0 : register(t0, space1);
SamplerComparisonState arg_1 : register(s1, space1);

void textureSampleCompare_a3ca7e() {
  float3 arg_2 = (0.0f).xxx;
  int arg_3 = 1;
  float arg_4 = 1.0f;
  float res = arg_0.SampleCmp(arg_1, float4(arg_2, float(arg_3)), arg_4);
}

void fragment_main() {
  textureSampleCompare_a3ca7e();
  return;
}
