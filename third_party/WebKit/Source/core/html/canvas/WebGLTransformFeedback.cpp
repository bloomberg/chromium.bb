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
    deleteObject(0);
}

WebGLTransformFeedback::WebGLTransformFeedback(WebGL2RenderingContextBase* ctx)
    : WebGLSharedObject(ctx)
{
    setObject(ctx->webContext()->createTransformFeedback());
}

void WebGLTransformFeedback::deleteObjectImpl(blink::WebGraphicsContext3D* context3d, Platform3DObject object)
{
    context3d->deleteTransformFeedback(object);
}

} // namespace blink
