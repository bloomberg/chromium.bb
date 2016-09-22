// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLCompressedTextureES30_h
#define WebGLCompressedTextureES30_h

#include "modules/webgl/WebGLExtension.h"

namespace blink {

class WebGLCompressedTextureES30 final : public WebGLExtension {
    DEFINE_WRAPPERTYPEINFO();
public:
    static WebGLCompressedTextureES30* create(WebGLRenderingContextBase*);
    static bool supported(WebGLRenderingContextBase*);
    static const char* extensionName();

    ~WebGLCompressedTextureES30() override;
    WebGLExtensionName name() const override;

private:
    explicit WebGLCompressedTextureES30(WebGLRenderingContextBase*);
};

} // namespace blink

#endif // WebGLCompressedTextureES30_h
