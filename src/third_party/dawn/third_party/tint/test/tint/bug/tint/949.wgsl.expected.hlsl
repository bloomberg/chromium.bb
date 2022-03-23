struct lightingInfo {
  float3 diffuse;
  float3 specular;
};

static float u_Float = 0.0f;
static float3 u_Color = float3(0.0f, 0.0f, 0.0f);
Texture2D<float4> TextureSamplerTexture : register(t1, space2);
SamplerState TextureSamplerSampler : register(s0, space2);
static float2 vMainuv = float2(0.0f, 0.0f);
cbuffer cbuffer_x_269 : register(b6, space2) {
  uint4 x_269[11];
};
static float4 v_output1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
static bool gl_FrontFacing = false;
static float2 v_uv = float2(0.0f, 0.0f);
static float4 v_output2 = float4(0.0f, 0.0f, 0.0f, 0.0f);
Texture2D<float4> TextureSampler1Texture : register(t3, space2);
SamplerState TextureSampler1Sampler : register(s2, space2);
cbuffer cbuffer_light0 : register(b5, space0) {
  uint4 light0[6];
};
static float4 glFragColor = float4(0.0f, 0.0f, 0.0f, 0.0f);
SamplerState bumpSamplerSampler : register(s4, space2);
Texture2D<float4> bumpSamplerTexture : register(t5, space2);

float3x3 cotangent_frame_vf3_vf3_vf2_vf2_(inout float3 normal_1, inout float3 p, inout float2 uv, inout float2 tangentSpaceParams) {
  float3 dp1 = float3(0.0f, 0.0f, 0.0f);
  float3 dp2 = float3(0.0f, 0.0f, 0.0f);
  float2 duv1 = float2(0.0f, 0.0f);
  float2 duv2 = float2(0.0f, 0.0f);
  float3 dp2perp = float3(0.0f, 0.0f, 0.0f);
  float3 dp1perp = float3(0.0f, 0.0f, 0.0f);
  float3 tangent = float3(0.0f, 0.0f, 0.0f);
  float3 bitangent = float3(0.0f, 0.0f, 0.0f);
  float invmax = 0.0f;
  const float3 x_133 = p;
  dp1 = ddx(x_133);
  const float3 x_136 = p;
  dp2 = ddy(x_136);
  const float2 x_139 = uv;
  duv1 = ddx(x_139);
  const float2 x_142 = uv;
  duv2 = ddy(x_142);
  const float3 x_145 = dp2;
  const float3 x_146 = normal_1;
  dp2perp = cross(x_145, x_146);
  const float3 x_149 = normal_1;
  dp1perp = cross(x_149, dp1);
  const float3 x_153 = dp2perp;
  const float x_155 = duv1.x;
  const float3 x_157 = dp1perp;
  const float x_159 = duv2.x;
  tangent = ((x_153 * x_155) + (x_157 * x_159));
  const float3 x_163 = dp2perp;
  const float x_165 = duv1.y;
  const float3 x_167 = dp1perp;
  const float x_169 = duv2.y;
  bitangent = ((x_163 * x_165) + (x_167 * x_169));
  const float x_173 = tangentSpaceParams.x;
  tangent = (tangent * x_173);
  const float x_177 = tangentSpaceParams.y;
  bitangent = (bitangent * x_177);
  invmax = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
  const float3 x_191 = (tangent * invmax);
  const float3 x_194 = (bitangent * invmax);
  const float3 x_195 = normal_1;
  return float3x3(float3(x_191.x, x_191.y, x_191.z), float3(x_194.x, x_194.y, x_194.z), float3(x_195.x, x_195.y, x_195.z));
}

float3x3 transposeMat3_mf33_(inout float3x3 inMatrix) {
  float3 i0 = float3(0.0f, 0.0f, 0.0f);
  float3 i1 = float3(0.0f, 0.0f, 0.0f);
  float3 i2 = float3(0.0f, 0.0f, 0.0f);
  float3x3 outMatrix = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  const float3 x_60 = inMatrix[0];
  i0 = x_60;
  const float3 x_64 = inMatrix[1];
  i1 = x_64;
  const float3 x_68 = inMatrix[2];
  i2 = x_68;
  const float x_73 = i0.x;
  const float x_75 = i1.x;
  const float x_77 = i2.x;
  const float3 x_78 = float3(x_73, x_75, x_77);
  const float x_81 = i0.y;
  const float x_83 = i1.y;
  const float x_85 = i2.y;
  const float3 x_86 = float3(x_81, x_83, x_85);
  const float x_89 = i0.z;
  const float x_91 = i1.z;
  const float x_93 = i2.z;
  const float3 x_94 = float3(x_89, x_91, x_93);
  outMatrix = float3x3(float3(x_78.x, x_78.y, x_78.z), float3(x_86.x, x_86.y, x_86.z), float3(x_94.x, x_94.y, x_94.z));
  return outMatrix;
}

float3 perturbNormalBase_mf33_vf3_f1_(inout float3x3 cotangentFrame, inout float3 normal, inout float scale) {
  const float3x3 x_113 = cotangentFrame;
  const float3 x_114 = normal;
  return normalize(mul(x_114, x_113));
}

float3 perturbNormal_mf33_vf3_f1_(inout float3x3 cotangentFrame_1, inout float3 textureSample, inout float scale_1) {
  float3x3 param = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float3 param_1 = float3(0.0f, 0.0f, 0.0f);
  float param_2 = 0.0f;
  const float3 x_119 = textureSample;
  const float3x3 x_125 = cotangentFrame_1;
  param = x_125;
  param_1 = ((x_119 * 2.0f) - float3(1.0f, 1.0f, 1.0f));
  const float x_128 = scale_1;
  param_2 = x_128;
  const float3 x_129 = perturbNormalBase_mf33_vf3_f1_(param, param_1, param_2);
  return x_129;
}

lightingInfo computeHemisphericLighting_vf3_vf3_vf4_vf3_vf3_vf3_f1_(inout float3 viewDirectionW, inout float3 vNormal, inout float4 lightData, inout float3 diffuseColor, inout float3 specularColor, inout float3 groundColor, inout float glossiness) {
  float ndl = 0.0f;
  lightingInfo result = (lightingInfo)0;
  float3 angleW = float3(0.0f, 0.0f, 0.0f);
  float specComp = 0.0f;
  const float3 x_212 = vNormal;
  const float4 x_213 = lightData;
  ndl = ((dot(x_212, float3(x_213.x, x_213.y, x_213.z)) * 0.5f) + 0.5f);
  const float3 x_220 = groundColor;
  const float3 x_221 = diffuseColor;
  const float x_222 = ndl;
  result.diffuse = lerp(x_220, x_221, float3(x_222, x_222, x_222));
  const float3 x_227 = viewDirectionW;
  const float4 x_228 = lightData;
  angleW = normalize((x_227 + float3(x_228.x, x_228.y, x_228.z)));
  const float3 x_233 = vNormal;
  specComp = max(0.0f, dot(x_233, angleW));
  const float x_237 = specComp;
  const float x_238 = glossiness;
  specComp = pow(x_237, max(1.0f, x_238));
  const float x_241 = specComp;
  const float3 x_242 = specularColor;
  result.specular = (x_242 * x_241);
  return result;
}

void main_1() {
  float4 tempTextureRead = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float3 rgb = float3(0.0f, 0.0f, 0.0f);
  float3 output5 = float3(0.0f, 0.0f, 0.0f);
  float4 output4 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float2 uvOffset = float2(0.0f, 0.0f);
  float normalScale = 0.0f;
  float2 TBNUV = float2(0.0f, 0.0f);
  float2 x_299 = float2(0.0f, 0.0f);
  float3x3 TBN = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float3 param_3 = float3(0.0f, 0.0f, 0.0f);
  float3 param_4 = float3(0.0f, 0.0f, 0.0f);
  float2 param_5 = float2(0.0f, 0.0f);
  float2 param_6 = float2(0.0f, 0.0f);
  float3x3 invTBN = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float3x3 param_7 = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float parallaxLimit = 0.0f;
  float2 vOffsetDir = float2(0.0f, 0.0f);
  float2 vMaxOffset = float2(0.0f, 0.0f);
  float numSamples = 0.0f;
  float stepSize = 0.0f;
  float currRayHeight = 0.0f;
  float2 vCurrOffset = float2(0.0f, 0.0f);
  float2 vLastOffset = float2(0.0f, 0.0f);
  float lastSampledHeight = 0.0f;
  float currSampledHeight = 0.0f;
  int i = 0;
  float delta1 = 0.0f;
  float delta2 = 0.0f;
  float ratio = 0.0f;
  float2 parallaxOcclusion_0 = float2(0.0f, 0.0f);
  float3x3 param_8 = float3x3(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  float3 param_9 = float3(0.0f, 0.0f, 0.0f);
  float param_10 = 0.0f;
  float2 output6 = float2(0.0f, 0.0f);
  float4 tempTextureRead1 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float3 rgb1 = float3(0.0f, 0.0f, 0.0f);
  float3 viewDirectionW_1 = float3(0.0f, 0.0f, 0.0f);
  float shadow = 0.0f;
  float glossiness_1 = 0.0f;
  float3 diffuseBase = float3(0.0f, 0.0f, 0.0f);
  float3 specularBase = float3(0.0f, 0.0f, 0.0f);
  float3 normalW = float3(0.0f, 0.0f, 0.0f);
  lightingInfo info = (lightingInfo)0;
  float3 param_11 = float3(0.0f, 0.0f, 0.0f);
  float3 param_12 = float3(0.0f, 0.0f, 0.0f);
  float4 param_13 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  float3 param_14 = float3(0.0f, 0.0f, 0.0f);
  float3 param_15 = float3(0.0f, 0.0f, 0.0f);
  float3 param_16 = float3(0.0f, 0.0f, 0.0f);
  float param_17 = 0.0f;
  float3 diffuseOutput = float3(0.0f, 0.0f, 0.0f);
  float3 specularOutput = float3(0.0f, 0.0f, 0.0f);
  float3 output3 = float3(0.0f, 0.0f, 0.0f);
  u_Float = 100.0f;
  u_Color = float3(0.5f, 0.5f, 0.5f);
  const float4 x_262 = TextureSamplerTexture.Sample(TextureSamplerSampler, vMainuv);
  tempTextureRead = x_262;
  const float4 x_264 = tempTextureRead;
  const float x_273 = asfloat(x_269[10].x);
  rgb = (float3(x_264.x, x_264.y, x_264.z) * x_273);
  const float3 x_279 = asfloat(x_269[9].xyz);
  const float4 x_282 = v_output1;
  output5 = normalize((x_279 - float3(x_282.x, x_282.y, x_282.z)));
  output4 = float4(0.0f, 0.0f, 0.0f, 0.0f);
  uvOffset = float2(0.0f, 0.0f);
  const float x_292 = asfloat(x_269[8].x);
  normalScale = (1.0f / x_292);
  if (gl_FrontFacing) {
    x_299 = v_uv;
  } else {
    x_299 = -(v_uv);
  }
  TBNUV = x_299;
  const float4 x_310 = v_output2;
  param_3 = (float3(x_310.x, x_310.y, x_310.z) * normalScale);
  const float4 x_317 = v_output1;
  param_4 = float3(x_317.x, x_317.y, x_317.z);
  param_5 = TBNUV;
  const float2 x_324 = asfloat(x_269[10].zw);
  param_6 = x_324;
  const float3x3 x_325 = cotangent_frame_vf3_vf3_vf2_vf2_(param_3, param_4, param_5, param_6);
  TBN = x_325;
  param_7 = TBN;
  const float3x3 x_329 = transposeMat3_mf33_(param_7);
  invTBN = x_329;
  const float3 x_334 = mul(-(output5), invTBN);
  parallaxLimit = (length(float2(x_334.x, x_334.y)) / mul(-(output5), invTBN).z);
  const float x_345 = asfloat(x_269[9].w);
  parallaxLimit = (parallaxLimit * x_345);
  const float3 x_352 = mul(-(output5), invTBN);
  vOffsetDir = normalize(float2(x_352.x, x_352.y));
  vMaxOffset = (vOffsetDir * parallaxLimit);
  const float4 x_366 = v_output2;
  numSamples = (15.0f + (dot(mul(-(output5), invTBN), mul(float3(x_366.x, x_366.y, x_366.z), invTBN)) * -11.0f));
  stepSize = (1.0f / numSamples);
  currRayHeight = 1.0f;
  vCurrOffset = float2(0.0f, 0.0f);
  vLastOffset = float2(0.0f, 0.0f);
  lastSampledHeight = 1.0f;
  currSampledHeight = 1.0f;
  i = 0;
  {
    [loop] for(; (i < 15); i = (i + 1)) {
      const float4 x_397 = TextureSamplerTexture.Sample(TextureSamplerSampler, (v_uv + vCurrOffset));
      currSampledHeight = x_397.w;
      if ((currSampledHeight > currRayHeight)) {
        delta1 = (currSampledHeight - currRayHeight);
        delta2 = ((currRayHeight + stepSize) - lastSampledHeight);
        ratio = (delta1 / (delta1 + delta2));
        vCurrOffset = ((vLastOffset * ratio) + (vCurrOffset * (1.0f - ratio)));
        break;
      } else {
        currRayHeight = (currRayHeight - stepSize);
        vLastOffset = vCurrOffset;
        vCurrOffset = (vCurrOffset + (vMaxOffset * stepSize));
        lastSampledHeight = currSampledHeight;
      }
    }
  }
  parallaxOcclusion_0 = vCurrOffset;
  uvOffset = parallaxOcclusion_0;
  const float4 x_452 = TextureSamplerTexture.Sample(TextureSamplerSampler, (v_uv + uvOffset));
  const float x_454 = asfloat(x_269[8].x);
  param_8 = TBN;
  param_9 = float3(x_452.x, x_452.y, x_452.z);
  param_10 = (1.0f / x_454);
  const float3 x_461 = perturbNormal_mf33_vf3_f1_(param_8, param_9, param_10);
  output4 = float4(x_461.x, x_461.y, x_461.z, output4.w);
  output6 = (v_uv + uvOffset);
  const float4 x_475 = TextureSampler1Texture.Sample(TextureSampler1Sampler, output6);
  tempTextureRead1 = x_475;
  const float4 x_477 = tempTextureRead1;
  rgb1 = float3(x_477.x, x_477.y, x_477.z);
  const float3 x_481 = asfloat(x_269[9].xyz);
  const float4 x_482 = v_output1;
  viewDirectionW_1 = normalize((x_481 - float3(x_482.x, x_482.y, x_482.z)));
  shadow = 1.0f;
  glossiness_1 = (1.0f * u_Float);
  diffuseBase = float3(0.0f, 0.0f, 0.0f);
  specularBase = float3(0.0f, 0.0f, 0.0f);
  const float4 x_494 = output4;
  normalW = float3(x_494.x, x_494.y, x_494.z);
  param_11 = viewDirectionW_1;
  param_12 = normalW;
  const float4 x_507 = asfloat(light0[0]);
  param_13 = x_507;
  const float4 x_510 = asfloat(light0[1]);
  param_14 = float3(x_510.x, x_510.y, x_510.z);
  const float4 x_514 = asfloat(light0[2]);
  param_15 = float3(x_514.x, x_514.y, x_514.z);
  const float3 x_518 = asfloat(light0[3].xyz);
  param_16 = x_518;
  param_17 = glossiness_1;
  const lightingInfo x_521 = computeHemisphericLighting_vf3_vf3_vf4_vf3_vf3_vf3_f1_(param_11, param_12, param_13, param_14, param_15, param_16, param_17);
  info = x_521;
  shadow = 1.0f;
  const float3 x_523 = info.diffuse;
  diffuseBase = (diffuseBase + (x_523 * shadow));
  const float3 x_529 = info.specular;
  specularBase = (specularBase + (x_529 * shadow));
  diffuseOutput = (diffuseBase * rgb1);
  specularOutput = (specularBase * u_Color);
  output3 = (diffuseOutput + specularOutput);
  const float3 x_548 = output3;
  glFragColor = float4(x_548.x, x_548.y, x_548.z, 1.0f);
  return;
}

struct main_out {
  float4 glFragColor_1;
};
struct tint_symbol_1 {
  float4 v_output1_param : TEXCOORD0;
  float2 vMainuv_param : TEXCOORD1;
  float4 v_output2_param : TEXCOORD2;
  float2 v_uv_param : TEXCOORD3;
  bool gl_FrontFacing_param : SV_IsFrontFace;
};
struct tint_symbol_2 {
  float4 glFragColor_1 : SV_Target0;
};

main_out main_inner(float2 vMainuv_param, float4 v_output1_param, bool gl_FrontFacing_param, float2 v_uv_param, float4 v_output2_param) {
  vMainuv = vMainuv_param;
  v_output1 = v_output1_param;
  gl_FrontFacing = gl_FrontFacing_param;
  v_uv = v_uv_param;
  v_output2 = v_output2_param;
  main_1();
  const main_out tint_symbol_8 = {glFragColor};
  return tint_symbol_8;
}

tint_symbol_2 main(tint_symbol_1 tint_symbol) {
  const main_out inner_result = main_inner(tint_symbol.vMainuv_param, tint_symbol.v_output1_param, tint_symbol.gl_FrontFacing_param, tint_symbol.v_uv_param, tint_symbol.v_output2_param);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.glFragColor_1 = inner_result.glFragColor_1;
  return wrapper_result;
}
