TextureCubeArray<float4> arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSample_4dd1bf() {
  float3 arg_2 = (0.0f).xxx;
  int arg_3 = 1;
  float4 res = arg_0.Sample(arg_1, float4(arg_2, float(arg_3)));
}

void fragment_main() {
  textureSample_4dd1bf();
  return;
}
