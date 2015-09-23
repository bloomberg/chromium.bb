// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/webgl/WebGLTransformFeedback.h"

#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLTransformFeedback* WebGLTransformFeedback::create(WebGL2RenderingContextBase* ctx)
{
    return new WebGLTransformFeedback(ctx);
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_target(0)
{
    setObject(ctx->webContext()->createTransformFeedback());
}

void WebGLTransformFeedback::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteTransformFeedback(m_object);
    m_object = 0;
}

void WebGLTransformFeedback::setTarget(GLenum target)
{
    if (m_target)
        return;
    if (target == GL_TRANSFORM_FEEDBACK)
        m_target = target;
}

} // namespace blink
