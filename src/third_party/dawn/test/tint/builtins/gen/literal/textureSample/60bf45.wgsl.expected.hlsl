Texture2DArray arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSample_60bf45() {
  float res = arg_0.Sample(arg_1, float3(0.0f, 0.0f, float(1)), (0).xx).x;
}

void fragment_main() {
  textureSample_60bf45();
  return;
}
