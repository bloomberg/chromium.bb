void tan_2f030e() {
  float arg_0 = 1.0f;
  float res = tan(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  tan_2f030e();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  tan_2f030e();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  tan_2f030e();
  return;
}
