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

#ifndef WebGLTexture_h
#define WebGLTexture_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "wtf/Vector.h"

namespace blink {

struct WebGLSamplerState {
    WebGLSamplerState();

    GLenum compreFunc;
    GLenum compreMode;
    GLenum magFilter;
    GLenum minFilter;
    GLenum wrapR;
    GLenum wrapS;
    GLenum wrapT;
    GLfloat maxLod;
    GLfloat minLod;
};

class WebGLTexture final : public WebGLSharedPlatform3DObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLTexture() override;

    static WebGLTexture* create(WebGLRenderingContextBase*);

    void setTarget(GLenum target, GLint maxLevel);
    void setParameteri(GLenum pname, GLint param);
    void setParameterf(GLenum pname, GLfloat param);

    GLenum getTarget() const { return m_target; }

    int getMinFilter() const { return m_samplerState.minFilter; }

    static GLenum getValidFormatForInternalFormat(GLenum);

    // Whether width/height is NotPowerOfTwo.
    static bool isNPOT(GLsizei, GLsizei);

    bool hasEverBeenBound() const { return object() && m_target; }

    static GLint computeLevelCount(GLsizei width, GLsizei height, GLsizei depth);
    static GLenum getValidTypeForInternalFormat(GLenum);

    const WebGLSamplerState* getSamplerState() const { return &m_samplerState; }

private:
    explicit WebGLTexture(WebGLRenderingContextBase*);

    void deleteObjectImpl(WebGraphicsContext3D*) override;

    bool isTexture() const override { return true; }

    int mapTargetToIndex(GLenum) const;

    GLenum m_target;

    WebGLSamplerState m_samplerState;

    bool m_isWebGL2OrHigher;
    size_t m_baseLevel;
    size_t m_maxLevel;
};

} // namespace blink

#endif // WebGLTexture_h
