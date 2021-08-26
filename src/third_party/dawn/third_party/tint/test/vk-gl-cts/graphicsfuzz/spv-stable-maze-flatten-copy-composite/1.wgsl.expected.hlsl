static float4 gl_FragCoord = float4(0.0f, 0.0f, 0.0f, 0.0f);
cbuffer cbuffer_x_7 : register(b0, space0) {
  uint4 x_7[1];
};
static int map[256] = (int[256])0;
static float4 x_GLF_color = float4(0.0f, 0.0f, 0.0f, 0.0f);

void main_1() {
  float2 pos = float2(0.0f, 0.0f);
  int2 ipos = int2(0, 0);
  int i = 0;
  int2 p = int2(0, 0);
  bool canwalk = false;
  int v = 0;
  int directions = 0;
  int j = 0;
  int d = 0;
  const float4 x_59 = gl_FragCoord;
  const float2 x_62 = asfloat(x_7[0].xy);
  pos = (float2(x_59.x, x_59.y) / x_62);
  const float x_65 = pos.x;
  const float x_69 = pos.y;
  ipos = int2(int((x_65 * 16.0f)), int((x_69 * 16.0f)));
  i = 0;
  {
    for(; (i < 256); i = (i + 1)) {
      map[i] = 0;
    }
  }
  p = int2(0, 0);
  canwalk = true;
  v = 0;
  while (true) {
    bool x_104 = false;
    bool x_124 = false;
    bool x_144 = false;
    bool x_164 = false;
    bool x_105_phi = false;
    bool x_125_phi = false;
    bool x_145_phi = false;
    bool x_165_phi = false;
    v = (v + 1);
    directions = 0;
    const int x_91 = p.x;
    const bool x_92 = (x_91 > 0);
    x_105_phi = x_92;
    if (x_92) {
      const int x_96 = p.x;
      const int x_99 = p.y;
      const int x_103 = map[((x_96 - 2) + (x_99 * 16))];
      x_104 = (x_103 == 0);
      x_105_phi = x_104;
    }
    if (x_105_phi) {
      directions = (directions + 1);
    }
    const int x_111 = p.y;
    const bool x_112 = (x_111 > 0);
    x_125_phi = x_112;
    if (x_112) {
      const int x_116 = p.x;
      const int x_118 = p.y;
      const int x_123 = map[(x_116 + ((x_118 - 2) * 16))];
      x_124 = (x_123 == 0);
      x_125_phi = x_124;
    }
    if (x_125_phi) {
      directions = (directions + 1);
    }
    const int x_131 = p.x;
    const bool x_132 = (x_131 < 14);
    x_145_phi = x_132;
    if (x_132) {
      const int x_136 = p.x;
      const int x_139 = p.y;
      const int x_143 = map[((x_136 + 2) + (x_139 * 16))];
      x_144 = (x_143 == 0);
      x_145_phi = x_144;
    }
    if (x_145_phi) {
      directions = (directions + 1);
    }
    const int x_151 = p.y;
    const bool x_152 = (x_151 < 14);
    x_165_phi = x_152;
    if (x_152) {
      const int x_156 = p.x;
      const int x_158 = p.y;
      const int x_163 = map[(x_156 + ((x_158 + 2) * 16))];
      x_164 = (x_163 == 0);
      x_165_phi = x_164;
    }
    if (x_165_phi) {
      directions = (directions + 1);
    }
    bool x_229 = false;
    bool x_242 = false;
    bool x_281 = false;
    int x_288 = 0;
    int x_289 = 0;
    int x_295 = 0;
    int x_296 = 0;
    int x_303[256] = (int[256])0;
    int x_304[256] = (int[256])0;
    int x_315 = 0;
    int x_316 = 0;
    bool x_359 = false;
    bool x_372 = false;
    bool x_411 = false;
    bool x_424 = false;
    bool x_230_phi = false;
    bool x_243_phi = false;
    bool x_282_phi = false;
    int x_290_phi = 0;
    int x_297_phi = 0;
    int x_305_phi[256] = (int[256])0;
    int x_317_phi = 0;
    bool x_360_phi = false;
    bool x_373_phi = false;
    bool x_412_phi = false;
    bool x_425_phi = false;
    if ((directions == 0)) {
      canwalk = false;
      i = 0;
      {
        for(; (i < 8); i = (i + 1)) {
          j = 0;
          {
            for(; (j < 8); j = (j + 1)) {
              const int x_196 = map[((j * 2) + ((i * 2) * 16))];
              if ((x_196 == 0)) {
                p.x = (j * 2);
                p.y = (i * 2);
                canwalk = true;
              }
            }
          }
        }
      }
      const int x_211 = p.x;
      const int x_213 = p.y;
      map[(x_211 + (x_213 * 16))] = 1;
    } else {
      d = (v % directions);
      v = (v + directions);
      const bool x_224 = (d >= 0);
      x_230_phi = x_224;
      if (x_224) {
        const int x_228 = p.x;
        x_229 = (x_228 > 0);
        x_230_phi = x_229;
      }
      const bool x_230 = x_230_phi;
      x_243_phi = x_230;
      if (x_230) {
        const int x_234 = p.x;
        const int x_237 = p.y;
        const int x_241 = map[((x_234 - 2) + (x_237 * 16))];
        x_242 = (x_241 == 0);
        x_243_phi = x_242;
      }
      if (x_243_phi) {
        d = (d - 1);
        const int x_249 = p.x;
        const int x_251 = p.y;
        map[(x_249 + (x_251 * 16))] = 1;
        const int x_256 = p.x;
        const int x_259 = p.y;
        map[((x_256 - 1) + (x_259 * 16))] = 1;
        const int x_264 = p.x;
        const int x_267 = p.y;
        map[((x_264 - 2) + (x_267 * 16))] = 1;
        const int x_272 = p.x;
        p.x = (x_272 - 2);
      }
      const bool x_276 = (d >= 0);
      x_282_phi = x_276;
      if (x_276) {
        const int x_280 = p.y;
        x_281 = (x_280 > 0);
        x_282_phi = x_281;
      }
      const bool x_282 = x_282_phi;
      if (x_282) {
        x_288 = p.x;
        x_290_phi = x_288;
      } else {
        x_289 = 0;
        x_290_phi = x_289;
      }
      const int x_290 = x_290_phi;
      if (x_282) {
        x_295 = p.y;
        x_297_phi = x_295;
      } else {
        x_296 = 0;
        x_297_phi = x_296;
      }
      const int x_299 = ((x_297_phi - 2) * 16);
      if (x_282) {
        x_303 = map;
        x_305_phi = x_303;
      } else {
        const int tint_symbol_4[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        x_304 = tint_symbol_4;
        x_305_phi = x_304;
      }
      const int x_305[256] = x_305_phi;
      if (x_282) {
        const int tint_symbol_5[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        map = tint_symbol_5;
      }
      if (x_282) {
        map = x_305;
      }
      if (x_282) {
        x_315 = map[(x_290 + x_299)];
        x_317_phi = x_315;
      } else {
        x_316 = 0;
        x_317_phi = x_316;
      }
      const bool x_318 = (x_317_phi == 0);
      if ((x_282 ? x_318 : x_282)) {
        d = (d - 1);
        const int x_326 = p.x;
        const int x_328 = p.y;
        map[(x_326 + (x_328 * 16))] = 1;
        const int x_333 = p.x;
        const int x_335 = p.y;
        map[(x_333 + ((x_335 - 1) * 16))] = 1;
        const int x_341 = p.x;
        const int x_343 = p.y;
        const int x_345[256] = map;
        const int tint_symbol_6[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        map = tint_symbol_6;
        map = x_345;
        map[(x_341 + ((x_343 - 2) * 16))] = 1;
        const int x_350 = p.y;
        p.y = (x_350 - 2);
      }
      const bool x_354 = (d >= 0);
      x_360_phi = x_354;
      if (x_354) {
        const int x_358 = p.x;
        x_359 = (x_358 < 14);
        x_360_phi = x_359;
      }
      const bool x_360 = x_360_phi;
      x_373_phi = x_360;
      if (x_360) {
        const int x_364 = p.x;
        const int x_367 = p.y;
        const int x_371 = map[((x_364 + 2) + (x_367 * 16))];
        x_372 = (x_371 == 0);
        x_373_phi = x_372;
      }
      if (x_373_phi) {
        d = (d - 1);
        const int x_379 = p.x;
        const int x_381 = p.y;
        map[(x_379 + (x_381 * 16))] = 1;
        const int x_386 = p.x;
        const int x_389 = p.y;
        map[((x_386 + 1) + (x_389 * 16))] = 1;
        const int x_394 = p.x;
        const int x_397 = p.y;
        map[((x_394 + 2) + (x_397 * 16))] = 1;
        const int x_402 = p.x;
        p.x = (x_402 + 2);
      }
      const bool x_406 = (d >= 0);
      x_412_phi = x_406;
      if (x_406) {
        const int x_410 = p.y;
        x_411 = (x_410 < 14);
        x_412_phi = x_411;
      }
      const bool x_412 = x_412_phi;
      x_425_phi = x_412;
      if (x_412) {
        const int x_416 = p.x;
        const int x_418 = p.y;
        const int x_423 = map[(x_416 + ((x_418 + 2) * 16))];
        x_424 = (x_423 == 0);
        x_425_phi = x_424;
      }
      if (x_425_phi) {
        d = (d - 1);
        const int x_431 = p.x;
        const int x_433 = p.y;
        map[(x_431 + (x_433 * 16))] = 1;
        const int x_438 = p.x;
        const int x_440 = p.y;
        map[(x_438 + ((x_440 + 1) * 16))] = 1;
        const int x_446 = p.x;
        const int x_448 = p.y;
        map[(x_446 + ((x_448 + 2) * 16))] = 1;
        const int x_454 = p.y;
        p.y = (x_454 + 2);
      }
    }
    const int x_458 = ipos.y;
    const int x_461 = ipos.x;
    const int x_464 = map[((x_458 * 16) + x_461)];
    if ((x_464 == 1)) {
      x_GLF_color = float4(1.0f, 1.0f, 1.0f, 1.0f);
      return;
    }
    {
      if (canwalk) {
      } else {
        break;
      }
    }
  }
  x_GLF_color = float4(0.0f, 0.0f, 0.0f, 1.0f);
  return;
}

struct main_out {
  float4 x_GLF_color_1;
};
struct tint_symbol_1 {
  float4 gl_FragCoord_param : SV_Position;
};
struct tint_symbol_2 {
  float4 x_GLF_color_1 : SV_Target0;
};

main_out main_inner(float4 gl_FragCoord_param) {
  gl_FragCoord = gl_FragCoord_param;
  main_1();
  const main_out tint_symbol_7 = {x_GLF_color};
  return tint_symbol_7;
}

tint_symbol_2 main(tint_symbol_1 tint_symbol) {
  const main_out inner_result = main_inner(tint_symbol.gl_FragCoord_param);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.x_GLF_color_1 = inner_result.x_GLF_color_1;
  return wrapper_result;
}
