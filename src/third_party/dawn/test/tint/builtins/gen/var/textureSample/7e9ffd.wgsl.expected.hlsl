Texture2DArray arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSample_7e9ffd() {
  float2 arg_2 = (0.0f).xx;
  int arg_3 = 1;
  float res = arg_0.Sample(arg_1, float3(arg_2, float(arg_3))).x;
}

void fragment_main() {
  textureSample_7e9ffd();
  return;
}
