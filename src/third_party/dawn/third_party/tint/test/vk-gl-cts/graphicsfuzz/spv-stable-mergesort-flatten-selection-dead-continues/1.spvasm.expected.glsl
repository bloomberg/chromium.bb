SKIP: FAILED

#version 310 es
precision mediump float;

struct buf0 {
  vec2 injectionSwitch;
};

int data[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
int temp[10] = int[10](0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
layout (binding = 0) uniform buf0_1 {
  vec2 injectionSwitch;
} x_28;
vec4 tint_symbol = vec4(0.0f, 0.0f, 0.0f, 0.0f);
vec4 x_GLF_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

void merge_i1_i1_i1_(inout int from, inout int mid, inout int to) {
  int k = 0;
  int i = 0;
  int j = 0;
  int i_1 = 0;
  int x_255 = from;
  k = x_255;
  int x_256 = from;
  i = x_256;
  int x_257 = mid;
  j = (x_257 + 1);
  while (true) {
    int x_285 = 0;
    int x_286 = 0;
    int x_305 = 0;
    int x_306 = 0;
    int x_320 = 0;
    int x_324 = 0;
    int x_339 = 0;
    int x_338 = 0;
    int x_352 = 0;
    int x_351 = 0;
    int x_366 = 0;
    int x_365 = 0;
    int x_287_phi = 0;
    int x_307_phi = 0;
    int x_328_phi = 0;
    int x_340_phi = 0;
    int x_353_phi = 0;
    int x_367_phi = 0;
    float x_261 = x_28.injectionSwitch.x;
    if ((1.0f >= x_261)) {
    } else {
      continue;
    }
    int x_266 = i;
    int x_267 = mid;
    int x_269 = j;
    int x_270 = to;
    if (((x_266 <= x_267) & (x_269 <= x_270))) {
    } else {
      break;
    }
    int x_276 = data[i];
    int x_279 = data[j];
    bool x_280 = (x_276 < x_279);
    if (x_280) {
      x_285 = k;
      x_287_phi = x_285;
    } else {
      x_286 = 0;
      x_287_phi = x_286;
    }
    int x_287 = x_287_phi;
    int x_288 = (x_287 + 1);
    if (x_280) {
      k = x_288;
      float x_293 = x_28.injectionSwitch.x;
      if (!((1.0f <= x_293))) {
      } else {
        continue;
      }
    }
    float x_297 = x_28.injectionSwitch.y;
    if ((x_297 >= 0.0f)) {
    } else {
      continue;
    }
    int x_300 = 0;
    if (x_280) {
      x_305 = i;
      x_307_phi = x_305;
    } else {
      x_306 = 0;
      x_307_phi = x_306;
    }
    int x_309 = (x_280 ? x_307_phi : x_300);
    if (x_280) {
      i = (x_309 + 1);
    }
    int x_315 = 0;
    if (x_280) {
      x_320 = data[x_309];
      float x_322 = x_28.injectionSwitch.y;
      x_328_phi = x_320;
      if (!((0.0f <= x_322))) {
        continue;
      }
    } else {
      x_324 = 0;
      float x_326 = x_28.injectionSwitch.y;
      x_328_phi = x_324;
      if (!((x_326 < 0.0f))) {
      } else {
        continue;
      }
    }
    int x_328 = x_328_phi;
    if (x_280) {
      temp[x_287] = (x_280 ? x_328 : x_315);
    }
    if (x_280) {
      x_339 = 0;
      x_340_phi = x_339;
    } else {
      x_338 = k;
      x_340_phi = x_338;
    }
    int x_340 = x_340_phi;
    if (x_280) {
    } else {
      k = (x_340 + 1);
    }
    float x_345 = x_28.injectionSwitch.x;
    if (!((1.0f <= x_345))) {
    } else {
      continue;
    }
    if (x_280) {
      x_352 = 0;
      x_353_phi = x_352;
    } else {
      x_351 = j;
      x_353_phi = x_351;
    }
    int x_357 = (x_280 ? 0 : x_353_phi);
    if (x_280) {
    } else {
      j = (x_357 + 1);
    }
    if (x_280) {
      x_366 = 0;
      x_367_phi = x_366;
    } else {
      x_365 = data[x_357];
      x_367_phi = x_365;
    }
    int x_367 = x_367_phi;
    if (x_280) {
    } else {
      temp[x_340] = x_367;
    }
  }
  while (true) {
    int x_376 = i;
    int x_378 = i;
    int x_379 = mid;
    if (((x_376 < 10) & (x_378 <= x_379))) {
    } else {
      break;
    }
    int x_383 = k;
    k = (x_383 + 1);
    int x_385 = i;
    i = (x_385 + 1);
    int x_388 = data[x_385];
    temp[x_383] = x_388;
  }
  int x_390 = from;
  i_1 = x_390;
  while (true) {
    int x_395 = i_1;
    int x_396 = to;
    if ((x_395 <= x_396)) {
    } else {
      break;
    }
    int x_399 = i_1;
    int x_402 = temp[i_1];
    data[x_399] = x_402;
    {
      i_1 = (i_1 + 1);
    }
  }
  return;
}

void mergeSort_() {
  int low = 0;
  int high = 0;
  int m = 0;
  int i_2 = 0;
  int from_1 = 0;
  int mid_1 = 0;
  int to_1 = 0;
  int param = 0;
  int param_1 = 0;
  int param_2 = 0;
  low = 0;
  high = 9;
  m = 1;
  {
    for(; (m <= high); m = (2 * m)) {
      i_2 = low;
      {
        for(; (i_2 < high); i_2 = (i_2 + (2 * m))) {
          from_1 = i_2;
          mid_1 = ((i_2 + m) - 1);
          to_1 = min(((i_2 + (2 * m)) - 1), high);
          param = from_1;
          param_1 = mid_1;
          param_2 = to_1;
          merge_i1_i1_i1_(param, param_1, param_2);
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
  float x_88 = x_28.injectionSwitch.x;
  i_3 = int(x_88);
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
      int x_137 = j_1;
      int x_140 = data[j_1];
      temp[x_137] = x_140;
    }
  }
  mergeSort_();
  float x_146 = tint_symbol.y;
  if ((int(x_146) < 30)) {
    int x_153 = data[0];
    grey = (0.5f + (float(x_153) / 10.0f));
  } else {
    float x_158 = tint_symbol.y;
    if ((int(x_158) < 60)) {
      int x_165 = data[1];
      grey = (0.5f + (float(x_165) / 10.0f));
    } else {
      float x_170 = tint_symbol.y;
      if ((int(x_170) < 90)) {
        int x_177 = data[2];
        grey = (0.5f + (float(x_177) / 10.0f));
      } else {
        float x_182 = tint_symbol.y;
        if ((int(x_182) < 120)) {
          int x_189 = data[3];
          grey = (0.5f + (float(x_189) / 10.0f));
        } else {
          float x_194 = tint_symbol.y;
          if ((int(x_194) < 150)) {
            discard;
          } else {
            float x_201 = tint_symbol.y;
            if ((int(x_201) < 180)) {
              int x_208 = data[5];
              grey = (0.5f + (float(x_208) / 10.0f));
            } else {
              float x_213 = tint_symbol.y;
              if ((int(x_213) < 210)) {
                int x_220 = data[6];
                grey = (0.5f + (float(x_220) / 10.0f));
              } else {
                float x_225 = tint_symbol.y;
                if ((int(x_225) < 240)) {
                  int x_232 = data[7];
                  grey = (0.5f + (float(x_232) / 10.0f));
                } else {
                  float x_237 = tint_symbol.y;
                  if ((int(x_237) < 270)) {
                    int x_244 = data[8];
                    grey = (0.5f + (float(x_244) / 10.0f));
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
  float x_248 = grey;
  vec3 x_249 = vec3(x_248, x_248, x_248);
  x_GLF_color = vec4(x_249.x, x_249.y, x_249.z, 1.0f);
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
ERROR: 0:55: '&' :  wrong operand types: no operation '&' exists that takes a left-hand operand of type ' temp bool' and a right operand of type ' temp bool' (or there is no acceptable conversion)
ERROR: 0:55: '' : compilation terminated 
ERROR: 2 compilation errors.  No code generated.



