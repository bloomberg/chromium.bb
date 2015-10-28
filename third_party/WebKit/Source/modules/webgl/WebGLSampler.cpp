// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/webgl/WebGLSampler.h"

#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSampler* WebGLSampler::create(WebGL2RenderingContextBase* ctx)
{
    return new WebGLSampler(ctx);
}

WebGLSampler::~WebGLSampler()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLSampler::WebGLSampler(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_compreFunc(GL_LEQUAL)
    , m_compreMode(GL_NONE)
    , m_magFilter(GL_LINEAR)
    , m_minFilter(GL_NEAREST_MIPMAP_LINEAR)
    , m_wrapR(GL_REPEAT)
    , m_wrapS(GL_REPEAT)
    , m_wrapT(GL_REPEAT)
    , m_maxLod(1000.0f)
    , m_minLod(-1000.0f)
{
    setObject(ctx->webContext()->createSampler());
}

void WebGLSampler::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteSampler(m_object);
    m_object = 0;
}

void WebGLSampler::setParameteri(GLenum pname, GLint param)
{
    ASSERT(object());

    switch (pname) {
    case GL_TEXTURE_MAX_LOD:
    case GL_TEXTURE_MIN_LOD:
        {
            GLfloat fparam = static_cast<GLfloat>(param);
            setParameterf(pname, fparam);
            return;
        }
    case GL_TEXTURE_COMPARE_FUNC:
        m_compreFunc = param;
        break;
    case GL_TEXTURE_COMPARE_MODE:
        m_compreMode = param;
        break;
    case GL_TEXTURE_MAG_FILTER:
        m_magFilter = param;
        break;
    case GL_TEXTURE_MIN_FILTER:
        m_minFilter = param;
        break;
    case GL_TEXTURE_WRAP_R:
        m_wrapR = param;
        break;
    case GL_TEXTURE_WRAP_S:
        m_wrapS = param;
        break;
    case GL_TEXTURE_WRAP_T:
        m_wrapT = param;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

void WebGLSampler::setParameterf(GLenum pname, GLfloat param)
{
    ASSERT(object());

    switch (pname) {
    case GL_TEXTURE_COMPARE_FUNC:
    case GL_TEXTURE_COMPARE_MODE:
    case GL_TEXTURE_MAG_FILTER:
    case GL_TEXTURE_MIN_FILTER:
    case GL_TEXTURE_WRAP_R:
    case GL_TEXTURE_WRAP_S:
    case GL_TEXTURE_WRAP_T:
        {
            GLint iparam = static_cast<GLint>(param);
            setParameteri(pname, iparam);
            return;
        }
    case GL_TEXTURE_MAX_LOD:
        m_maxLod = param;
        break;
    case GL_TEXTURE_MIN_LOD:
        m_minLod = param;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace blink
