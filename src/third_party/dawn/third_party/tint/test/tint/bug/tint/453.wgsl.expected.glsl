#version 310 es

layout(r32ui) uniform highp writeonly uimage2D Dst;
uniform highp usampler2D Src_1;
void tint_symbol() {
  uvec4 srcValue = uvec4(0u, 0u, 0u, 0u);
  uvec4 x_22 = texelFetch(Src_1, ivec2(0, 0), 0);
  srcValue = x_22;
  uint x_24 = srcValue.x;
  uint x_25 = (x_24 + 1u);
  imageStore(Dst, ivec2(0, 0), srcValue.xxxx);
  return;
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol();
  return;
}
