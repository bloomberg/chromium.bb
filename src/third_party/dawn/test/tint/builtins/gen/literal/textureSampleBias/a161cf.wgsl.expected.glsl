#version 310 es
precision mediump float;

uniform highp sampler2D arg_0_arg_1;

void textureSampleBias_a161cf() {
  vec4 res = textureOffset(arg_0_arg_1, vec2(0.0f), ivec2(0), 1.0f);
}

void fragment_main() {
  textureSampleBias_a161cf();
}

void main() {
  fragment_main();
  return;
}
