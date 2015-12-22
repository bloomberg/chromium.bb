// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLSampler.h"

#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSamplerState::WebGLSamplerState()
    : compreFunc(GL_LEQUAL)
    , compreMode(GL_NONE)
    , magFilter(GL_LINEAR)
    , minFilter(GL_NEAREST_MIPMAP_LINEAR)
    , wrapR(GL_REPEAT)
    , wrapS(GL_REPEAT)
    , wrapT(GL_REPEAT)
    , maxLod(1000.0f)
    , minLod(-1000.0f)
{
}

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
        m_state.compreFunc = param;
        break;
    case GL_TEXTURE_COMPARE_MODE:
        m_state.compreMode = param;
        break;
    case GL_TEXTURE_MAG_FILTER:
        m_state.magFilter = param;
        break;
    case GL_TEXTURE_MIN_FILTER:
        m_state.minFilter = param;
        break;
    case GL_TEXTURE_WRAP_R:
        m_state.wrapR = param;
        break;
    case GL_TEXTURE_WRAP_S:
        m_state.wrapS = param;
        break;
    case GL_TEXTURE_WRAP_T:
        m_state.wrapT = param;
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
        m_state.maxLod = param;
        break;
    case GL_TEXTURE_MIN_LOD:
        m_state.minLod = param;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }
}

} // namespace blink
