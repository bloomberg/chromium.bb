// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
{
    setObject(ctx->webContext()->createSampler());
}

void WebGLSampler::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteSampler(m_object);
    m_object = 0;
}

} // namespace blink
