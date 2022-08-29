#version 310 es

vec4 tint_symbol_1 = vec4(0.0f);
uniform highp sampler2DMS arg_0_1;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  ivec2 x_16 = ivec2(textureSize(arg_0_1));
  res = x_16;
  return;
}

void tint_symbol_2(vec4 tint_symbol) {
  tint_symbol_1 = tint_symbol;
  return;
}

void vertex_main_1() {
  textureDimensions_f60bdb();
  tint_symbol_2(vec4(0.0f));
  return;
}

struct vertex_main_out {
  vec4 tint_symbol_1_1;
};

vertex_main_out vertex_main() {
  vertex_main_1();
  vertex_main_out tint_symbol_3 = vertex_main_out(tint_symbol_1);
  return tint_symbol_3;
}

void main() {
  gl_PointSize = 1.0;
  vertex_main_out inner_result = vertex_main();
  gl_Position = inner_result.tint_symbol_1_1;
  gl_Position.y = -(gl_Position.y);
  gl_Position.z = ((2.0f * gl_Position.z) - gl_Position.w);
  return;
}
#version 310 es
precision mediump float;

uniform highp sampler2DMS arg_0_1;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  ivec2 x_16 = ivec2(textureSize(arg_0_1));
  res = x_16;
  return;
}

struct vertex_main_out {
  vec4 tint_symbol_1_1;
};

void fragment_main_1() {
  textureDimensions_f60bdb();
  return;
}

void fragment_main() {
  fragment_main_1();
}

void main() {
  fragment_main();
  return;
}
#version 310 es

uniform highp sampler2DMS arg_0_1;
void textureDimensions_f60bdb() {
  ivec2 res = ivec2(0);
  ivec2 x_16 = ivec2(textureSize(arg_0_1));
  res = x_16;
  return;
}

struct vertex_main_out {
  vec4 tint_symbol_1_1;
};

void compute_main_1() {
  textureDimensions_f60bdb();
  return;
}

void compute_main() {
  compute_main_1();
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
  compute_main();
  return;
}
