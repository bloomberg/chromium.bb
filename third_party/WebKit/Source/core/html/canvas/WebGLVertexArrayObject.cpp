// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/WebGLVertexArrayObject.h"

#include "core/html/canvas/WebGLRenderingContextBase.h"

namespace blink {

PassRefPtrWillBeRawPtr<WebGLVertexArrayObject> WebGLVertexArrayObject::create(WebGLRenderingContextBase* ctx, VaoType type)
{
    return adoptRefWillBeNoop(new WebGLVertexArrayObject(ctx, type));
}

WebGLVertexArrayObject::WebGLVertexArrayObject(WebGLRenderingContextBase* ctx, VaoType type)
    : WebGLVertexArrayObjectBase(ctx, type)
{
}

} // namespace blink
