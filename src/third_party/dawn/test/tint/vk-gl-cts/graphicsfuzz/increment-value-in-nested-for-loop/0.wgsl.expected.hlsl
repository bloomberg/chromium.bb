SKIP: FAILED

static float4 gl_FragCoord = float4(0.0f, 0.0f, 0.0f, 0.0f);
cbuffer cbuffer_x_6 : register(b0, space0) {
  uint4 x_6[1];
};
static float4 x_GLF_color = float4(0.0f, 0.0f, 0.0f, 0.0f);

void main_1() {
  [loop] while (true) {
    bool x_45 = false;
    int x_48 = 0;
    int x_49 = 0;
    bool x_46 = false;
    int x_115 = 0;
    int x_116 = 0;
    bool x_45_phi = false;
    int x_48_phi = 0;
    int x_50_phi = 0;
    int x_52_phi = 0;
    int x_111_phi = 0;
    bool x_112_phi = false;
    int x_115_phi = 0;
    int x_118_phi = 0;
    int x_120_phi = 0;
    int x_161_phi = 0;
    const float x_40 = asfloat(x_6[0].x);
    const bool x_41 = (x_40 < -1.0f);
    x_45_phi = false;
    x_48_phi = 0;
    x_50_phi = 0;
    x_52_phi = 0;
    [loop] while (true) {
      int x_62 = 0;
      int x_65 = 0;
      int x_66 = 0;
      int x_63 = 0;
      int x_53 = 0;
      int x_62_phi = 0;
      int x_65_phi = 0;
      int x_67_phi = 0;
      int x_51_phi = 0;
      int x_49_phi = 0;
      bool x_46_phi = false;
      x_45 = x_45_phi;
      x_48 = x_48_phi;
      const int x_50 = x_50_phi;
      const int x_52 = x_52_phi;
      const float x_55 = gl_FragCoord.y;
      x_111_phi = x_48;
      x_112_phi = x_45;
      if ((x_52 < ((x_55 > -1.0f) ? 10 : 100))) {
      } else {
        break;
      }
      x_62_phi = x_48;
      x_65_phi = x_50;
      x_67_phi = 0;
      [loop] while (true) {
        int x_97 = 0;
        int x_68 = 0;
        int x_66_phi = 0;
        x_62 = x_62_phi;
        x_65 = x_65_phi;
        const int x_67 = x_67_phi;
        x_51_phi = x_65;
        x_49_phi = x_62;
        x_46_phi = x_45;
        if ((x_67 < 2)) {
        } else {
          break;
        }
        [loop] while (true) {
          bool x_78 = false;
          int x_86_phi = 0;
          int x_97_phi = 0;
          bool x_98_phi = false;
          const float x_77 = gl_FragCoord.x;
          x_78 = (x_77 < -1.0f);
          if (!((x_40 < 0.0f))) {
            if (x_78) {
              x_66_phi = 0;
              break;
            }
            x_86_phi = 1;
            [loop] while (true) {
              int x_87 = 0;
              const int x_86 = x_86_phi;
              x_97_phi = x_65;
              x_98_phi = false;
              if ((x_86 < 3)) {
              } else {
                break;
              }
              if (x_78) {
                {
                  x_87 = (x_86 + 1);
                  x_86_phi = x_87;
                }
                continue;
              }
              if ((x_86 > 0)) {
                x_97_phi = 1;
                x_98_phi = true;
                break;
              }
              {
                x_87 = (x_86 + 1);
                x_86_phi = x_87;
              }
            }
            x_97 = x_97_phi;
            const bool x_98 = x_98_phi;
            x_66_phi = x_97;
            if (x_98) {
              break;
            }
          }
          x_66_phi = 0;
          break;
        }
        x_66 = x_66_phi;
        x_63 = asint((x_62 + x_66));
        if (x_41) {
          [loop] while (true) {
            if (x_41) {
            } else {
              break;
            }
            {
              const float x_105 = float(x_52);
              x_GLF_color = float4(x_105, x_105, x_105, x_105);
            }
          }
          x_51_phi = x_66;
          x_49_phi = x_63;
          x_46_phi = true;
          break;
        }
        {
          x_68 = (x_67 + 1);
          x_62_phi = x_63;
          x_65_phi = x_66;
          x_67_phi = x_68;
        }
      }
      const int x_51 = x_51_phi;
      x_49 = x_49_phi;
      x_46 = x_46_phi;
      x_111_phi = x_49;
      x_112_phi = x_46;
      if (x_46) {
        break;
      }
      if (!(x_41)) {
        x_111_phi = x_49;
        x_112_phi = x_46;
        break;
      }
      {
        x_53 = (x_52 + 1);
        x_45_phi = x_46;
        x_48_phi = x_49;
        x_50_phi = x_51;
        x_52_phi = x_53;
      }
    }
    const int x_111 = x_111_phi;
    if (x_112_phi) {
      break;
    }
    x_115_phi = x_111;
    x_118_phi = 0;
    x_120_phi = 0;
    [loop] while (true) {
      int x_154 = 0;
      int x_121 = 0;
      int x_119_phi = 0;
      x_115 = x_115_phi;
      const int x_118 = x_118_phi;
      const int x_120 = x_120_phi;
      const float x_123 = asfloat(x_6[0].y);
      x_161_phi = x_115;
      if ((x_120 < int((x_123 + 1.0f)))) {
      } else {
        break;
      }
      [loop] while (true) {
        bool x_135 = false;
        int x_143_phi = 0;
        int x_154_phi = 0;
        bool x_155_phi = false;
        const float x_134 = gl_FragCoord.x;
        x_135 = (x_134 < -1.0f);
        if (!((x_40 < 0.0f))) {
          if (x_135) {
            x_119_phi = 0;
            break;
          }
          x_143_phi = 1;
          [loop] while (true) {
            int x_144 = 0;
            const int x_143 = x_143_phi;
            x_154_phi = x_118;
            x_155_phi = false;
            if ((x_143 < 3)) {
            } else {
              break;
            }
            if (x_135) {
              {
                x_144 = (x_143 + 1);
                x_143_phi = x_144;
              }
              continue;
            }
            if ((x_143 > 0)) {
              x_154_phi = 1;
              x_155_phi = true;
              break;
            }
            {
              x_144 = (x_143 + 1);
              x_143_phi = x_144;
            }
          }
          x_154 = x_154_phi;
          const bool x_155 = x_155_phi;
          x_119_phi = x_154;
          if (x_155) {
            break;
          }
        }
        x_119_phi = 0;
        break;
      }
      int x_119 = 0;
      x_119 = x_119_phi;
      x_116 = asint((x_115 + x_119));
      if ((!(x_41) ? false : x_41)) {
        x_161_phi = x_116;
        break;
      }
      {
        x_121 = (x_120 + 1);
        x_115_phi = x_116;
        x_118_phi = x_119;
        x_120_phi = x_121;
      }
    }
    if ((x_161_phi == 4)) {
      x_GLF_color = float4(1.0f, 0.0f, 0.0f, 1.0f);
    } else {
      x_GLF_color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }
    break;
  }
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
  const main_out tint_symbol_4 = {x_GLF_color};
  return tint_symbol_4;
}

tint_symbol_2 main(tint_symbol_1 tint_symbol) {
  const main_out inner_result = main_inner(tint_symbol.gl_FragCoord_param);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.x_GLF_color_1 = inner_result.x_GLF_color_1;
  return wrapper_result;
}
C:\src\tint\test\Shader@0x0000020EFE36E240(8,10-21): warning X3557: loop only executes for 0 iteration(s), consider removing [loop]
C:\src\tint\test\Shader@0x0000020EFE36E240(71,16-27): warning X3557: loop only executes for 0 iteration(s), consider removing [loop]
C:\src\tint\test\Shader@0x0000020EFE36E240(186,14-25): warning X3557: loop only executes for 0 iteration(s), consider removing [loop]
C:\src\tint\test\Shader@0x0000020EFE36E240(123,18-29): error X4029: infinite loop detected - loop never exits

