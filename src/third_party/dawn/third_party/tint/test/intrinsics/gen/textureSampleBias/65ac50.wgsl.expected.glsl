#version 310 es
precision mediump float;

uniform highp sampler2DArray arg_0;


void textureSampleBias_65ac50() {
  vec4 res = textureOffset(arg_0, vec3(0.0f, 0.0f, float(1)), ivec2(0, 0), 1.0f);
}

void fragment_main() {
  textureSampleBias_65ac50();
  return;
}
void main() {
  fragment_main();
}


