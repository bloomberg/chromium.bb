// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HLSL file used by views.

Texture2D textureMap;

// Blender to give source-over for textures.
BlendState SrcAlphaBlendingAdd {
  BlendEnable[0] = TRUE;
  SrcBlend = ONE;
  DestBlend = INV_SRC_ALPHA;
  BlendOp = ADD;
  SrcBlendAlpha = ONE;
  DestBlendAlpha = INV_SRC_ALPHA;
  BlendOpAlpha = ADD;
  RenderTargetWriteMask[0] = 0x0F;  // Enables for all color components.
};

// Avoids doing depth detection, which would normally mean only the frontmost
// texture is drawn.
DepthStencilState StencilState {
  DepthEnable = false;
};

cbuffer cbPerObject {
  float4x4 gWVP;
};

struct VS_IN {
  float3 posL : POSITION;
  float2 texC : TEXC;
};

struct VS_OUT {
  float4 posW : SV_POSITION;
  float2 texC : TEXC;
};

SamplerState Sampler {
  Filter = MIN_MAG_MIP_POINT;
  AddressU = Wrap;
  AddressV = Wrap;
};

VS_OUT VS(VS_IN vIn) {
  VS_OUT vOut;
  vOut.posW = mul(float4(vIn.posL, 1.0f), gWVP);
  vOut.texC = vIn.texC;
  return vOut;
}

float alpha = 1.0f;

float4 PS(VS_OUT pIn) : SV_Target {
  return textureMap.Sample(Sampler, float2(pIn.texC)) * alpha;
}

technique10 ViewTech {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, PS()));
    SetDepthStencilState(StencilState, 0);
    SetBlendState(SrcAlphaBlendingAdd, float4(0.0f, 0.0f, 0.0f, 0.0f),
                  0xFFFFFFFF);
  }
}
