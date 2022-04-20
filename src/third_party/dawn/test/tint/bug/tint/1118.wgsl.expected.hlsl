static float fClipDistance3 = 0.0f;
static float fClipDistance4 = 0.0f;
cbuffer cbuffer_x_29 : register(b0, space0) {
  uint4 x_29[1];
};
cbuffer cbuffer_x_49 : register(b1, space0) {
  uint4 x_49[3];
};
cbuffer cbuffer_x_137 : register(b2, space0) {
  uint4 x_137[1];
};
static float4 glFragColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
static bool tint_discard = false;

void main_1() {
  float3 viewDirectionW = float3(0.0f, 0.0f, 0.0f);
  float4 baseColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float3 diffuseColor = float3(0.0f, 0.0f, 0.0f);
  float alpha = 0.0f;
  float3 normalW = float3(0.0f, 0.0f, 0.0f);
  float2 uvOffset = float2(0.0f, 0.0f);
  float3 baseAmbientColor = float3(0.0f, 0.0f, 0.0f);
  float glossiness = 0.0f;
  float3 diffuseBase = float3(0.0f, 0.0f, 0.0f);
  float shadow = 0.0f;
  float4 refractionColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float4 reflectionColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float3 emissiveColor = float3(0.0f, 0.0f, 0.0f);
  float3 finalDiffuse = float3(0.0f, 0.0f, 0.0f);
  float3 finalSpecular = float3(0.0f, 0.0f, 0.0f);
  float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
  if ((fClipDistance3 > 0.0f)) {
    tint_discard = true;
    return;
  }
  if ((fClipDistance4 > 0.0f)) {
    tint_discard = true;
    return;
  }
  const float4 x_34 = asfloat(x_29[0]);
  const float3 x_38 = float3(0.0f, 0.0f, 0.0f);
  viewDirectionW = normalize((float3(x_34.x, x_34.y, x_34.z) - x_38));
  baseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
  const float4 x_52 = asfloat(x_49[0]);
  diffuseColor = float3(x_52.x, x_52.y, x_52.z);
  const float x_60 = asfloat(x_49[0].w);
  alpha = x_60;
  const float3 x_62 = float3(0.0f, 0.0f, 0.0f);
  const float3 x_64 = float3(0.0f, 0.0f, 0.0f);
  normalW = normalize(-(cross(ddx(x_62), ddy(x_64))));
  uvOffset = float2(0.0f, 0.0f);
  const float4 x_74 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  const float4 x_76 = baseColor;
  const float3 x_78 = (float3(x_76.x, x_76.y, x_76.z) * float3(x_74.x, x_74.y, x_74.z));
  baseColor = float4(x_78.x, x_78.y, x_78.z, baseColor.w);
  baseAmbientColor = float3(1.0f, 1.0f, 1.0f);
  glossiness = 0.0f;
  diffuseBase = float3(0.0f, 0.0f, 0.0f);
  shadow = 1.0f;
  refractionColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
  reflectionColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
  const float3 x_94 = asfloat(x_49[2].xyz);
  emissiveColor = x_94;
  const float3 x_96 = diffuseBase;
  const float3 x_97 = diffuseColor;
  const float3 x_99 = emissiveColor;
  const float3 x_103 = asfloat(x_49[1].xyz);
  const float4 x_108 = baseColor;
  finalDiffuse = (clamp((((x_96 * x_97) + x_99) + x_103), float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f)) * float3(x_108.x, x_108.y, x_108.z));
  finalSpecular = float3(0.0f, 0.0f, 0.0f);
  const float4 x_118 = reflectionColor;
  const float4 x_121 = refractionColor;
  const float3 x_123 = ((((finalDiffuse * baseAmbientColor) + finalSpecular) + float3(x_118.x, x_118.y, x_118.z)) + float3(x_121.x, x_121.y, x_121.z));
  color = float4(x_123.x, x_123.y, x_123.z, alpha);
  const float4 x_129 = color;
  const float3 x_132 = max(float3(x_129.x, x_129.y, x_129.z), float3(0.0f, 0.0f, 0.0f));
  color = float4(x_132.x, x_132.y, x_132.z, color.w);
  const float x_140 = asfloat(x_137[0].x);
  const float x_142 = color.w;
  color.w = (x_142 * x_140);
  glFragColor = color;
  return;
}

struct main_out {
  float4 glFragColor_1;
};
struct tint_symbol_1 {
  float fClipDistance3_param : TEXCOORD2;
  float fClipDistance4_param : TEXCOORD3;
};
struct tint_symbol_2 {
  float4 glFragColor_1 : SV_Target0;
};

main_out main_inner(float fClipDistance3_param, float fClipDistance4_param) {
  fClipDistance3 = fClipDistance3_param;
  fClipDistance4 = fClipDistance4_param;
  main_1();
  if (tint_discard) {
    const main_out tint_symbol_8 = (main_out)0;
    return tint_symbol_8;
  }
  const main_out tint_symbol_9 = {glFragColor};
  return tint_symbol_9;
}

void tint_discard_func() {
  discard;
}

tint_symbol_2 main(tint_symbol_1 tint_symbol) {
  const main_out inner_result = main_inner(tint_symbol.fClipDistance3_param, tint_symbol.fClipDistance4_param);
  if (tint_discard) {
    tint_discard_func();
    const tint_symbol_2 tint_symbol_10 = (tint_symbol_2)0;
    return tint_symbol_10;
  }
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.glFragColor_1 = inner_result.glFragColor_1;
  return wrapper_result;
}
