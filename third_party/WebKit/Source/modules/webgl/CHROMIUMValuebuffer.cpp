// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/CHROMIUMValuebuffer.h"

#include "gpu/command_buffer/client/gles2_interface.h"
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
    GLuint buffer;
    ctx->contextGL()->GenValuebuffersCHROMIUM(1, &buffer);
    setObject(buffer);
}

void CHROMIUMValuebuffer::deleteObjectImpl(WebGraphicsContext3D* context3d, gpu::gles2::GLES2Interface* gl)
{
    gl->DeleteValuebuffersCHROMIUM(1, &m_object);
    m_object = 0;
}

} // namespace blink
