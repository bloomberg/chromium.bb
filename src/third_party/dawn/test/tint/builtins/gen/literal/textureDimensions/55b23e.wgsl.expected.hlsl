RWTexture1D<float4> arg_0 : register(u0, space1);

void textureDimensions_55b23e() {
  int tint_tmp;
  arg_0.GetDimensions(tint_tmp);
  int res = tint_tmp;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureDimensions_55b23e();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureDimensions_55b23e();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureDimensions_55b23e();
  return;
}
