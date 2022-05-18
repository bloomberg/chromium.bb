bug/tint/1118.wgsl:64:31 warning: 'dpdx' must only be called from uniform control flow
  normalW = normalize(-(cross(dpdx(x_62), dpdy(x_64))));
                              ^^^^

bug/tint/1118.wgsl:46:19 note: reading from module-scope private variable 'fClipDistance3' may result in a non-uniform value
  let x_9 : f32 = fClipDistance3;
                  ^^^^^^^^^^^^^^

#version 310 es
precision mediump float;

layout(location = 2) in float fClipDistance3_param_1;
layout(location = 3) in float fClipDistance4_param_1;
layout(location = 0) out vec4 glFragColor_1_1;
struct Scene {
  vec4 vEyePosition;
};

struct Material {
  vec4 vDiffuseColor;
  vec3 vAmbientColor;
  float placeholder;
  vec3 vEmissiveColor;
  float placeholder2;
};

struct Mesh {
  float visibility;
};

float fClipDistance3 = 0.0f;
float fClipDistance4 = 0.0f;
layout(binding = 0) uniform Scene_1 {
  vec4 vEyePosition;
} x_29;

layout(binding = 1) uniform Material_1 {
  vec4 vDiffuseColor;
  vec3 vAmbientColor;
  float placeholder;
  vec3 vEmissiveColor;
  float placeholder2;
} x_49;

layout(binding = 2) uniform Mesh_1 {
  float visibility;
} x_137;

vec4 glFragColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
bool tint_discard = false;
void main_1() {
  vec3 viewDirectionW = vec3(0.0f, 0.0f, 0.0f);
  vec4 baseColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec3 diffuseColor = vec3(0.0f, 0.0f, 0.0f);
  float alpha = 0.0f;
  vec3 normalW = vec3(0.0f, 0.0f, 0.0f);
  vec2 uvOffset = vec2(0.0f, 0.0f);
  vec3 baseAmbientColor = vec3(0.0f, 0.0f, 0.0f);
  float glossiness = 0.0f;
  vec3 diffuseBase = vec3(0.0f, 0.0f, 0.0f);
  float shadow = 0.0f;
  vec4 refractionColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec4 reflectionColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec3 emissiveColor = vec3(0.0f, 0.0f, 0.0f);
  vec3 finalDiffuse = vec3(0.0f, 0.0f, 0.0f);
  vec3 finalSpecular = vec3(0.0f, 0.0f, 0.0f);
  vec4 color = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((fClipDistance3 > 0.0f)) {
    tint_discard = true;
    return;
  }
  if ((fClipDistance4 > 0.0f)) {
    tint_discard = true;
    return;
  }
  vec4 x_34 = x_29.vEyePosition;
  vec3 x_38 = vec3(0.0f, 0.0f, 0.0f);
  viewDirectionW = normalize((vec3(x_34.x, x_34.y, x_34.z) - x_38));
  baseColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
  vec4 x_52 = x_49.vDiffuseColor;
  diffuseColor = vec3(x_52.x, x_52.y, x_52.z);
  float x_60 = x_49.vDiffuseColor.w;
  alpha = x_60;
  vec3 x_62 = vec3(0.0f, 0.0f, 0.0f);
  vec3 x_64 = vec3(0.0f, 0.0f, 0.0f);
  normalW = normalize(-(cross(dFdx(x_62), dFdy(x_64))));
  uvOffset = vec2(0.0f, 0.0f);
  vec4 x_74 = vec4(0.0f, 0.0f, 0.0f, 0.0f);
  vec4 x_76 = baseColor;
  vec3 x_78 = (vec3(x_76.x, x_76.y, x_76.z) * vec3(x_74.x, x_74.y, x_74.z));
  baseColor = vec4(x_78.x, x_78.y, x_78.z, baseColor.w);
  baseAmbientColor = vec3(1.0f, 1.0f, 1.0f);
  glossiness = 0.0f;
  diffuseBase = vec3(0.0f, 0.0f, 0.0f);
  shadow = 1.0f;
  refractionColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  reflectionColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
  vec3 x_94 = x_49.vEmissiveColor;
  emissiveColor = x_94;
  vec3 x_96 = diffuseBase;
  vec3 x_97 = diffuseColor;
  vec3 x_99 = emissiveColor;
  vec3 x_103 = x_49.vAmbientColor;
  vec4 x_108 = baseColor;
  finalDiffuse = (clamp((((x_96 * x_97) + x_99) + x_103), vec3(0.0f, 0.0f, 0.0f), vec3(1.0f, 1.0f, 1.0f)) * vec3(x_108.x, x_108.y, x_108.z));
  finalSpecular = vec3(0.0f, 0.0f, 0.0f);
  vec4 x_118 = reflectionColor;
  vec4 x_121 = refractionColor;
  vec3 x_123 = ((((finalDiffuse * baseAmbientColor) + finalSpecular) + vec3(x_118.x, x_118.y, x_118.z)) + vec3(x_121.x, x_121.y, x_121.z));
  color = vec4(x_123.x, x_123.y, x_123.z, alpha);
  vec4 x_129 = color;
  vec3 x_132 = max(vec3(x_129.x, x_129.y, x_129.z), vec3(0.0f, 0.0f, 0.0f));
  color = vec4(x_132.x, x_132.y, x_132.z, color.w);
  float x_140 = x_137.visibility;
  float x_142 = color.w;
  color.w = (x_142 * x_140);
  glFragColor = color;
  return;
}

struct main_out {
  vec4 glFragColor_1;
};

main_out tint_symbol(float fClipDistance3_param, float fClipDistance4_param) {
  fClipDistance3 = fClipDistance3_param;
  fClipDistance4 = fClipDistance4_param;
  main_1();
  if (tint_discard) {
    main_out tint_symbol_1 = main_out(vec4(0.0f, 0.0f, 0.0f, 0.0f));
    return tint_symbol_1;
  }
  main_out tint_symbol_2 = main_out(glFragColor);
  return tint_symbol_2;
}

void tint_discard_func() {
  discard;
}

void main() {
  main_out inner_result = tint_symbol(fClipDistance3_param_1, fClipDistance4_param_1);
  if (tint_discard) {
    tint_discard_func();
    return;
  }
  glFragColor_1_1 = inner_result.glFragColor_1;
  return;
}
