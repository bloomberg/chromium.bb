#version 310 es
precision mediump float;

uniform highp sampler2DArrayShadow arg_0_arg_1;

void textureSampleCompare_dd431d() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  float arg_4 = 1.0f;
  float res = texture(arg_0_arg_1, vec4(vec3(arg_2, float(arg_3)), arg_4));
}

void fragment_main() {
  textureSampleCompare_dd431d();
}

void main() {
  fragment_main();
  return;
}
