// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/webgl/WebGLQuery.h"

#include "modules/webgl/WebGL2RenderingContextBase.h"

namespace blink {

WebGLQuery* WebGLQuery::create(WebGL2RenderingContextBase* ctx)
{
    return new WebGLQuery(ctx);
}

WebGLQuery::~WebGLQuery()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLQuery::WebGLQuery(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_target(0)
{
    setObject(ctx->webContext()->createQueryEXT());
}

void WebGLQuery::setTarget(GLenum target)
{
    ASSERT(object());
    ASSERT(!m_target);
    m_target = target;
}

void WebGLQuery::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteQueryEXT(m_object);
    m_object = 0;
}

} // namespace blink
