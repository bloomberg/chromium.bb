RWTexture2DArray<float4> arg_0 : register(u0, space1);

void textureNumLayers_e31be1() {
  int3 tint_tmp;
  arg_0.GetDimensions(tint_tmp.x, tint_tmp.y, tint_tmp.z);
  int res = tint_tmp.z;
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureNumLayers_e31be1();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureNumLayers_e31be1();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureNumLayers_e31be1();
  return;
}
