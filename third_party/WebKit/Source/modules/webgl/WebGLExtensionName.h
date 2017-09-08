// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLExtensionName_h
#define WebGLExtensionName_h

namespace blink {

// Extension names are needed to properly wrap instances in JavaScript objects.
enum WebGLExtensionName {
  kANGLEInstancedArraysName,
  kEXTBlendMinMaxName,
  kEXTColorBufferFloatName,
  kEXTColorBufferHalfFloatName,
  kEXTDisjointTimerQueryName,
  kEXTDisjointTimerQueryWebGL2Name,
  kEXTFragDepthName,
  kEXTShaderTextureLODName,
  kEXTsRGBName,
  kEXTTextureFilterAnisotropicName,
  kOESElementIndexUintName,
  kOESStandardDerivativesName,
  kOESTextureFloatLinearName,
  kOESTextureFloatName,
  kOESTextureHalfFloatLinearName,
  kOESTextureHalfFloatName,
  kOESVertexArrayObjectName,
  kWebGLColorBufferFloatName,
  kWebGLCompressedTextureASTCName,
  kWebGLCompressedTextureATCName,
  kWebGLCompressedTextureETCName,
  kWebGLCompressedTextureETC1Name,
  kWebGLCompressedTexturePVRTCName,
  kWebGLCompressedTextureS3TCName,
  kWebGLCompressedTextureS3TCsRGBName,
  kWebGLDebugRendererInfoName,
  kWebGLDebugShadersName,
  kWebGLDepthTextureName,
  kWebGLDrawBuffersName,
  kWebGLGetBufferSubDataAsyncName,
  kWebGLLoseContextName,
  kWebGLExtensionNameCount,  // Must be the last entry
};
}  // namespace blink

#endif  // WebGLExtensionName_h
