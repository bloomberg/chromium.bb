#version 310 es
precision mediump float;

struct Uniforms {
  uint i;
};
struct OuterS {
  vec3 v1;
};

layout (binding = 4) uniform Uniforms_1 {
  uint i;
} uniforms;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void tint_symbol() {
  OuterS s1 = OuterS(vec3(0.0f, 0.0f, 0.0f));
  s1.v1[uniforms.i] = 1.0f;
  return;
}
void main() {
  tint_symbol();
}


