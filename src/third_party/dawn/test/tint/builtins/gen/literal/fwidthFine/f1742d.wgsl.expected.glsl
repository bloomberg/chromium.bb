#version 310 es
precision mediump float;

void fwidthFine_f1742d() {
  float res = fwidth(1.0f);
}

void fragment_main() {
  fwidthFine_f1742d();
}

void main() {
  fragment_main();
  return;
}
