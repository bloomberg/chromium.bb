void determinant_2b62ba() {
  float3x3 arg_0 = float3x3((0.0f).xxx, (0.0f).xxx, (0.0f).xxx);
  float res = determinant(arg_0);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  determinant_2b62ba();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  determinant_2b62ba();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  determinant_2b62ba();
  return;
}
