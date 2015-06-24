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
    // Always call detach here to ensure that platform object deletion
    // happens with Oilpan enabled. It keeps the code regular to do it
    // with or without Oilpan enabled.
    //
    // See comment in WebGLBuffer's destructor for additional
    // information on why this is done for WebGLSharedObject-derived
    // objects.
    detachAndDeleteObject();
}

WebGLQuery::WebGLQuery(WebGL2RenderingContextBase* ctx)
    : WebGLSharedPlatform3DObject(ctx)
    , m_target(0)
{
    setObject(ctx->webContext()->createQueryEXT());
}

void WebGLQuery::setTarget(GLenum target)
{
    ASSERT(object());
    ASSERT(isCompatibleTarget(target));
    m_target = target;
}

bool WebGLQuery::isCompatibleTarget(GLenum target)
{
    if (!m_target)
        return true;

    switch (target) {
    // The SAMPLES_PASSED targets are interchangable.
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
        return m_target == GL_ANY_SAMPLES_PASSED || m_target == GL_ANY_SAMPLES_PASSED_CONSERVATIVE;
    // Otherwise target is finalized the first time beginQuery() is called.
    default:
        return m_target == target;
    }
}

void WebGLQuery::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    context3d->deleteQueryEXT(m_object);
    m_object = 0;
}

} // namespace blink
