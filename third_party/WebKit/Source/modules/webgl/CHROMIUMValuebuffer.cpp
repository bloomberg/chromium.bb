// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/CHROMIUMValuebuffer.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

CHROMIUMValuebuffer* CHROMIUMValuebuffer::create(WebGLRenderingContextBase* ctx)
{
    return new CHROMIUMValuebuffer(ctx);
}

CHROMIUMValuebuffer::~CHROMIUMValuebuffer()
{
    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

CHROMIUMValuebuffer::CHROMIUMValuebuffer(WebGLRenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_hasEverBeenBound(false)
{
    setObject(ctx->webContext()->createValuebufferCHROMIUM());
}

void CHROMIUMValuebuffer::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteValuebufferCHROMIUM(m_object);
    m_object = 0;
}

} // namespace blink
