// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebGLSampler_h
#define WebGLSampler_h

#include "modules/webgl/WebGLSharedPlatform3DObject.h"

namespace blink {

class WebGL2RenderingContextBase;

class WebGLSampler : public WebGLSharedPlatform3DObject {
    DEFINE_WRAPPERTYPEINFO();
public:
    ~WebGLSampler() override;

    static WebGLSampler* create(WebGL2RenderingContextBase*);

    void setParameteri(GLenum pname, GLint param);
    void setParameterf(GLenum pname, GLfloat param);
    GLenum getCompareFunc() const { return m_compreFunc; }
    GLenum getCompareMode() const { return m_compreMode; }
    GLenum getMagFilter() const { return m_magFilter; }
    GLenum getMinFilter() const { return m_minFilter; }
    GLenum getWrapR() const { return m_wrapR; }
    GLenum getWrapS() const { return m_wrapS; }
    GLenum getWrapT() const { return m_wrapT; }
    GLfloat getMaxLod() const { return m_maxLod; }
    GLfloat getMinLod() const { return m_minLod; }

protected:
    explicit WebGLSampler(WebGL2RenderingContextBase*);

    void deleteObjectImpl(WebGraphicsContext3D*) override;

private:
    bool isSampler() const override { return true; }

    GLenum m_compreFunc;
    GLenum m_compreMode;
    GLenum m_magFilter;
    GLenum m_minFilter;
    GLenum m_wrapR;
    GLenum m_wrapS;
    GLenum m_wrapT;
    GLfloat m_maxLod;
    GLfloat m_minLod;
};

} // namespace blink

#endif // WebGLSampler_h
