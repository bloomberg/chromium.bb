SKIP: FAILED

#version 310 es
precision mediump float;

uniform highp sampler2DArrayShadow x_20_x_10;

void main_1() {
  float f1 = 1.0f;
  vec2 vf12 = vec2(1.0f, 2.0f);
  vec2 vf21 = vec2(2.0f, 1.0f);
  vec3 vf123 = vec3(1.0f, 2.0f, 3.0f);
  vec4 vf1234 = vec4(1.0f, 2.0f, 3.0f, 4.0f);
  int i1 = 1;
  ivec2 vi12 = ivec2(1, 2);
  ivec3 vi123 = ivec3(1, 2, 3);
  ivec4 vi1234 = ivec4(1, 2, 3, 4);
  uint u1 = 1u;
  uvec2 vu12 = uvec2(1u, 2u);
  uvec3 vu123 = uvec3(1u, 2u, 3u);
  uvec4 vu1234 = uvec4(1u, 2u, 3u, 4u);
  float coords1 = 1.0f;
  vec2 coords12 = vf12;
  vec3 coords123 = vf123;
  vec4 coords1234 = vf1234;
  float x_79 = textureOffset(x_20_x_10, vec4(vec3(coords123.xy, float(int(round(coords123.z)))), 0.200000003f), ivec2(3, 4));
  return;
}

void tint_symbol() {
  main_1();
}

void main() {
  tint_symbol();
  return;
}
Error parsing GLSL shader:
ERROR: 0:24: 'sampler' : TextureOffset does not support sampler2DArrayShadow :  ES Profile
ERROR: 0:24: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



