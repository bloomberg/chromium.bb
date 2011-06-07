// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pixel shader used to do a bloom. It's expected that the consumers set
// TexelKernel before using.

Texture2D textureMap;

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
  AddressU = Clamp;
  AddressV = Clamp;
};

VS_OUT VS(VS_IN vIn) {
  VS_OUT vOut;
  vOut.posW = mul(float4(vIn.posL, 1.0f), gWVP);
  vOut.texC = vIn.texC;
  return vOut;
}

// Size of the kernel.
static const int g_cKernelSize = 13;

static const float BlurWeights[g_cKernelSize] = {
1.0f / 4096.0f,
12.0f / 4096.0f,
66.0f / 4096.0f,
220.0f / 4096.0f,
495.0f / 4096.0f,
792.0f / 4096.0f,
924.0f / 4096.0f,
792.0f / 4096.0f,
495.0f / 4096.0f,
220.0f / 4096.0f,
66.0f / 4096.0f,
12.0f / 4096.0f,
1.0f / 4096.0f,
};

// Set by consumer as it depends upon width/height of viewport.
float4 TexelKernel[g_cKernelSize];

float4 PS(VS_OUT pIn) : SV_Target {
  float4 c = 0;
  for (int i = 0; i < g_cKernelSize; i++)  {
    c += textureMap.Sample(Sampler, float2(pIn.texC) + TexelKernel[i].xy) *
                           BlurWeights[i];
  }
  return c;
}

technique10 ViewTech {
  pass P0 {
    SetVertexShader(CompileShader(vs_4_0, VS()));
    SetGeometryShader(NULL);
    SetPixelShader(CompileShader(ps_4_0, PS()));
  }
}
