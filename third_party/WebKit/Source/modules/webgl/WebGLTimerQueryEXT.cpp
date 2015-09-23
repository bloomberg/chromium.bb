// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "modules/webgl/WebGLTimerQueryEXT.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLTimerQueryEXT* WebGLTimerQueryEXT::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLTimerQueryEXT(ctx);
}

WebGLTimerQueryEXT::~WebGLTimerQueryEXT()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

WebGLTimerQueryEXT::WebGLTimerQueryEXT(WebGLRenderingContextBase* ctx)
    : WebGLContextObject(ctx)
    , m_target(0)
    , m_queryId(0)
{
    m_queryId = context()->webContext()->createQueryEXT();
}

void WebGLTimerQueryEXT::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteQueryEXT(m_queryId);
    m_queryId = 0;
}

} // namespace blink
