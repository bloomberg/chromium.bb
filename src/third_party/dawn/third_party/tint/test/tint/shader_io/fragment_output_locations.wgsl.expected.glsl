#version 310 es
precision mediump float;

layout(location = 0) out int value;
int main0() {
  return 1;
}

void main() {
  int inner_result = main0();
  value = inner_result;
  return;
}
#version 310 es
precision mediump float;

layout(location = 1) out uint value;
uint main1() {
  return 1u;
}

void main() {
  uint inner_result = main1();
  value = inner_result;
  return;
}
#version 310 es
precision mediump float;

layout(location = 2) out float value;
float main2() {
  return 1.0f;
}

void main() {
  float inner_result = main2();
  value = inner_result;
  return;
}
#version 310 es
precision mediump float;

layout(location = 3) out vec4 value;
vec4 main3() {
  return vec4(1.0f, 2.0f, 3.0f, 4.0f);
}

void main() {
  vec4 inner_result = main3();
  value = inner_result;
  return;
}
