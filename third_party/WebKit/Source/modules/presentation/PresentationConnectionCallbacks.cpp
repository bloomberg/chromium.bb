// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationConnectionCallbacks.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationError.h"
#include "modules/presentation/PresentationRequest.h"
#include "public/platform/modules/presentation/WebPresentationError.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

PresentationConnectionCallbacks::PresentationConnectionCallbacks(
    ScriptPromiseResolver* resolver,
    PresentationRequest* request)
    : m_resolver(resolver), m_request(request) {
  ASSERT(m_resolver);
  ASSERT(m_request);
}

void PresentationConnectionCallbacks::onSuccess(
    const WebPresentationSessionInfo& sessionInfo) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->isContextDestroyed())
    return;
  m_resolver->resolve(
      PresentationConnection::take(m_resolver.get(), sessionInfo, m_request));
}

void PresentationConnectionCallbacks::onError(
    const WebPresentationError& error) {
  if (!m_resolver->getExecutionContext() ||
      m_resolver->getExecutionContext()->isContextDestroyed())
    return;
  m_resolver->reject(PresentationError::take(error));
}

}  // namespace blink
