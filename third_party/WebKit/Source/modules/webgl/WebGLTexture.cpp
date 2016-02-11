/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLTexture.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLTexture* WebGLTexture::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLTexture(ctx);
}

WebGLTexture::WebGLTexture(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_target(0)
    , m_isWebGL2OrHigher(ctx->isWebGL2OrHigher())
    , m_baseLevel(0)
    , m_maxLevel(1000)
{
    setObject(ctx->webContext()->createTexture());
}

WebGLTexture::~WebGLTexture()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

void WebGLTexture::setTarget(GLenum target, GLint maxLevel)
{
    if (!object())
        return;
    // Target is finalized the first time bindTexture() is called.
    if (m_target)
        return;
    m_target = target;
}

void WebGLTexture::setParameteri(GLenum pname, GLint param)
{
    if (!object() || !m_target)
        return;
    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
        switch (param) {
        case GL_NEAREST:
        case GL_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_LINEAR:
            m_samplerState.minFilter = param;
            break;
        }
        break;
    case GL_TEXTURE_MAG_FILTER:
        switch (param) {
        case GL_NEAREST:
        case GL_LINEAR:
            m_samplerState.magFilter = param;
            break;
        }
        break;
    case GL_TEXTURE_WRAP_R:
        switch (param) {
        case GL_CLAMP_TO_EDGE:
        case GL_MIRRORED_REPEAT:
        case GL_REPEAT:
            m_samplerState.wrapR = param;
            break;
        }
        break;
    case GL_TEXTURE_WRAP_S:
        switch (param) {
        case GL_CLAMP_TO_EDGE:
        case GL_MIRRORED_REPEAT:
        case GL_REPEAT:
            m_samplerState.wrapS = param;
            break;
        }
        break;
    case GL_TEXTURE_WRAP_T:
        switch (param) {
        case GL_CLAMP_TO_EDGE:
        case GL_MIRRORED_REPEAT:
        case GL_REPEAT:
            m_samplerState.wrapT = param;
            break;
        }
        break;
    case GL_TEXTURE_BASE_LEVEL:
        if (m_isWebGL2OrHigher && param >= 0)
            m_baseLevel = param;
        break;
    case GL_TEXTURE_MAX_LEVEL:
        if (m_isWebGL2OrHigher && param >= 0)
            m_maxLevel = param;
        break;
    default:
        return;
    }
}

void WebGLTexture::setParameterf(GLenum pname, GLfloat param)
{
    if (!object() || !m_target)
        return;
    GLint iparam = static_cast<GLint>(param);
    setParameteri(pname, iparam);
}

bool WebGLTexture::isNPOT(GLsizei width, GLsizei height)
{
    ASSERT(width >= 0 && height >= 0);
    if (!width || !height)
        return false;
    if ((width & (width - 1)) || (height & (height - 1)))
        return true;
    return false;
}

void WebGLTexture::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteTexture(m_object);
    m_object = 0;
}

int WebGLTexture::mapTargetToIndex(GLenum target) const
{
    if (m_target == GL_TEXTURE_2D) {
        if (target == GL_TEXTURE_2D)
            return 0;
    } else if (m_target == GL_TEXTURE_CUBE_MAP) {
        switch (target) {
        case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            return 0;
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            return 1;
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            return 2;
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            return 3;
        case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            return 4;
        case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return 5;
        }
    } else if (m_target == GL_TEXTURE_3D) {
        if (target == GL_TEXTURE_3D)
            return 0;
    } else if (m_target == GL_TEXTURE_2D_ARRAY) {
        if (target == GL_TEXTURE_2D_ARRAY)
            return 0;
    }
    return -1;
}

GLint WebGLTexture::computeLevelCount(GLsizei width, GLsizei height, GLsizei depth)
{
    // return 1 + log2Floor(std::max(width, height));
    GLsizei n = std::max(std::max(width, height), depth);
    if (n <= 0)
        return 0;
    GLint log = 0;
    GLsizei value = n;
    for (int ii = 4; ii >= 0; --ii) {
        int shift = (1 << ii);
        GLsizei x = (value >> shift);
        if (x) {
            value = x;
            log += shift;
        }
    }
    ASSERT(value == 1);
    return log + 1;
}

// TODO(bajones): Logic surrounding relationship of internalFormat, format, and type needs to be revisisted for WebGL 2.0
GLenum WebGLTexture::getValidTypeForInternalFormat(GLenum internalFormat)
{
    switch (internalFormat) {
    case GL_R8:
        return GL_UNSIGNED_BYTE;
    case GL_R8_SNORM:
        return GL_BYTE;
    case GL_R16F:
        return GL_HALF_FLOAT;
    case GL_R32F:
        return GL_FLOAT;
    case GL_R8UI:
        return GL_UNSIGNED_BYTE;
    case GL_R8I:
        return GL_BYTE;
    case GL_R16UI:
        return GL_UNSIGNED_SHORT;
    case GL_R16I:
        return GL_SHORT;
    case GL_R32UI:
        return GL_UNSIGNED_INT;
    case GL_R32I:
        return GL_INT;
    case GL_RG8:
        return GL_UNSIGNED_BYTE;
    case GL_RG8_SNORM:
        return GL_BYTE;
    case GL_RG16F:
        return GL_HALF_FLOAT;
    case GL_RG32F:
        return GL_FLOAT;
    case GL_RG8UI:
        return GL_UNSIGNED_BYTE;
    case GL_RG8I:
        return GL_BYTE;
    case GL_RG16UI:
        return GL_UNSIGNED_SHORT;
    case GL_RG16I:
        return GL_SHORT;
    case GL_RG32UI:
        return GL_UNSIGNED_INT;
    case GL_RG32I:
        return GL_INT;
    case GL_RGB8:
        return GL_UNSIGNED_BYTE;
    case GL_SRGB8:
        return GL_UNSIGNED_BYTE;
    case GL_RGB565:
        return GL_UNSIGNED_SHORT_5_6_5;
    case GL_RGB8_SNORM:
        return GL_BYTE;
    case GL_R11F_G11F_B10F:
        return GL_UNSIGNED_INT_10F_11F_11F_REV;
    case GL_RGB9_E5:
        return GL_UNSIGNED_INT_5_9_9_9_REV;
    case GL_RGB16F:
        return GL_HALF_FLOAT;
    case GL_RGB32F:
        return GL_FLOAT;
    case GL_RGB8UI:
        return GL_UNSIGNED_BYTE;
    case GL_RGB8I:
        return GL_BYTE;
    case GL_RGB16UI:
        return GL_UNSIGNED_SHORT;
    case GL_RGB16I:
        return GL_SHORT;
    case GL_RGB32UI:
        return GL_UNSIGNED_INT;
    case GL_RGB32I:
        return GL_INT;
    case GL_RGBA8:
        return GL_UNSIGNED_BYTE;
    case GL_SRGB8_ALPHA8:
        return GL_UNSIGNED_BYTE;
    case GL_RGBA8_SNORM:
        return GL_BYTE;
    case GL_RGB5_A1:
        return GL_UNSIGNED_SHORT_5_5_5_1;
    case GL_RGBA4:
        return GL_UNSIGNED_SHORT_4_4_4_4;
    case GL_RGB10_A2:
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGBA16F:
        return GL_HALF_FLOAT;
    case GL_RGBA32F:
        return GL_FLOAT;
    case GL_RGBA8UI:
        return GL_UNSIGNED_BYTE;
    case GL_RGBA8I:
        return GL_BYTE;
    case GL_RGB10_A2UI:
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    case GL_RGBA16UI:
        return GL_UNSIGNED_SHORT;
    case GL_RGBA16I:
        return GL_SHORT;
    case GL_RGBA32I:
        return GL_INT;
    case GL_RGBA32UI:
        return GL_UNSIGNED_INT;
    case GL_DEPTH_COMPONENT16:
        return GL_UNSIGNED_SHORT;
    case GL_DEPTH_COMPONENT24:
        return GL_UNSIGNED_INT;
    case GL_DEPTH_COMPONENT32F:
        return GL_FLOAT;
    case GL_DEPTH24_STENCIL8:
        return GL_UNSIGNED_INT_24_8;
    case GL_DEPTH32F_STENCIL8:
        return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
    // Compressed types.
    case GL_ATC_RGB_AMD:
    case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD:
    case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD:
    case GL_ETC1_RGB8_OES:
    case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
    case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
    case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
    case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        return GL_UNSIGNED_BYTE;
    default:
        return GL_NONE;
    }
}

} // namespace blink
