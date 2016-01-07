// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLVertexArrayObjectBase.h"

#include "modules/webgl/WebGLRenderingContextBase.h"

namespace blink {

WebGLVertexArrayObjectBase::WebGLVertexArrayObjectBase(WebGLRenderingContextBase* ctx, VaoType type)
    : WebGLContextObject(ctx)
    , m_object(0)
    , m_type(type)
    , m_hasEverBeenBound(false)
    , m_destructionInProgress(false)
    , m_boundElementArrayBuffer(nullptr)
{
    m_arrayBufferList.resize(ctx->maxVertexAttribs());

    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        m_object = context()->webContext()->createVertexArrayOES();
        break;
    }
}

WebGLVertexArrayObjectBase::~WebGLVertexArrayObjectBase()
{
    m_destructionInProgress = true;

    // Delete the platform framebuffer resource, in case
    // where this vertex array object isn't detached when it and
    // the WebGLRenderingContextBase object it is registered with
    // are both finalized.
    detachAndDeleteObject();
}

void WebGLVertexArrayObjectBase::dispatchDetached(WebGraphicsContext3D* context3d)
{
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context3d);

    for (size_t i = 0; i < m_arrayBufferList.size(); ++i) {
        if (m_arrayBufferList[i])
            m_arrayBufferList[i]->onDetached(context3d);
    }
}

void WebGLVertexArrayObjectBase::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    switch (m_type) {
    case VaoTypeDefault:
        break;
    default:
        context3d->deleteVertexArrayOES(m_object);
        m_object = 0;
        break;
    }

    // Member<> objects must not be accessed during the destruction,
    // since they could have been already finalized.
    // The finalizers of these objects will handle their detachment
    // by themselves.
    if (!m_destructionInProgress)
        dispatchDetached(context3d);
}

void WebGLVertexArrayObjectBase::setElementArrayBuffer(WebGLBuffer* buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_boundElementArrayBuffer)
        m_boundElementArrayBuffer->onDetached(context()->webContext());
    m_boundElementArrayBuffer = buffer;
}

WebGLBuffer* WebGLVertexArrayObjectBase::getArrayBufferForAttrib(size_t index)
{
    ASSERT(index < context()->maxVertexAttribs());
    return m_arrayBufferList[index].get();
}

void WebGLVertexArrayObjectBase::setArrayBufferForAttrib(GLuint index, WebGLBuffer* buffer)
{
    if (buffer)
        buffer->onAttached();
    if (m_arrayBufferList[index])
        m_arrayBufferList[index]->onDetached(context()->webContext());

    m_arrayBufferList[index] = buffer;
}

void WebGLVertexArrayObjectBase::unbindBuffer(WebGLBuffer* buffer)
{
    if (m_boundElementArrayBuffer == buffer) {
        m_boundElementArrayBuffer->onDetached(context()->webContext());
        m_boundElementArrayBuffer = nullptr;
    }

    for (size_t i = 0; i < m_arrayBufferList.size(); ++i) {
        if (m_arrayBufferList[i] == buffer) {
            m_arrayBufferList[i]->onDetached(context()->webContext());
            m_arrayBufferList[i] = nullptr;
        }
    }
}

DEFINE_TRACE(WebGLVertexArrayObjectBase)
{
    visitor->trace(m_boundElementArrayBuffer);
    visitor->trace(m_arrayBufferList);
    WebGLContextObject::trace(visitor);
}

} // namespace blink
