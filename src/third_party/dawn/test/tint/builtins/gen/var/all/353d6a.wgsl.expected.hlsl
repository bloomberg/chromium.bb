void all_353d6a() {
  bool arg_0 = false;
  bool res = all(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  all_353d6a();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  all_353d6a();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  all_353d6a();
  return;
}
