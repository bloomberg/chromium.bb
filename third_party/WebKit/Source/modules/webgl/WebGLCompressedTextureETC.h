// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureETC_h
#define WebGLCompressedTextureETC_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGLCompressedTextureETC final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WebGLCompressedTextureETC* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;

 private:
  explicit WebGLCompressedTextureETC(WebGLRenderingContextBase*);
};

}  // namespace blink

#endif  // WebGLCompressedTextureETC_h
