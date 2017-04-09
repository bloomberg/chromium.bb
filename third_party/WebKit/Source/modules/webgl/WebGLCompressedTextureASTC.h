// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureASTC_h
#define WebGLCompressedTextureASTC_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGLCompressedTextureASTC final : public WebGLExtension {
  DEFINE_WRAPPERTYPEINFO();

 public:
  typedef struct {
    int compress_type;
    int block_width;
    int block_height;
  } BlockSizeCompressASTC;

  static WebGLCompressedTextureASTC* Create(WebGLRenderingContextBase*);
  static bool Supported(WebGLRenderingContextBase*);
  static const char* ExtensionName();

  WebGLExtensionName GetName() const override;
  static const BlockSizeCompressASTC kBlockSizeCompressASTC[];

 private:
  explicit WebGLCompressedTextureASTC(WebGLRenderingContextBase*);
};

}  // namespace blink

#endif  // WebGLCompressedTextureASTC_h
