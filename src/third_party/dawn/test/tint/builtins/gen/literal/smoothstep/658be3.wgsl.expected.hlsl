builtins/gen/literal/smoothstep/658be3.wgsl:28:24 warning: use of deprecated builtin
  var res: vec3<f32> = smoothStep(vec3<f32>(), vec3<f32>(), vec3<f32>());
                       ^^^^^^^^^^

void smoothStep_658be3() {
  float3 res = smoothstep((0.0f).xxx, (0.0f).xxx, (0.0f).xxx);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  smoothStep_658be3();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  smoothStep_658be3();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  smoothStep_658be3();
  return;
}
