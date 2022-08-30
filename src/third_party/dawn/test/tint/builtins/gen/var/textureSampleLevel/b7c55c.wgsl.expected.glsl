#version 310 es

uniform highp sampler2DArray arg_0_arg_1;

void textureSampleLevel_b7c55c() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  float arg_4 = 1.0f;
  vec4 res = textureLodOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, ivec2(0));
}

vec4 vertex_main() {
  textureSampleLevel_b7c55c();
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

void textureSampleLevel_b7c55c() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  float arg_4 = 1.0f;
  vec4 res = textureLodOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, ivec2(0));
}

void fragment_main() {
  textureSampleLevel_b7c55c();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uniform highp sampler2DArray arg_0_arg_1;

void textureSampleLevel_b7c55c() {
  vec2 arg_2 = vec2(0.0f);
  int arg_3 = 1;
  float arg_4 = 1.0f;
  vec4 res = textureLodOffset(arg_0_arg_1, vec3(arg_2, float(arg_3)), arg_4, ivec2(0));
}

void compute_main() {
  textureSampleLevel_b7c55c();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
