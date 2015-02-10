// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/WebGLQuery.h"

#include "core/html/canvas/WebGL2RenderingContextBase.h"

namespace blink {

PassRefPtrWillBeRawPtr<WebGLQuery> WebGLQuery::create(WebGL2RenderingContextBase* ctx)
{
    return adoptRefWillBeNoop(new WebGLQuery(ctx));
}

WebGLQuery::~WebGLQuery()
{
    deleteObject(0);
}

WebGLQuery::WebGLQuery(WebGL2RenderingContextBase* ctx)
    : WebGLSharedObject(ctx)
{
    setObject(ctx->webContext()->createQueryEXT());
}

void WebGLQuery::deleteObjectImpl(blink::WebGraphicsContext3D* context3d, Platform3DObject object)
{
    context3d->deleteQueryEXT(object);
}

} // namespace blink
