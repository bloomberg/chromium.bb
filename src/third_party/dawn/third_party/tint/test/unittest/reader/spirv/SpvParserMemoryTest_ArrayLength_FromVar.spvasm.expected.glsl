SKIP: FAILED

#version 310 es
precision mediump float;

struct S {
  uint first;
  uint rtarr[];
};

layout(binding = 0, std430) buffer S_1 {
  uint first;
  uint rtarr[];
} myvar;
void main_1() {
  uint x_1 = uint(myvar.rtarr.length());
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
ERROR: 0:6: '' : array size required 
ERROR: 0:7: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



