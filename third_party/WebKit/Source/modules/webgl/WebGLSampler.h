// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSampler_h
#define WebGLSampler_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"
#include "modules/webgl/WebGLTexture.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSampler : public WebGLSharedPlatform3DObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLSampler() override;

    static WebGLSampler* create(WebGL2RenderingContextBase*);

    void setParameteri(GLenum pname, GLint param);
    void setParameterf(GLenum pname, GLfloat param);
    GLenum getCompareFunc() const { return m_state.compreFunc; }
    GLenum getCompareMode() const { return m_state.compreMode; }
    GLenum getMagFilter() const { return m_state.magFilter; }
    GLenum getMinFilter() const { return m_state.minFilter; }
    GLenum getWrapR() const { return m_state.wrapR; }
    GLenum getWrapS() const { return m_state.wrapS; }
    GLenum getWrapT() const { return m_state.wrapT; }
    GLfloat getMaxLod() const { return m_state.maxLod; }
    GLfloat getMinLod() const { return m_state.minLod; }

    const WebGLSamplerState* getSamplerState() const { return &m_state; }

protected:
    explicit WebGLSampler(WebGL2RenderingContextBase*);

    void deleteObjectImpl(WebGraphicsContext3D*) override;

private:
    bool isSampler() const override { return true; }

    WebGLSamplerState m_state;
};

} // namespace blink

#endif // WebGLSampler_h
