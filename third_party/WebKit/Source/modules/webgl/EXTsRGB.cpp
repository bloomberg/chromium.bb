// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/EXTsRGB.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

EXTsRGB::EXTsRGB(WebGLRenderingContextBase* context) : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled("GL_EXT_sRGB");
}

WebGLExtensionName EXTsRGB::GetName() const {
  return kEXTsRGBName;
}

EXTsRGB* EXTsRGB::Create(WebGLRenderingContextBase* context) {
  return new EXTsRGB(context);
}

bool EXTsRGB::Supported(WebGLRenderingContextBase* context) {
  Extensions3DUtil* extensions_util = context->ExtensionsUtil();
  return extensions_util->SupportsExtension("GL_EXT_sRGB");
}

const char* EXTsRGB::ExtensionName() {
  return "EXT_sRGB";
}

}  // namespace blink
