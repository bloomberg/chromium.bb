void set_vector_float2x4(inout float2x4 mat, int col, float4 val) {
  switch (col) {
    case 0: mat[0] = val; break;
    case 1: mat[1] = val; break;
  }
}

cbuffer cbuffer_uniforms : register(b4, space1) {
  uint4 uniforms[1];
};
static float2x4 m1 = float2x4(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);

[numthreads(1, 1, 1)]
void main() {
  set_vector_float2x4(m1, uniforms[0].x, (1.0f).xxxx);
  return;
}
