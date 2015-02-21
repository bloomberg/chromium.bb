// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/WebGLTransformFeedback.h"

#include "core/html/canvas/WebGL2RenderingContextBase.h"

namespace blink {

PassRefPtrWillBeRawPtr<WebGLTransformFeedback> WebGLTransformFeedback::create(WebGL2RenderingContextBase* ctx)
{
    return adoptRefWillBeNoop(new WebGLTransformFeedback(ctx));
}

WebGLTransformFeedback::~WebGLTransformFeedback()
{
    // Always call detach here to ensure that platform object deletion
    // happens with Oilpan enabled. It keeps the code regular to do it
    // with or without Oilpan enabled.
    //
    // See comment in WebGLBuffer's destructor for additional
    // information on why this is done for WebGLSharedObject-derived
    // objects.
    detachAndDeleteObject();
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
{
    setObject(ctx->webContext()->createTransformFeedback());
}

void WebGLTransformFeedback::deleteObjectImpl(blink::WebGraphicsContext3D* context3d)
{
    context3d->deleteTransformFeedback(m_object);
    m_object = 0;
}

} // namespace blink
