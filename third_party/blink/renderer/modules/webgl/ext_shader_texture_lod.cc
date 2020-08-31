// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgl/ext_shader_texture_lod.h"

namespace blink {

EXTShaderTextureLOD::EXTShaderTextureLOD(WebGLRenderingContextBase* context)
    : WebGLExtension(context) {
  context->ExtensionsUtil()->EnsureExtensionEnabled(
      "GL_EXT_shader_texture_lod");
}

WebGLExtensionName EXTShaderTextureLOD::GetName() const {
  return kEXTShaderTextureLODName;
}

bool EXTShaderTextureLOD::Supported(WebGLRenderingContextBase* context) {
  return context->ExtensionsUtil()->SupportsExtension(
      "GL_EXT_shader_texture_lod");
}

const char* EXTShaderTextureLOD::ExtensionName() {
  return "EXT_shader_texture_lod";
}

}  // namespace blink
