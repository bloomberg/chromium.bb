void refract_cbc1d2() {
  float3 arg_0 = (0.0f).xxx;
  float3 arg_1 = (0.0f).xxx;
  float arg_2 = 1.0f;
  float3 res = refract(arg_0, arg_1, arg_2);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  refract_cbc1d2();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  refract_cbc1d2();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  refract_cbc1d2();
  return;
}
