#version 310 es
precision mediump float;

uniform highp usampler2D t_s;

void tint_symbol() {
  uvec4 res = textureGather(t_s, vec2(0.0f), 0);
}

void main() {
  tint_symbol();
  return;
}
