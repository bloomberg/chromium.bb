void distance_cfed73() {
  float arg_0 = 1.0f;
  float arg_1 = 1.0f;
  float res = distance(arg_0, arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  distance_cfed73();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  distance_cfed73();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  distance_cfed73();
  return;
}
