// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/WebGLSampler.h"

#include "core/html/canvas/WebGL2RenderingContextBase.h"

namespace blink {

PassRefPtrWillBeRawPtr<WebGLSampler> WebGLSampler::create(WebGL2RenderingContextBase* ctx)
{
    return adoptRefWillBeNoop(new WebGLSampler(ctx));
}

WebGLSampler::~WebGLSampler()
{
    deleteObject(0);
}

WebGLSampler::WebGLSampler(WebGL2RenderingContextBase* ctx)
    : WebGLSharedObject(ctx)
{
    setObject(ctx->webContext()->createSampler());
}

void WebGLSampler::deleteObjectImpl(blink::WebGraphicsContext3D* context3d, Platform3DObject object)
{
    context3d->deleteSampler(object);
}

} // namespace blink
