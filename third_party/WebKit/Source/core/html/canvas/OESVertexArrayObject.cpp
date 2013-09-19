/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/html/canvas/OESVertexArrayObject.h"

#include "bindings/v8/ExceptionState.h"
#include "core/html/canvas/WebGLRenderingContext.h"
#include "core/html/canvas/WebGLVertexArrayObjectOES.h"
#include "core/platform/graphics/Extensions3D.h"

namespace WebCore {

OESVertexArrayObject::OESVertexArrayObject(WebGLRenderingContext* context)
    : WebGLExtension(context)
{
    ScriptWrappable::init(this);
    context->graphicsContext3D()->extensions()->ensureEnabled("GL_OES_vertex_array_object");
}

OESVertexArrayObject::~OESVertexArrayObject()
{
}

WebGLExtension::ExtensionName OESVertexArrayObject::name() const
{
    return OESVertexArrayObjectName;
}

PassRefPtr<OESVertexArrayObject> OESVertexArrayObject::create(WebGLRenderingContext* context)
{
    return adoptRef(new OESVertexArrayObject(context));
}

PassRefPtr<WebGLVertexArrayObjectOES> OESVertexArrayObject::createVertexArrayOES()
{
    if (isLost())
        return 0;

    RefPtr<WebGLVertexArrayObjectOES> o = WebGLVertexArrayObjectOES::create(m_context, WebGLVertexArrayObjectOES::VaoTypeUser);
    m_context->addContextObject(o.get());
    return o.release();
}

void OESVertexArrayObject::deleteVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (!arrayObject || isLost())
        return;

    if (!arrayObject->isDefaultObject() && arrayObject == m_context->m_boundVertexArrayObject)
        m_context->setBoundVertexArrayObject(0);

    arrayObject->deleteObject(m_context->graphicsContext3D());
}

GC3Dboolean OESVertexArrayObject::isVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (!arrayObject || isLost())
        return 0;

    if (!arrayObject->hasEverBeenBound())
        return 0;

    Extensions3D* extensions = m_context->graphicsContext3D()->extensions();
    return extensions->isVertexArrayOES(arrayObject->object());
}

void OESVertexArrayObject::bindVertexArrayOES(WebGLVertexArrayObjectOES* arrayObject)
{
    if (isLost())
        return;

    if (arrayObject && (arrayObject->isDeleted() || !arrayObject->validate(0, context()))) {
        m_context->graphicsContext3D()->synthesizeGLError(GraphicsContext3D::INVALID_OPERATION);
        return;
    }

    Extensions3D* extensions = m_context->graphicsContext3D()->extensions();
    if (arrayObject && !arrayObject->isDefaultObject() && arrayObject->object()) {
        extensions->bindVertexArrayOES(arrayObject->object());

        arrayObject->setHasEverBeenBound();
        m_context->setBoundVertexArrayObject(arrayObject);
    } else {
        extensions->bindVertexArrayOES(0);
        m_context->setBoundVertexArrayObject(0);
    }
}

bool OESVertexArrayObject::supported(WebGLRenderingContext* context)
{
    Extensions3D* extensions = context->graphicsContext3D()->extensions();
    return extensions->supports("GL_OES_vertex_array_object");
}

const char* OESVertexArrayObject::extensionName()
{
    return "OES_vertex_array_object";
}

} // namespace WebCore
