SKIP: FAILED

#version 310 es
precision mediump float;

uniform highp sampler2DArrayShadow arg_0_arg_1;

void textureSample_60bf45() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  float res = textureOffset(arg_0_arg_1, vec4(vec3(arg_2, float(arg_3)), 0.0f), ivec2(0));
}

void fragment_main() {
  textureSample_60bf45();
}

void main() {
  fragment_main();
  return;
}
Error parsing GLSL shader:
ERROR: 0:9: 'sampler' : TextureOffset does not support sampler2DArrayShadow :  ES Profile
ERROR: 0:9: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



