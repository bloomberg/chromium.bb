SKIP: FAILED

#version 310 es
precision mediump float;

struct buf0 {
  vec2 injectionSwitch;
};

int data[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
int temp[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
vec4 tint_symbol = vec4(0.0f, 0.0f, 0.0f, 0.0f);
layout (binding = 0) uniform buf0_1 {
  vec2 injectionSwitch;
} x_34;
vec4 x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

void merge_i1_i1_i1_(inout int from, inout int mid, inout int to) {
  int k = 0;
  int i = 0;
  int j = 0;
  int i_1 = 0;
  int x_260 = from;
  k = x_260;
  int x_261 = from;
  i = x_261;
  int x_262 = mid;
  j = (x_262 + 1);
  while (true) {
    int x_268 = i;
    int x_269 = mid;
    int x_271 = j;
    int x_272 = to;
    if (((x_268 <= x_269) & (x_271 <= x_272))) {
    } else {
      break;
    }
    int x_278 = data[i];
    int x_281 = data[j];
    if ((x_278 < x_281)) {
      int x_286 = k;
      k = (x_286 + 1);
      int x_288 = i;
      i = (x_288 + 1);
      int x_291 = data[x_288];
      temp[x_286] = x_291;
    } else {
      int x_293 = k;
      k = (x_293 + 1);
      int x_295 = j;
      j = (x_295 + 1);
      int x_298 = data[x_295];
      temp[x_293] = x_298;
    }
  }
  while (true) {
    int x_304 = i;
    int x_306 = i;
    int x_307 = mid;
    if (((x_304 < 10) & (x_306 <= x_307))) {
    } else {
      break;
    }
    int x_311 = k;
    k = (x_311 + 1);
    int x_313 = i;
    i = (x_313 + 1);
    int x_316 = data[x_313];
    temp[x_311] = x_316;
  }
  int x_318 = from;
  i_1 = x_318;
  while (true) {
    int x_323 = i_1;
    int x_324 = to;
    if ((x_323 <= x_324)) {
    } else {
      break;
    }
    int x_327 = i_1;
    int x_330 = temp[i_1];
    data[x_327] = x_330;
    {
      i_1 = (i_1 + 1);
    }
  }
  return;
}

int func_i1_i1_(inout int m, inout int high) {
  int x = 0;
  int x_335 = 0;
  int x_336 = 0;
  float x_338 = tint_symbol.x;
  if ((x_338 >= 0.0f)) {
    if (false) {
      int x_346 = high;
      x_336 = (x_346 << uint(0));
    } else {
      x_336 = 4;
    }
    x_335 = (1 << uint(x_336));
  } else {
    x_335 = 1;
  }
  x = x_335;
  x = (x >> uint(4));
  int x_353 = m;
  int x_355 = m;
  int x_357 = m;
  return clamp((2 * x_353), (2 * x_355), ((2 * x_357) / x));
}

void mergeSort_() {
  int low = 0;
  int high_1 = 0;
  int m_1 = 0;
  int i_2 = 0;
  int from_1 = 0;
  int mid_1 = 0;
  int to_1 = 0;
  int param = 0;
  int param_1 = 0;
  int param_2 = 0;
  int param_3 = 0;
  int param_4 = 0;
  low = 0;
  high_1 = 9;
  m_1 = 1;
  {
    for(; (m_1 <= high_1); m_1 = (2 * m_1)) {
      i_2 = low;
      while (true) {
        if ((i_2 < high_1)) {
        } else {
          break;
        }
        from_1 = i_2;
        mid_1 = ((i_2 + m_1) - 1);
        to_1 = min(((i_2 + (2 * m_1)) - 1), high_1);
        param = from_1;
        param_1 = mid_1;
        param_2 = to_1;
        merge_i1_i1_i1_(param, param_1, param_2);
        {
          param_3 = m_1;
          param_4 = high_1;
          int x_398 = func_i1_i1_(param_3, param_4);
          i_2 = (i_2 + x_398);
        }
      }
    }
  }
  return;
}

void main_1() {
  int i_3 = 0;
  int j_1 = 0;
  float grey = 0.0f;
  float x_93 = x_34.injectionSwitch.x;
  i_3 = int(x_93);
  while (true) {
    switch(i_3) {
      case 9: {
        data[i_3] = -5;
        break;
      }
      case 8: {
        data[i_3] = -4;
        break;
      }
      case 7: {
        data[i_3] = -3;
        break;
      }
      case 6: {
        data[i_3] = -2;
        break;
      }
      case 5: {
        data[i_3] = -1;
        break;
      }
      case 4: {
        data[i_3] = 0;
        break;
      }
      case 3: {
        data[i_3] = 1;
        break;
      }
      case 2: {
        data[i_3] = 2;
        break;
      }
      case 1: {
        data[i_3] = 3;
        break;
      }
      case 0: {
        data[i_3] = 4;
        break;
      }
      default: {
        break;
      }
    }
    i_3 = (i_3 + 1);
    {
      if ((i_3 < 10)) {
      } else {
        break;
      }
    }
  }
  j_1 = 0;
  {
    for(; (j_1 < 10); j_1 = (j_1 + 1)) {
      int x_142 = j_1;
      int x_145 = data[j_1];
      temp[x_142] = x_145;
    }
  }
  mergeSort_();
  float x_151 = tint_symbol.y;
  if ((int(x_151) < 30)) {
    int x_158 = data[0];
    grey = (0.5f + (float(x_158) / 10.0f));
  } else {
    float x_163 = tint_symbol.y;
    if ((int(x_163) < 60)) {
      int x_170 = data[1];
      grey = (0.5f + (float(x_170) / 10.0f));
    } else {
      float x_175 = tint_symbol.y;
      if ((int(x_175) < 90)) {
        int x_182 = data[2];
        grey = (0.5f + (float(x_182) / 10.0f));
      } else {
        float x_187 = tint_symbol.y;
        if ((int(x_187) < 120)) {
          int x_194 = data[3];
          grey = (0.5f + (float(x_194) / 10.0f));
        } else {
          float x_199 = tint_symbol.y;
          if ((int(x_199) < 150)) {
            discard;
          } else {
            float x_206 = tint_symbol.y;
            if ((int(x_206) < 180)) {
              int x_213 = data[5];
              grey = (0.5f + (float(x_213) / 10.0f));
            } else {
              float x_218 = tint_symbol.y;
              if ((int(x_218) < 210)) {
                int x_225 = data[6];
                grey = (0.5f + (float(x_225) / 10.0f));
              } else {
                float x_230 = tint_symbol.y;
                if ((int(x_230) < 240)) {
                  int x_237 = data[7];
                  grey = (0.5f + (float(x_237) / 10.0f));
                } else {
                  float x_242 = tint_symbol.y;
                  if ((int(x_242) < 270)) {
                    int x_249 = data[8];
                    grey = (0.5f + (float(x_249) / 10.0f));
                  } else {
                    discard;
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  float x_253 = grey;
  vec3 x_254 = vec3(x_253, x_253, x_253);
  x_GLF_color = vec4(x_254.x, x_254.y, x_254.z, 1.0f);
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
ERROR: 0:32: '&' :  wrong operand types: no operation '&' exists that takes a left-hand operand of type ' temp bool' and a right operand of type ' temp bool' (or there is no acceptable conversion)
ERROR: 0:32: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



