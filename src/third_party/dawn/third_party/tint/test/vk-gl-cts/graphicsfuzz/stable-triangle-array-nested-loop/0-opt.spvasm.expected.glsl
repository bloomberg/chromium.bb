SKIP: FAILED

#version 310 es
precision mediump float;

struct buf0 {
  vec2 resolution;
};

vec4 tint_symbol = vec4(0.0f, 0.0f, 0.0f, 0.0f);
layout (binding = 0) uniform buf0_1 {
  vec2 resolution;
} x_24;
vec4 x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

float cross2d_vf2_vf2_(inout vec2 a, inout vec2 b) {
  float x_79 = a.x;
  float x_81 = b.y;
  float x_84 = b.x;
  float x_86 = a.y;
  return ((x_79 * x_81) - (x_84 * x_86));
}

int pointInTriangle_vf2_vf2_vf2_vf2_(inout vec2 p, inout vec2 a_1, inout vec2 b_1, inout vec2 c) {
  bool x_90 = false;
  int x_91 = 0;
  float pab = 0.0f;
  vec2 param = vec2(0.0f, 0.0f);
  vec2 param_1 = vec2(0.0f, 0.0f);
  float pbc = 0.0f;
  vec2 param_2 = vec2(0.0f, 0.0f);
  vec2 param_3 = vec2(0.0f, 0.0f);
  float pca = 0.0f;
  vec2 param_4 = vec2(0.0f, 0.0f);
  vec2 param_5 = vec2(0.0f, 0.0f);
  bool x_140 = false;
  bool x_168 = false;
  bool x_141_phi = false;
  bool x_169_phi = false;
  int x_173_phi = 0;
  switch(0u) {
    default: {
      float x_95 = p.x;
      float x_97 = a_1.x;
      float x_100 = p.y;
      float x_102 = a_1.y;
      float x_106 = b_1.x;
      float x_107 = a_1.x;
      float x_110 = b_1.y;
      float x_111 = a_1.y;
      param = vec2((x_95 - x_97), (x_100 - x_102));
      param_1 = vec2((x_106 - x_107), (x_110 - x_111));
      float x_114 = cross2d_vf2_vf2_(param, param_1);
      pab = x_114;
      float x_115 = p.x;
      float x_116 = b_1.x;
      float x_118 = p.y;
      float x_119 = b_1.y;
      float x_123 = c.x;
      float x_124 = b_1.x;
      float x_127 = c.y;
      float x_128 = b_1.y;
      param_2 = vec2((x_115 - x_116), (x_118 - x_119));
      param_3 = vec2((x_123 - x_124), (x_127 - x_128));
      float x_131 = cross2d_vf2_vf2_(param_2, param_3);
      pbc = x_131;
      bool x_134 = ((x_114 < 0.0f) & (x_131 < 0.0f));
      x_141_phi = x_134;
      if (!(x_134)) {
        x_140 = ((x_114 >= 0.0f) & (x_131 >= 0.0f));
        x_141_phi = x_140;
      }
      if (!(x_141_phi)) {
        x_90 = true;
        x_91 = 0;
        x_173_phi = 0;
        break;
      }
      float x_145 = p.x;
      float x_146 = c.x;
      float x_148 = p.y;
      float x_149 = c.y;
      float x_152 = a_1.x;
      float x_153 = c.x;
      float x_155 = a_1.y;
      float x_156 = c.y;
      param_4 = vec2((x_145 - x_146), (x_148 - x_149));
      param_5 = vec2((x_152 - x_153), (x_155 - x_156));
      float x_159 = cross2d_vf2_vf2_(param_4, param_5);
      pca = x_159;
      bool x_162 = ((x_114 < 0.0f) & (x_159 < 0.0f));
      x_169_phi = x_162;
      if (!(x_162)) {
        x_168 = ((x_114 >= 0.0f) & (x_159 >= 0.0f));
        x_169_phi = x_168;
      }
      if (!(x_169_phi)) {
        x_90 = true;
        x_91 = 0;
        x_173_phi = 0;
        break;
      }
      x_90 = true;
      x_91 = 1;
      x_173_phi = 1;
      break;
    }
  }
  return x_173_phi;
}

void main_1() {
  vec2 pos = vec2(0.0f, 0.0f);
  vec2 param_6 = vec2(0.0f, 0.0f);
  vec2 param_7 = vec2(0.0f, 0.0f);
  vec2 param_8 = vec2(0.0f, 0.0f);
  vec2 param_9 = vec2(0.0f, 0.0f);
  vec4 x_67 = tint_symbol;
  vec2 x_70 = x_24.resolution;
  vec2 x_71 = (vec2(x_67.x, x_67.y) / x_70);
  pos = x_71;
  param_6 = x_71;
  param_7 = vec2(0.699999988f, 0.300000012f);
  param_8 = vec2(0.5f, 0.899999976f);
  param_9 = vec2(0.100000001f, 0.400000006f);
  int x_72 = pointInTriangle_vf2_vf2_vf2_vf2_(param_6, param_7, param_8, param_9);
  if ((x_72 == 1)) {
    x_GLF_color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
  } else {
    x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  }
  return;
}

struct main_out {
  vec4 x_GLF_color_1;
};
struct tint_symbol_4 {
  vec4 tint_symbol_2;
};
struct tint_symbol_5 {
  vec4 x_GLF_color_1;
};

main_out tint_symbol_1_inner(vec4 tint_symbol_2) {
  tint_symbol = tint_symbol_2;
  main_1();
  main_out tint_symbol_6 = main_out(x_GLF_color);
  return tint_symbol_6;
}

tint_symbol_5 tint_symbol_1(tint_symbol_4 tint_symbol_3) {
  main_out inner_result = tint_symbol_1_inner(tint_symbol_3.tint_symbol_2);
  tint_symbol_5 wrapper_result = tint_symbol_5(vec4(0.0f, 0.0f, 0.0f, 0.0f));
  wrapper_result.x_GLF_color_1 = inner_result.x_GLF_color_1;
  return wrapper_result;
}
out vec4 x_GLF_color_1;
void main() {
  tint_symbol_4 inputs;
  inputs.tint_symbol_2 = gl_FragCoord;
  tint_symbol_5 outputs;
  outputs = tint_symbol_1(inputs);
  x_GLF_color_1 = outputs.x_GLF_color_1;
}


Error parsing GLSL shader:
ERROR: 0:65: '&' :  wrong operand types: no operation '&' exists that takes a left-hand operand of type ' temp bool' and a right operand of type ' temp bool' (or there is no acceptable conversion)
ERROR: 0:65: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



