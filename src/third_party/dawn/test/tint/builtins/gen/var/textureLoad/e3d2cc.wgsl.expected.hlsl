Texture2DMS<int4> arg_0 : register(t0, space1);

void textureLoad_e3d2cc() {
  int2 arg_1 = (0).xx;
  int arg_2 = 1;
  int4 res = arg_0.Load(arg_1, arg_2);
}

struct tint_symbol {
  float4 value : SV_Position;
};

float4 vertex_main_inner() {
  textureLoad_e3d2cc();
  return (0.0f).xxxx;
}

tint_symbol vertex_main() {
  const float4 inner_result = vertex_main_inner();
  tint_symbol wrapper_result = (tint_symbol)0;
  wrapper_result.value = inner_result;
  return wrapper_result;
}

void fragment_main() {
  textureLoad_e3d2cc();
  return;
}

[numthreads(1, 1, 1)]
void compute_main() {
  textureLoad_e3d2cc();
  return;
}
