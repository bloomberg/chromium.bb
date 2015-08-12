// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSessionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "modules/presentation/PresentationSession.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "public/platform/modules/presentation/WebPresentationSessionClient.h"

namespace blink {

PresentationSessionCallbacks::PresentationSessionCallbacks(ScriptPromiseResolver* resolver, PresentationRequest* request)
    : m_resolver(resolver)
    , m_request(request)
{
    ASSERT(m_resolver);
    ASSERT(m_request);
}

void PresentationSessionCallbacks::onSuccess(WebPassOwnPtr<WebPresentationSessionClient> presentationSessionClient)
{
    OwnPtr<WebPresentationSessionClient> result(presentationSessionClient.release());

    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->resolve(PresentationSession::take(m_resolver.get(), result.release(), m_request));
}

void PresentationSessionCallbacks::onError(const WebPresentationError& error)
{
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped())
        return;
    m_resolver->reject(PresentationError::take(m_resolver.get(), error));
}

} // namespace blink
