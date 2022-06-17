#version 310 es

uniform highp sampler2DArray arg_0_arg_1;

void textureSampleGrad_d65515() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  vec2 arg_4 = vec2(0.0f);
  vec2 arg_5 = vec2(0.0f);
  vec4 res = textureGradOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, arg_5, ivec2(0));
}

vec4 vertex_main() {
  textureSampleGrad_d65515();
  return vec4(0.0f);
}

void main() {
  gl_PointSize = 1.0;
  vec4 inner_result = vertex_main();
  gl_Position = inner_result;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

uniform highp sampler2DArray arg_0_arg_1;

void textureSampleGrad_d65515() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  vec2 arg_4 = vec2(0.0f);
  vec2 arg_5 = vec2(0.0f);
  vec4 res = textureGradOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, arg_5, ivec2(0));
}

void fragment_main() {
  textureSampleGrad_d65515();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uniform highp sampler2DArray arg_0_arg_1;

void textureSampleGrad_d65515() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  vec2 arg_4 = vec2(0.0f);
  vec2 arg_5 = vec2(0.0f);
  vec4 res = textureGradOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, arg_5, ivec2(0));
}

void compute_main() {
  textureSampleGrad_d65515();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
