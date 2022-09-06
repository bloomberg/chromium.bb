bug/dawn/947.wgsl:59:20 warning: 'textureSample' must only be called from uniform control flow
    var srcColor = textureSample(myTexture, mySampler, texcoord);
                   ^^^^^^^^^^^^^

bug/dawn/947.wgsl:55:5 note: control flow depends on non-uniform value
    if (!all(clampedTexcoord == texcoord)) {
    ^^

bug/dawn/947.wgsl:55:33 note: reading from user-defined input 'texcoord' may result in a non-uniform value
    if (!all(clampedTexcoord == texcoord)) {
                                ^^^^^^^^

cbuffer cbuffer_uniforms : register(b0, space0) {
  uint4 uniforms[1];
};

struct VertexOutputs {
  float2 texcoords;
  float4 position;
};
struct tint_symbol_1 {
  uint VertexIndex : SV_VertexID;
};
struct tint_symbol_2 {
  float2 texcoords : TEXCOORD0;
  float4 position : SV_Position;
};

VertexOutputs vs_main_inner(uint VertexIndex) {
  float2 texcoord[3] = {float2(-0.5f, 0.0f), float2(1.5f, 0.0f), float2(0.5f, 2.0f)};
  VertexOutputs output = (VertexOutputs)0;
  output.position = float4(((texcoord[VertexIndex] * 2.0f) - (1.0f).xx), 0.0f, 1.0f);
  bool flipY = (asfloat(uniforms[0].y) < 0.0f);
  if (flipY) {
    output.texcoords = ((((texcoord[VertexIndex] * asfloat(uniforms[0].xy)) + asfloat(uniforms[0].zw)) * float2(1.0f, -1.0f)) + float2(0.0f, 1.0f));
  } else {
    output.texcoords = ((((texcoord[VertexIndex] * float2(1.0f, -1.0f)) + float2(0.0f, 1.0f)) * asfloat(uniforms[0].xy)) + asfloat(uniforms[0].zw));
  }
  return output;
}

tint_symbol_2 vs_main(tint_symbol_1 tint_symbol) {
  const VertexOutputs inner_result = vs_main_inner(tint_symbol.VertexIndex);
  tint_symbol_2 wrapper_result = (tint_symbol_2)0;
  wrapper_result.texcoords = inner_result.texcoords;
  wrapper_result.position = inner_result.position;
  return wrapper_result;
}

SamplerState mySampler : register(s1, space0);
Texture2D<float4> myTexture : register(t2, space0);

struct tint_symbol_4 {
  float2 texcoord : TEXCOORD0;
};
struct tint_symbol_5 {
  float4 value : SV_Target0;
};

static bool tint_discard = false;

float4 fs_main_inner(float2 texcoord) {
  float2 clampedTexcoord = clamp(texcoord, (0.0f).xx, (1.0f).xx);
  if (!(all((clampedTexcoord == texcoord)))) {
    tint_discard = true;
    return (0.0f).xxxx;
  }
  float4 srcColor = myTexture.Sample(mySampler, texcoord);
  return srcColor;
}

void tint_discard_func() {
  discard;
}

tint_symbol_5 fs_main(tint_symbol_4 tint_symbol_3) {
  const float4 inner_result_1 = fs_main_inner(tint_symbol_3.texcoord);
  if (tint_discard) {
    tint_discard_func();
    const tint_symbol_5 tint_symbol_8 = (tint_symbol_5)0;
    return tint_symbol_8;
  }
  tint_symbol_5 wrapper_result_1 = (tint_symbol_5)0;
  wrapper_result_1.value = inner_result_1;
  return wrapper_result_1;
}
