// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @gyp_namespace(ui_surface)
// Compiles into C++ as 'accelerated_surface_transformer_win_hlsl_compiled.h'

struct Vertex {
  float4 position : POSITION;
  float2 texCoord : TEXCOORD0;
};

texture t;
sampler s;

// @gyp_compile(vs_2_0, vsOneTexture)
//
// Passes a position and texture coordinate to the pixel shader.
Vertex vsOneTexture(Vertex input) {
  return input;
};

// @gyp_compile(ps_2_0, psOneTexture)
//
// Samples a texture at the given texture coordinate and returns the result.
float4 psOneTexture(float2 texCoord : TEXCOORD0) : COLOR0 {
  return tex2D(s, texCoord);
};
