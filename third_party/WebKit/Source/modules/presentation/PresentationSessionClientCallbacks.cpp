// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/PresentationSessionClientCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/presentation/Presentation.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationSession.h"
#include "public/platform/WebString.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/OwnPtr.h"

namespace blink {

PresentationSessionClientCallbacks::PresentationSessionClientCallbacks(
    PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver,
    Presentation* presentation)
    : m_resolver(resolver)
    , m_presentation(presentation)
{
    ASSERT(m_resolver);
    ASSERT(m_presentation);
}

PresentationSessionClientCallbacks::~PresentationSessionClientCallbacks()
{
}

void PresentationSessionClientCallbacks::onSuccess(WebPresentationSessionClient* client)
{
    ASSERT(client);
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        PresentationSession::dispose(client);
        return;
    }

    m_resolver->resolve(PresentationSession::take(client, m_presentation));
}

void PresentationSessionClientCallbacks::onError(WebPresentationError* error)
{
    ASSERT(error);
    if (!m_resolver->executionContext() || m_resolver->executionContext()->activeDOMObjectsAreStopped()) {
        PresentationError::dispose(error);
        return;
    }

    m_resolver->reject(PresentationError::take(error));
}

} // namespace blink
