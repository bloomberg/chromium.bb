#version 310 es
precision mediump float;

void dpdyFine_d0a648() {
  vec4 res = dFdy(vec4(0.0f, 0.0f, 0.0f, 0.0f));
}

void fragment_main() {
  dpdyFine_d0a648();
}

void main() {
  fragment_main();
  return;
}
