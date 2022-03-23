#version 310 es
precision mediump float;

layout(location = 0) out vec4 x_GLF_color_1_1;
struct strided_arr {
  int el;
};

struct buf0 {
  strided_arr x_GLF_uniform_int_values[2];
};

struct strided_arr_1 {
  float el;
};

struct buf1 {
  strided_arr_1 x_GLF_uniform_float_values[2];
};

vec4 x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
layout(binding = 0) uniform buf0_1 {
  strided_arr x_GLF_uniform_int_values[2];
} x_8;

vec4 tint_symbol = vec4(0.0f, 0.0f, 0.0f, 0.0f);
layout(binding = 1) uniform buf1_1 {
  strided_arr_1 x_GLF_uniform_float_values[2];
} x_10;

float f1_f1_(inout float a) {
  float x_100 = a;
  return dFdx(x_100);
}

void main_1() {
  vec4 v2 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  float a_1 = 0.0f;
  float x_40 = 0.0f;
  float param = 0.0f;
  int x_42 = x_8.x_GLF_uniform_int_values[0].el;
  int x_45 = x_8.x_GLF_uniform_int_values[1].el;
  int x_48 = x_8.x_GLF_uniform_int_values[1].el;
  int x_51 = x_8.x_GLF_uniform_int_values[0].el;
  x_GLF_color = vec4(float(x_42), float(x_45), float(x_48), float(x_51));
  float x_55 = tint_symbol.x;
  float x_57 = x_10.x_GLF_uniform_float_values[1].el;
  if ((x_55 < x_57)) {
    float x_62 = v2.x;
    if (!((x_62 < 1.0f))) {
      float x_68 = x_10.x_GLF_uniform_float_values[1].el;
      float x_70 = x_10.x_GLF_uniform_float_values[1].el;
      float x_72 = x_10.x_GLF_uniform_float_values[0].el;
      if ((x_70 > x_72)) {
        float x_78 = x_10.x_GLF_uniform_float_values[0].el;
        param = x_78;
        float x_79 = f1_f1_(param);
        x_40 = x_79;
      } else {
        float x_81 = x_10.x_GLF_uniform_float_values[0].el;
        x_40 = x_81;
      }
      a_1 = (x_68 / x_40);
      float x_85 = x_10.x_GLF_uniform_float_values[0].el;
      float x_88 = x_10.x_GLF_uniform_float_values[0].el;
      float x_90 = a_1;
      vec3 x_92 = mix(vec3(x_85, x_85, x_85), vec3(x_88, x_88, x_88), vec3(x_90, x_90, x_90));
      float x_94 = x_10.x_GLF_uniform_float_values[1].el;
      x_GLF_color = vec4(x_92.x, x_92.y, x_92.z, x_94);
    }
  }
  return;
}

struct main_out {
  vec4 x_GLF_color_1;
};

main_out tint_symbol_1(vec4 tint_symbol_2) {
  tint_symbol = tint_symbol_2;
  main_1();
  main_out tint_symbol_3 = main_out(x_GLF_color);
  return tint_symbol_3;
}

void main() {
  main_out inner_result = tint_symbol_1(gl_FragCoord);
  x_GLF_color_1_1 = inner_result.x_GLF_color_1;
  return;
}
