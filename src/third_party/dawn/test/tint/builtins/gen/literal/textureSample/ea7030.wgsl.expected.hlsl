TextureCube arg_0 : register(t0, space1);
SamplerState arg_1 : register(s1, space1);

void textureSample_ea7030() {
  float res = arg_0.Sample(arg_1, (0.0f).xxx).x;
}

void fragment_main() {
  textureSample_ea7030();
  return;
}
