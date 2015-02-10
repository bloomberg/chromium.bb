// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/WebGLSync.h"

#include "core/html/canvas/WebGL2RenderingContextBase.h"

namespace blink {

WebGLSync::~WebGLSync()
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

WebGLSync::WebGLSync(WebGL2RenderingContextBase* ctx, GLenum objectType)
    : WebGLSharedObject(ctx)
    , m_objectType(objectType)
{
}

void WebGLSync::deleteObjectImpl(blink::WebGraphicsContext3D* context3d, Platform3DObject object)
{
    context3d->deleteSync(object);
}

} // namespace blink
