void distance_0657d4() {
  float3 arg_0 = (0.0f).xxx;
  float3 arg_1 = (0.0f).xxx;
  float res = distance(arg_0, arg_1);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  distance_0657d4();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  distance_0657d4();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  distance_0657d4();
  return;
}
