// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediastream/RTCVoidRequestPromiseImpl.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/mediastream/RTCPeerConnection.h"

namespace blink {

RTCVoidRequestPromiseImpl* RTCVoidRequestPromiseImpl::create(RTCPeerConnection* requester, ScriptPromiseResolver* resolver, ExceptionCode exceptionCode)
{
    return new RTCVoidRequestPromiseImpl(requester, resolver, exceptionCode);
}

RTCVoidRequestPromiseImpl::RTCVoidRequestPromiseImpl(RTCPeerConnection* requester, ScriptPromiseResolver* resolver, ExceptionCode exceptionCode)
    : m_requester(requester)
    , m_resolver(resolver)
    , m_exceptionCode(exceptionCode)
{
    ASSERT(m_requester);
    ASSERT(m_resolver);
}

RTCVoidRequestPromiseImpl::~RTCVoidRequestPromiseImpl()
{
}

void RTCVoidRequestPromiseImpl::requestSucceeded()
{
    if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
        m_resolver->resolve();
    } else {
        // This is needed to have the resolver release its internal resources
        // while leaving the associated promise pending as specified.
        m_resolver->detach();
    }

    clear();
}

void RTCVoidRequestPromiseImpl::requestFailed(const String& error)
{
    if (m_requester && m_requester->shouldFireDefaultCallbacks()) {
        m_resolver->reject(DOMException::create(m_exceptionCode, error));
    } else {
        // This is needed to have the resolver release its internal resources
        // while leaving the associated promise pending as specified.
        m_resolver->detach();
    }

    clear();
}

void RTCVoidRequestPromiseImpl::clear()
{
    m_requester.clear();
}

DEFINE_TRACE(RTCVoidRequestPromiseImpl)
{
    visitor->trace(m_resolver);
    visitor->trace(m_requester);
    RTCVoidRequest::trace(visitor);
}

} // namespace blink
