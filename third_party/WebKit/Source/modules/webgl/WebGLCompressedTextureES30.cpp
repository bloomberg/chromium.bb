// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLCompressedTextureES30.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLCompressedTextureES30::WebGLCompressedTextureES30(WebGLRenderingContextBase* context)
    : WebGLExtension(context)
{
    context->addCompressedTextureFormat(GL_COMPRESSED_R11_EAC);
    context->addCompressedTextureFormat(GL_COMPRESSED_SIGNED_R11_EAC);
    context->addCompressedTextureFormat(GL_COMPRESSED_RGB8_ETC2);
    context->addCompressedTextureFormat(GL_COMPRESSED_SRGB8_ETC2);
    context->addCompressedTextureFormat(GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2);
    context->addCompressedTextureFormat(GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2);
    context->addCompressedTextureFormat(GL_COMPRESSED_RG11_EAC);
    context->addCompressedTextureFormat(GL_COMPRESSED_SIGNED_RG11_EAC);
    context->addCompressedTextureFormat(GL_COMPRESSED_RGBA8_ETC2_EAC);
    context->addCompressedTextureFormat(GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC);
}

WebGLCompressedTextureES30::~WebGLCompressedTextureES30()
{
}

WebGLExtensionName WebGLCompressedTextureES30::name() const
{
    return WebGLCompressedTextureES30Name;
}

WebGLCompressedTextureES30* WebGLCompressedTextureES30::create(WebGLRenderingContextBase* context)
{
    return new WebGLCompressedTextureES30(context);
}

bool WebGLCompressedTextureES30::supported(WebGLRenderingContextBase* context)
{
    Extensions3DUtil* extensionsUtil = context->extensionsUtil();
    return extensionsUtil->supportsExtension("GL_CHROMIUM_compressed_texture_es3_0");
}

const char* WebGLCompressedTextureES30::extensionName()
{
    return "WEBGL_compressed_texture_es3_0";
}

} // namespace blink
